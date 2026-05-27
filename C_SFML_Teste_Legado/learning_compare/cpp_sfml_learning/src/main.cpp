#include <SFML/Graphics.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr float TAU = PI * 2.0f;
constexpr int WINDOW_W = 1320;
constexpr int WINDOW_H = 780;
constexpr int PANEL_W = 340;
constexpr int SIM_W = WINDOW_W - PANEL_W;
constexpr float CX = SIM_W * 0.5f;
constexpr float CY = WINDOW_H * 0.52f;
constexpr float FIXED_DT = 1.0f / 30.0f;
constexpr float VISION_FOV = 220.0f * PI / 180.0f;
const std::string CONFIG_FILE = "learning_compare_sfml_config.txt";

double nowSeconds() {
    using clock = std::chrono::high_resolution_clock;
    static const auto t0 = clock::now();
    return std::chrono::duration<double>(clock::now() - t0).count();
}

float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

float wrapPi(float x) {
    while (x > PI) x -= TAU;
    while (x < -PI) x += TAU;
    return x;
}

float sigmoid(float x) {
    x = clampf(x, -16.0f, 16.0f);
    return 1.0f / (1.0f + std::exp(-x));
}

struct Args {
    bool benchmark = true;
    bool visual = false;
    bool showVision = false;
    int agents = 600;
    int foods = 300;
    int bins = 16;
    int steps = 1200;
    int seed = 123;
    int stepsPerFrame = 1;
    int predators = 20;
    bool useNeat = false;
    bool channelD = true;
    bool channelR = true;
    bool channelG = true;
    bool channelB = false;
    std::string output;
};

Args parseArgs(int argc, char** argv) {
    Args args;
    if (argc <= 1) {
        args.visual = true;
        args.benchmark = false;
    }
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : ""; };
        if (a == "--visual") { args.visual = true; args.benchmark = false; }
        else if (a == "--benchmark") { args.benchmark = true; args.visual = false; }
        else if (a == "--show-vision") args.showVision = true;
        else if (a == "--agents") args.agents = std::stoi(next());
        else if (a == "--foods") args.foods = std::stoi(next());
        else if (a == "--bins") args.bins = std::stoi(next());
        else if (a == "--steps") args.steps = std::stoi(next());
        else if (a == "--seed") args.seed = std::stoi(next());
        else if (a == "--steps-per-frame") args.stepsPerFrame = std::stoi(next());
        else if (a == "--predators") args.predators = std::stoi(next());
        else if (a == "--neat") args.useNeat = true;
        else if (a == "--channels") {
            std::string channels = next();
            args.channelD = channels.find('D') != std::string::npos || channels.find('d') != std::string::npos;
            args.channelR = channels.find('R') != std::string::npos || channels.find('r') != std::string::npos;
            args.channelG = channels.find('G') != std::string::npos || channels.find('g') != std::string::npos;
            args.channelB = channels.find('B') != std::string::npos || channels.find('b') != std::string::npos;
        }
        else if (a == "--output") args.output = next();
    }
    args.agents = std::max(1, args.agents);
    args.foods = std::max(1, args.foods);
    args.bins = std::max(2, args.bins);
    args.steps = std::max(1, args.steps);
    args.stepsPerFrame = std::max(1, args.stepsPerFrame);
    args.predators = std::max(0, args.predators);
    if (!args.channelD && !args.channelR && !args.channelG && !args.channelB) args.channelD = true;
    return args;
}

struct Profile {
    double vision = 0.0;
    double brain = 0.0;
    double physics = 0.0;
    double interaction = 0.0;
    double collision = 0.0;
    double evolution = 0.0;
    double render = 0.0;
};

struct ScopedTimer {
    double& target;
    double t0;
    explicit ScopedTimer(double& dst) : target(dst), t0(nowSeconds()) {}
    ~ScopedTimer() { target += nowSeconds() - t0; }
};

struct SimParams {
    float timeScale = 1.0f;
    float substrateRadius = 330.0f;
    float targetFood = 300.0f;
    float foodRegenPerSecond = 80.0f;
    float foodEnergy = 18.0f;
    float maxAgents = 5000.0f;
    float minAgents = 100.0f;
    float startAgents = 600.0f;
    float startPredators = 20.0f;
    float minPredators = 0.0f;
    float maxPredators = 200.0f;
    float agentRadius = 5.5f;
    float predatorRadius = 7.5f;
    float foodRadius = 3.2f;
    float visionRadius = 145.0f;
    float maxSpeed = 78.0f;
    float turnRate = 3.4f;
    float idleCost = 0.016f;
    float moveCost = 0.00035f;
    float vmaxCost = 0.06f;
    float reproductionEnergy = 95.0f;
    float reproductionCooldown = 1.2f;
    float mutationRate = 0.08f;
    float mutationStrength = 0.18f;
    float neatAddConnectionRate = 0.08f;
    float neatAddNodeRate = 0.03f;
    float neatMaxHidden = 64.0f;
    float neatMaxConnections = 512.0f;
    float retinas = 16.0f;
    float distanceBands = 4.0f;
    float hiddenLayers = 1.0f;
    float neuronsPerLayer = 12.0f;
    float preyR = 82.0f;
    float preyG = 230.0f;
    float preyB = 155.0f;
    float predatorR = 235.0f;
    float predatorG = 76.0f;
    float predatorB = 78.0f;
    float foodR = 246.0f;
    float foodG = 190.0f;
    float foodB = 72.0f;
    bool useNeat = false;
    bool channelD = true;
    bool channelR = true;
    bool channelG = true;
    bool channelB = false;
    bool collision = true;
    bool showVision = false;
    bool paused = false;
    bool fullscreen = false;
    float zoom = 1.0f;
};

enum SliderId {
    S_TIME_SCALE,
    S_ZOOM,
    S_SUBSTRATE_RADIUS,
    S_START_AGENTS,
    S_MIN_AGENTS,
    S_MAX_AGENTS,
    S_START_PREDATORS,
    S_MIN_PREDATORS,
    S_MAX_PREDATORS,
    S_TARGET_FOOD,
    S_FOOD_REGEN,
    S_FOOD_ENERGY,
    S_AGENT_RADIUS,
    S_PREDATOR_RADIUS,
    S_FOOD_RADIUS,
    S_VISION_RADIUS,
    S_RETINAS,
    S_DISTANCE_BANDS,
    S_HIDDEN_LAYERS,
    S_NEURONS,
    S_MAX_SPEED,
    S_TURN_RATE,
    S_IDLE_COST,
    S_MOVE_COST,
    S_VMAX_COST,
    S_REPRO_ENERGY,
    S_REPRO_COOLDOWN,
    S_MUTATION_RATE,
    S_MUTATION_STRENGTH,
    S_NEAT_ADD_CONN,
    S_NEAT_ADD_NODE,
    S_NEAT_MAX_HIDDEN,
    S_NEAT_MAX_CONN,
    S_PREY_R,
    S_PREY_G,
    S_PREY_B,
    S_PRED_R,
    S_PRED_G,
    S_PRED_B,
    S_FOOD_R,
    S_FOOD_G,
    S_FOOD_B,
    S_COUNT
};

enum ToggleId {
    T_USE_NEAT,
    T_CHANNEL_D,
    T_CHANNEL_R,
    T_CHANNEL_G,
    T_CHANNEL_B,
    T_COLLISION,
    T_SHOW_VISION,
    T_PAUSED,
    T_FULLSCREEN,
    T_COUNT
};

struct Slider {
    std::string label;
    float minV = 0.0f;
    float maxV = 1.0f;
    float value = 0.0f;
    bool integer = false;
    sf::FloatRect rect;
    bool active = false;
    std::string edit;

    float norm() const {
        return (value - minV) / std::max(0.0001f, maxV - minV);
    }

    void clampValue() {
        value = clampf(value, minV, maxV);
        if (integer) value = std::round(value);
    }

    void syncEdit() {
        edit = valueText();
    }

    void applyEdit() {
        try {
            if (!edit.empty() && edit != "-" && edit != "." && edit != "-.") {
                value = std::stof(edit);
                clampValue();
            }
        } catch (...) {
            clampValue();
        }
        syncEdit();
    }

    bool handle(const sf::Event& event, sf::Vector2f mouse) {
        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            bool hit = rect.contains(mouse);
            if (!hit && active) applyEdit();
            active = hit;
            if (active && edit.empty()) syncEdit();
            return hit;
        }
        if (!active) return false;
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Enter || event.key.code == sf::Keyboard::Return) {
                applyEdit();
                active = false;
                return true;
            }
            if (event.key.code == sf::Keyboard::Escape) {
                syncEdit();
                active = false;
                return true;
            }
            if (event.key.code == sf::Keyboard::BackSpace) {
                if (!edit.empty()) edit.pop_back();
                return true;
            }
        }
        if (event.type == sf::Event::TextEntered) {
            sf::Uint32 c = event.text.unicode;
            if ((c >= '0' && c <= '9') || c == '.' || c == '-') {
                edit.push_back(static_cast<char>(c));
                return true;
            }
            if (c == 13) {
                applyEdit();
                active = false;
                return true;
            }
            return true;
        }
        return active;
    }

    std::string valueText() const {
        std::ostringstream ss;
        if (integer || maxV - minV > 100.0f) ss << std::fixed << std::setprecision(0) << value;
        else if (maxV - minV <= 1.0f) ss << std::fixed << std::setprecision(3) << value;
        else ss << std::fixed << std::setprecision(2) << value;
        return ss.str();
    }
};

struct Toggle {
    std::string label;
    bool value = false;
    sf::FloatRect rect;

    bool handle(const sf::Event& event, sf::Vector2f mouse) {
        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left && rect.contains(mouse)) {
            value = !value;
            return true;
        }
        return false;
    }
};

struct Button {
    std::string label;
    sf::FloatRect rect;

    bool clicked(const sf::Event& event, sf::Vector2f mouse) const {
        return event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left && rect.contains(mouse);
    }
};

class ControlPanel {
public:
    ControlPanel(const Args& args) {
        sliders.resize(S_COUNT);
        toggles.resize(T_COUNT);
        auto setS = [&](int id, const std::string& label, float lo, float hi, float v, bool integer = false) {
            sliders[id] = Slider{label, lo, hi, v, integer};
            sliders[id].clampValue();
            sliders[id].syncEdit();
        };
        setS(S_TIME_SCALE, "Aceleracao tempo", 0.1f, 80.0f, 1.0f);
        setS(S_ZOOM, "Zoom visual", 0.25f, 6.0f, 1.0f);
        setS(S_SUBSTRATE_RADIUS, "Tamanho substrato", 120.0f, 370.0f, 330.0f);
        setS(S_START_AGENTS, "Organismos reset", 10.0f, 10000.0f, float(args.agents), true);
        setS(S_MIN_AGENTS, "Min organismos", 0.0f, 5000.0f, std::min(100.0f, float(args.agents)), true);
        setS(S_MAX_AGENTS, "Max organismos", 50.0f, 50000.0f, 5000.0f, true);
        setS(S_START_PREDATORS, "Predadores reset", 0.0f, 5000.0f, float(args.predators), true);
        setS(S_MIN_PREDATORS, "Min predadores", 0.0f, 2000.0f, 0.0f, true);
        setS(S_MAX_PREDATORS, "Max predadores", 0.0f, 10000.0f, 200.0f, true);
        setS(S_TARGET_FOOD, "Target comida", 0.0f, 20000.0f, float(args.foods), true);
        setS(S_FOOD_REGEN, "Reposicao comida/s", 0.0f, 5000.0f, 80.0f);
        setS(S_FOOD_ENERGY, "Energia comida", 1.0f, 120.0f, 18.0f);
        setS(S_AGENT_RADIUS, "Raio organismo", 2.0f, 12.0f, 5.5f);
        setS(S_PREDATOR_RADIUS, "Raio predador", 2.0f, 18.0f, 7.5f);
        setS(S_FOOD_RADIUS, "Raio comida", 1.0f, 9.0f, 3.2f);
        setS(S_VISION_RADIUS, "Raio visao", 30.0f, 360.0f, 145.0f);
        setS(S_RETINAS, "Retinas / bins", 4.0f, 96.0f, float(args.bins), true);
        setS(S_DISTANCE_BANDS, "Subdivisoes distancia", 1.0f, 32.0f, 4.0f, true);
        setS(S_HIDDEN_LAYERS, "Camadas RN", 1.0f, 5.0f, 1.0f, true);
        setS(S_NEURONS, "Neuronios/camada", 4.0f, 96.0f, 12.0f, true);
        setS(S_MAX_SPEED, "Velocidade max", 10.0f, 200.0f, 78.0f);
        setS(S_TURN_RATE, "Giro max", 0.2f, 10.0f, 3.4f);
        setS(S_IDLE_COST, "Custo parado", 0.0f, 0.2f, 0.016f);
        setS(S_MOVE_COST, "Custo movimento", 0.0f, 0.005f, 0.00035f);
        setS(S_VMAX_COST, "Custo em Vmax", 0.0f, 1.0f, 0.06f);
        setS(S_REPRO_ENERGY, "Energia reproducao", 10.0f, 250.0f, 95.0f);
        setS(S_REPRO_COOLDOWN, "Cooldown reproducao", 0.0f, 10.0f, 1.2f);
        setS(S_MUTATION_RATE, "Taxa mutacao", 0.0f, 0.5f, 0.08f);
        setS(S_MUTATION_STRENGTH, "Forca mutacao", 0.0f, 1.0f, 0.18f);
        setS(S_NEAT_ADD_CONN, "NEAT add conexao", 0.0f, 0.5f, 0.08f);
        setS(S_NEAT_ADD_NODE, "NEAT add neuronio", 0.0f, 0.5f, 0.03f);
        setS(S_NEAT_MAX_HIDDEN, "NEAT max ocultos", 0.0f, 512.0f, 64.0f, true);
        setS(S_NEAT_MAX_CONN, "NEAT max conexoes", 2.0f, 4096.0f, 512.0f, true);
        setS(S_PREY_R, "Cor presa R", 0.0f, 255.0f, 82.0f, true);
        setS(S_PREY_G, "Cor presa G", 0.0f, 255.0f, 230.0f, true);
        setS(S_PREY_B, "Cor presa B", 0.0f, 255.0f, 155.0f, true);
        setS(S_PRED_R, "Cor predador R", 0.0f, 255.0f, 235.0f, true);
        setS(S_PRED_G, "Cor predador G", 0.0f, 255.0f, 76.0f, true);
        setS(S_PRED_B, "Cor predador B", 0.0f, 255.0f, 78.0f, true);
        setS(S_FOOD_R, "Cor comida R", 0.0f, 255.0f, 246.0f, true);
        setS(S_FOOD_G, "Cor comida G", 0.0f, 255.0f, 190.0f, true);
        setS(S_FOOD_B, "Cor comida B", 0.0f, 255.0f, 72.0f, true);
        toggles[T_USE_NEAT] = Toggle{"Usar NEAT comum", args.useNeat};
        toggles[T_CHANNEL_D] = Toggle{"Canal D", args.channelD};
        toggles[T_CHANNEL_R] = Toggle{"Canal R", args.channelR};
        toggles[T_CHANNEL_G] = Toggle{"Canal G", args.channelG};
        toggles[T_CHANNEL_B] = Toggle{"Canal B", args.channelB};
        toggles[T_COLLISION] = Toggle{"Colisao organismos", true};
        toggles[T_SHOW_VISION] = Toggle{"Mostrar visao", args.showVision};
        toggles[T_PAUSED] = Toggle{"Pausar simulacao", false};
        toggles[T_FULLSCREEN] = Toggle{"Tela cheia F11", false};
        loadConfig(CONFIG_FILE);
        layout();
    }

    void loadConfig(const std::string& path) {
        std::ifstream in(path);
        if (!in) return;
        char kind = 0;
        int id = 0;
        float value = 0.0f;
        while (in >> kind >> id >> value) {
            if (kind == 'S' && id >= 0 && id < int(sliders.size())) {
                sliders[std::size_t(id)].value = value;
                sliders[std::size_t(id)].clampValue();
                sliders[std::size_t(id)].syncEdit();
            } else if (kind == 'T' && id >= 0 && id < int(toggles.size())) {
                toggles[std::size_t(id)].value = value >= 0.5f;
            }
        }
    }

    void saveConfig(const std::string& path) const {
        std::ofstream out(path, std::ios::binary);
        if (!out) return;
        out << std::fixed << std::setprecision(6);
        for (int i = 0; i < int(sliders.size()); ++i) out << "S " << i << " " << sliders[std::size_t(i)].value << "\n";
        for (int i = 0; i < int(toggles.size()); ++i) out << "T " << i << " " << (toggles[std::size_t(i)].value ? 1 : 0) << "\n";
    }

    SimParams params() const {
        SimParams p;
        p.timeScale = sliders[S_TIME_SCALE].value;
        p.zoom = sliders[S_ZOOM].value;
        p.substrateRadius = sliders[S_SUBSTRATE_RADIUS].value;
        p.startAgents = sliders[S_START_AGENTS].value;
        p.minAgents = sliders[S_MIN_AGENTS].value;
        p.maxAgents = sliders[S_MAX_AGENTS].value;
        p.startPredators = sliders[S_START_PREDATORS].value;
        p.minPredators = sliders[S_MIN_PREDATORS].value;
        p.maxPredators = sliders[S_MAX_PREDATORS].value;
        p.targetFood = sliders[S_TARGET_FOOD].value;
        p.foodRegenPerSecond = sliders[S_FOOD_REGEN].value;
        p.foodEnergy = sliders[S_FOOD_ENERGY].value;
        p.agentRadius = sliders[S_AGENT_RADIUS].value;
        p.predatorRadius = sliders[S_PREDATOR_RADIUS].value;
        p.foodRadius = sliders[S_FOOD_RADIUS].value;
        p.visionRadius = sliders[S_VISION_RADIUS].value;
        p.retinas = sliders[S_RETINAS].value;
        p.distanceBands = sliders[S_DISTANCE_BANDS].value;
        p.hiddenLayers = sliders[S_HIDDEN_LAYERS].value;
        p.neuronsPerLayer = sliders[S_NEURONS].value;
        p.maxSpeed = sliders[S_MAX_SPEED].value;
        p.turnRate = sliders[S_TURN_RATE].value;
        p.idleCost = sliders[S_IDLE_COST].value;
        p.moveCost = sliders[S_MOVE_COST].value;
        p.vmaxCost = sliders[S_VMAX_COST].value;
        p.reproductionEnergy = sliders[S_REPRO_ENERGY].value;
        p.reproductionCooldown = sliders[S_REPRO_COOLDOWN].value;
        p.mutationRate = sliders[S_MUTATION_RATE].value;
        p.mutationStrength = sliders[S_MUTATION_STRENGTH].value;
        p.neatAddConnectionRate = sliders[S_NEAT_ADD_CONN].value;
        p.neatAddNodeRate = sliders[S_NEAT_ADD_NODE].value;
        p.neatMaxHidden = sliders[S_NEAT_MAX_HIDDEN].value;
        p.neatMaxConnections = sliders[S_NEAT_MAX_CONN].value;
        p.preyR = sliders[S_PREY_R].value;
        p.preyG = sliders[S_PREY_G].value;
        p.preyB = sliders[S_PREY_B].value;
        p.predatorR = sliders[S_PRED_R].value;
        p.predatorG = sliders[S_PRED_G].value;
        p.predatorB = sliders[S_PRED_B].value;
        p.foodR = sliders[S_FOOD_R].value;
        p.foodG = sliders[S_FOOD_G].value;
        p.foodB = sliders[S_FOOD_B].value;
        p.useNeat = toggles[T_USE_NEAT].value;
        p.channelD = toggles[T_CHANNEL_D].value;
        p.channelR = toggles[T_CHANNEL_R].value;
        p.channelG = toggles[T_CHANNEL_G].value;
        p.channelB = toggles[T_CHANNEL_B].value;
        p.collision = toggles[T_COLLISION].value;
        p.showVision = toggles[T_SHOW_VISION].value;
        p.paused = toggles[T_PAUSED].value;
        p.fullscreen = toggles[T_FULLSCREEN].value;
        return p;
    }

    void setPaused(bool paused) {
        toggles[T_PAUSED].value = paused;
    }

    void setFullscreen(bool fullscreen) {
        toggles[T_FULLSCREEN].value = fullscreen;
    }

    void toggleVision() {
        toggles[T_SHOW_VISION].value = !toggles[T_SHOW_VISION].value;
    }

    void requestResetNow() {
        requestReset = true;
    }

    void multiplyZoom(float factor) {
        Slider& z = sliders[S_ZOOM];
        z.value = clampf(z.value * factor, z.minV, z.maxV);
        z.syncEdit();
    }

    bool handle(const sf::Event& event, sf::Vector2f mouse) {
        if (event.type == sf::Event::MouseWheelScrolled && mouse.x >= SIM_W) {
            scroll -= event.mouseWheelScroll.delta * 42.0f;
            clampScroll();
            layout();
            return true;
        }
        bool used = false;
        for (auto& s : sliders) used = s.handle(event, mouse) || used;
        for (auto& t : toggles) used = t.handle(event, mouse) || used;
        if (resetButton.clicked(event, mouse)) {
            requestReset = true;
            used = true;
        }
        if (foodButton.clicked(event, mouse)) {
            requestFoodFill = true;
            used = true;
        }
        return used;
    }

    void layout() {
        float x = float(SIM_W) + 32.0f;
        float y = 126.0f - scroll;
        resetButton.rect = {x, y, 126.0f, 30.0f};
        foodButton.rect = {x + 138.0f, y, 126.0f, 30.0f};
        y += 72.0f;
        for (auto& t : toggles) {
            t.rect = {x, y, 46.0f, 23.0f};
            y += 39.0f;
        }
        y += 12.0f;
        for (auto& s : sliders) {
            s.rect = {x + 152.0f, y, 112.0f, 28.0f};
            y += 39.0f;
        }
        contentHeight = y + scroll + 40.0f;
        clampScroll();
    }

    void draw(sf::RenderTarget& target, const sf::Font& font, int agents, int predators, int foods, double simTime,
              double realStepsPerSecond, const Profile& profile, int generation, int totalEats, int totalPredations,
              float bestEats, float avgEats) {
        sf::RectangleShape panel({float(PANEL_W), float(WINDOW_H)});
        panel.setPosition(float(SIM_W), 0.0f);
        panel.setFillColor(sf::Color(12, 18, 25, 244));
        target.draw(panel);

        sf::RectangleShape edge({2.0f, float(WINDOW_H)});
        edge.setPosition(float(SIM_W), 0.0f);
        edge.setFillColor(sf::Color(84, 198, 240, 130));
        target.draw(edge);

        drawText(target, font, "C++/SFML Learning Lab", SIM_W + 26.0f, 22.0f, 22, sf::Color(238, 248, 255));
        double profileTotal = std::max(
            1e-9,
            profile.vision + profile.brain + profile.physics + profile.interaction +
            profile.collision + profile.evolution + profile.render
        );
        auto pct = [&](double value) -> int {
            return int(std::round(100.0 * value / profileTotal));
        };
        std::ostringstream stats;
        stats << "presas " << agents << "  pred " << predators << "  comida " << foods << "\n"
              << "tempo " << std::fixed << std::setprecision(1) << simTime << "s"
              << "  steps/s " << std::setprecision(0) << realStepsPerSecond << "\n"
              << "gen " << generation << "  eats " << totalEats << "  hunt " << totalPredations
              << "  best " << std::setprecision(0) << bestEats
              << "  avg " << std::setprecision(1) << avgEats << "\n"
              << "vision " << pct(profile.vision) << "%  RN " << pct(profile.brain) << "%"
              << "  render " << pct(profile.render) << "%";
        drawText(target, font, stats.str(), SIM_W + 26.0f, 54.0f, 14, sf::Color(155, 183, 204));

        drawButton(target, font, resetButton);
        drawButton(target, font, foodButton);
        for (const auto& t : toggles) drawToggle(target, font, t);
        for (const auto& s : sliders) drawSlider(target, font, s);

        if (contentHeight > WINDOW_H) {
            float trackH = float(WINDOW_H) - 18.0f;
            float ratio = float(WINDOW_H) / std::max(float(WINDOW_H), contentHeight);
            float thumbH = std::max(42.0f, trackH * ratio);
            float maxScroll = std::max(1.0f, contentHeight - float(WINDOW_H));
            float thumbY = 9.0f + (trackH - thumbH) * (scroll / maxScroll);
            sf::RectangleShape track({4.0f, trackH});
            track.setPosition(WINDOW_W - 12.0f, 9.0f);
            track.setFillColor(sf::Color(35, 47, 59));
            target.draw(track);
            sf::RectangleShape thumb({4.0f, thumbH});
            thumb.setPosition(WINDOW_W - 12.0f, thumbY);
            thumb.setFillColor(sf::Color(97, 207, 245));
            target.draw(thumb);
        }
    }

    bool consumeResetRequest() {
        bool r = requestReset;
        requestReset = false;
        return r;
    }

    bool consumeFoodFillRequest() {
        bool r = requestFoodFill;
        requestFoodFill = false;
        return r;
    }

private:
    std::vector<Slider> sliders;
    std::vector<Toggle> toggles;
    Button resetButton{"Resetar", {}};
    Button foodButton{"Encher comida", {}};
    float scroll = 0.0f;
    float contentHeight = 0.0f;
    bool requestReset = false;
    bool requestFoodFill = false;

    void clampScroll() {
        scroll = clampf(scroll, 0.0f, std::max(0.0f, contentHeight - float(WINDOW_H) + 14.0f));
    }

    void drawText(sf::RenderTarget& target, const sf::Font& font, const std::string& text,
                  float x, float y, unsigned size, sf::Color color) const {
        sf::Text t(text, font, size);
        t.setFillColor(color);
        t.setPosition(x, y);
        target.draw(t);
    }

    std::string fmt(float value, bool integer, float span) const {
        std::ostringstream ss;
        if (integer || span > 100.0f) ss << std::fixed << std::setprecision(0) << value;
        else if (span <= 1.0f) ss << std::fixed << std::setprecision(3) << value;
        else ss << std::fixed << std::setprecision(2) << value;
        return ss.str();
    }

    void drawSlider(sf::RenderTarget& target, const sf::Font& font, const Slider& s) const {
        if (s.rect.top < -42.0f || s.rect.top > WINDOW_H + 24.0f) return;
        drawText(target, font, s.label, s.rect.left - 152.0f, s.rect.top + 5.0f, 13, sf::Color(220, 232, 245));
        sf::RectangleShape box({s.rect.width, s.rect.height});
        box.setPosition(s.rect.left, s.rect.top);
        box.setFillColor(sf::Color(25, 35, 47));
        box.setOutlineThickness(1.4f);
        box.setOutlineColor(s.active ? sf::Color(120, 226, 255) : sf::Color(60, 78, 96));
        target.draw(box);
        std::string text = s.active ? s.edit : s.valueText();
        drawText(target, font, text, s.rect.left + 8.0f, s.rect.top + 5.0f, 13, sf::Color(235, 248, 255));
    }

    void drawToggle(sf::RenderTarget& target, const sf::Font& font, const Toggle& t) const {
        if (t.rect.top < -32.0f || t.rect.top > WINDOW_H + 16.0f) return;
        sf::RectangleShape box({t.rect.width, t.rect.height});
        box.setPosition(t.rect.left, t.rect.top);
        box.setFillColor(t.value ? sf::Color(56, 148, 210) : sf::Color(30, 41, 54));
        box.setOutlineThickness(1.0f);
        box.setOutlineColor(t.value ? sf::Color(130, 225, 255) : sf::Color(71, 83, 96));
        target.draw(box);
        sf::CircleShape indicator(8.0f, 24);
        indicator.setOrigin(8.0f, 8.0f);
        indicator.setPosition(t.rect.left + (t.value ? t.rect.width - 14.0f : 14.0f), t.rect.top + t.rect.height * 0.5f);
        indicator.setFillColor(t.value ? sf::Color(240, 252, 255) : sf::Color(115, 126, 137));
        target.draw(indicator);
        drawText(target, font, t.label, t.rect.left + t.rect.width + 11.0f, t.rect.top + 2.0f, 14, sf::Color(220, 232, 245));
    }

    void drawButton(sf::RenderTarget& target, const sf::Font& font, const Button& b) const {
        if (b.rect.top < -38.0f || b.rect.top > WINDOW_H + 16.0f) return;
        sf::RectangleShape box({b.rect.width, b.rect.height});
        box.setPosition(b.rect.left, b.rect.top);
        box.setFillColor(sf::Color(32, 75, 98));
        box.setOutlineThickness(1.0f);
        box.setOutlineColor(sf::Color(100, 210, 245));
        target.draw(box);
        drawText(target, font, b.label, b.rect.left + 14.0f, b.rect.top + 6.0f, 14, sf::Color(235, 248, 255));
    }
};

struct NeatConnection {
    int src = 0;
    int dst = 0;
    float weight = 0.0f;
    bool enabled = true;
};

struct Brain {
    bool neat = false;
    std::vector<int> sizes;
    std::vector<std::vector<float>> weights;
    std::vector<std::vector<float>> biases;
    int neatInputs = 0;
    int neatOutputs = 2;
    int neatHidden = 0;
    std::vector<float> nodeLayer;
    std::vector<NeatConnection> conns;

    void randomize(const std::vector<int>& layerSizes, std::mt19937& rng) {
        neat = false;
        sizes = layerSizes;
        weights.clear();
        biases.clear();
        nodeLayer.clear();
        conns.clear();
        std::normal_distribution<float> wdist(0.0f, 0.65f);
        std::normal_distribution<float> bdist(0.0f, 0.12f);
        for (std::size_t l = 0; l + 1 < sizes.size(); ++l) {
            weights.emplace_back(std::size_t(sizes[l]) * std::size_t(sizes[l + 1]));
            biases.emplace_back(std::size_t(sizes[l + 1]));
            for (float& w : weights.back()) w = wdist(rng);
            for (float& b : biases.back()) b = bdist(rng);
        }
    }

    void randomizeNeat(int inputs, int maxConns, std::mt19937& rng) {
        neat = true;
        sizes.clear();
        weights.clear();
        biases.clear();
        conns.clear();
        neatInputs = std::max(1, inputs);
        neatOutputs = 2;
        neatHidden = 0;
        nodeLayer.assign(std::size_t(neatInputs + neatOutputs), 0.0f);
        for (int i = 0; i < neatInputs; ++i) nodeLayer[std::size_t(i)] = 0.0f;
        for (int o = 0; o < neatOutputs; ++o) nodeLayer[std::size_t(neatInputs + o)] = 1.0f;
        std::normal_distribution<float> wdist(0.0f, 0.65f);
        std::vector<std::pair<int, int>> candidates;
        candidates.reserve(std::size_t(neatInputs * neatOutputs));
        for (int i = 0; i < neatInputs; ++i) {
            for (int o = 0; o < neatOutputs; ++o) candidates.push_back({i, neatInputs + o});
        }
        std::shuffle(candidates.begin(), candidates.end(), rng);
        int initialConns = std::max(neatOutputs, std::min(int(candidates.size()), maxConns));
        conns.reserve(std::size_t(initialConns));
        for (int i = 0; i < initialConns; ++i) {
            conns.push_back({candidates[std::size_t(i)].first, candidates[std::size_t(i)].second, wdist(rng), true});
        }
    }

    void mutate(float rate, float strength, float addConnRate, float addNodeRate,
                int maxHidden, int maxConns, std::mt19937& rng) {
        std::bernoulli_distribution md(clampf(rate, 0.0f, 1.0f));
        std::normal_distribution<float> nd(0.0f, std::max(0.0f, strength));
        if (neat) {
            for (auto& c : conns) if (md(rng)) c.weight += nd(rng);
            std::uniform_real_distribution<float> unit(0.0f, 1.0f);
            if (unit(rng) < addConnRate && int(conns.size()) < maxConns) addConnection(rng);
            if (unit(rng) < addNodeRate && neatHidden < maxHidden && int(conns.size()) + 2 < maxConns) addNode(rng);
            return;
        }
        for (auto& layer : weights) {
            for (float& w : layer) if (md(rng)) w += nd(rng);
        }
        for (auto& layer : biases) {
            for (float& b : layer) if (md(rng)) b += nd(rng);
        }
    }

    void forward(const std::vector<float>& input, std::vector<float>& scratchA,
                 std::vector<float>& scratchB, float& turnOut, float& speedOut) const {
        if (neat) {
            int totalNodes = int(nodeLayer.size());
            scratchA.assign(std::size_t(totalNodes), 0.0f);
            int nIn = std::min(neatInputs, int(input.size()));
            for (int i = 0; i < nIn; ++i) scratchA[std::size_t(i)] = input[std::size_t(i)];
            std::vector<int> order((std::size_t(totalNodes)));
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(), [&](int a, int b) {
                return nodeLayer[std::size_t(a)] < nodeLayer[std::size_t(b)];
            });
            for (int node : order) {
                if (node >= neatInputs && node < neatInputs + neatHidden) {
                    scratchA[std::size_t(node)] = std::tanh(scratchA[std::size_t(node)]);
                }
                float v = scratchA[std::size_t(node)];
                for (const auto& c : conns) {
                    if (c.enabled && c.src == node) {
                        scratchA[std::size_t(c.dst)] += v * c.weight;
                    }
                }
            }
            turnOut = scratchA[std::size_t(neatInputs + neatHidden)];
            speedOut = scratchA[std::size_t(neatInputs + neatHidden + 1)];
            return;
        }
        const std::vector<float>* cur = &input;
        std::vector<float>* next = &scratchA;
        for (std::size_t l = 0; l < weights.size(); ++l) {
            int inN = sizes[l];
            int outN = sizes[l + 1];
            next->assign(std::size_t(outN), 0.0f);
            for (int o = 0; o < outN; ++o) {
                float sum = biases[l][o];
                for (int i = 0; i < inN; ++i) {
                    sum += (*cur)[i] * weights[l][std::size_t(i) * std::size_t(outN) + std::size_t(o)];
                }
                (*next)[o] = (l + 1 == weights.size()) ? sum : std::tanh(sum);
            }
            cur = next;
            next = (next == &scratchA) ? &scratchB : &scratchA;
        }
        turnOut = cur->empty() ? 0.0f : (*cur)[0];
        speedOut = cur->size() < 2 ? 0.0f : (*cur)[1];
    }

private:
    bool connectionExists(int src, int dst) const {
        for (const auto& c : conns) if (c.src == src && c.dst == dst) return true;
        return false;
    }

    void addConnection(std::mt19937& rng) {
        if (nodeLayer.size() < 2) return;
        std::uniform_int_distribution<int> pick(0, int(nodeLayer.size()) - 1);
        std::normal_distribution<float> wdist(0.0f, 0.65f);
        for (int tries = 0; tries < 32; ++tries) {
            int src = pick(rng);
            int dst = pick(rng);
            if (src == dst) continue;
            if (nodeLayer[std::size_t(src)] >= nodeLayer[std::size_t(dst)]) continue;
            if (src >= neatInputs + neatHidden) continue;
            if (dst < neatInputs) continue;
            if (connectionExists(src, dst)) continue;
            conns.push_back({src, dst, wdist(rng), true});
            return;
        }
    }

    void addNode(std::mt19937& rng) {
        std::vector<int> candidates;
        for (int i = 0; i < int(conns.size()); ++i) if (conns[std::size_t(i)].enabled) candidates.push_back(i);
        if (candidates.empty()) return;
        std::uniform_int_distribution<int> pick(0, int(candidates.size()) - 1);
        int idx = candidates[std::size_t(pick(rng))];
        NeatConnection old = conns[std::size_t(idx)];
        conns[std::size_t(idx)].enabled = false;
        int newId = neatInputs + neatHidden;
        int oldOut0 = neatInputs + neatHidden;
        int oldOut1 = oldOut0 + 1;
        nodeLayer.insert(nodeLayer.begin() + newId, (nodeLayer[std::size_t(old.src)] + nodeLayer[std::size_t(old.dst)]) * 0.5f);
        ++neatHidden;
        for (auto& c : conns) {
            if (c.src >= newId) ++c.src;
            if (c.dst >= newId) ++c.dst;
        }
        conns.push_back({old.src, newId, 1.0f, true});
        int shiftedDst = old.dst >= newId ? old.dst + 1 : old.dst;
        conns.push_back({newId, shiftedDst, old.weight, true});
        (void)oldOut0;
        (void)oldOut1;
    }
};

struct Agent {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float angle = 0.0f;
    float energy = 55.0f;
    float age = 0.0f;
    float cooldown = 0.0f;
    float eats = 0.0f;
    bool predator = false;
    Brain brain;
    std::vector<float> input;
    std::vector<float> scratchA;
    std::vector<float> scratchB;
};

struct Food {
    float x = 0.0f;
    float y = 0.0f;
};

class Sim {
public:
    explicit Sim(const Args& args)
        : rng(static_cast<unsigned int>(args.seed)) {
        params.retinas = float(args.bins);
        params.targetFood = float(args.foods);
        params.startAgents = float(args.agents);
        params.startPredators = float(args.predators);
        params.useNeat = args.useNeat;
        params.channelD = args.channelD;
        params.channelR = args.channelR;
        params.channelG = args.channelG;
        params.channelB = args.channelB;
        reset(params);
    }

    void reset(const SimParams& p) {
        params = p;
        profile = Profile{};
        steps = 0;
        generation = 0;
        totalEats = 0;
        totalPredations = 0;
        simulatedSeconds = 0.0;
        foodAccumulator = 0.0f;
        lastInputSize = inputSize(p);
        lastHiddenLayers = hiddenLayers(p);
        lastNeurons = neuronsPerLayer(p);
        agents.clear();
        predators.clear();
        foods.clear();
        int maxStartAgents = std::max(1, int(std::round(p.maxAgents)));
        int start = std::min(maxStartAgents, std::max(1, int(std::round(p.startAgents))));
        agents.reserve(std::min(start * 2, maxStartAgents));
        for (int i = 0; i < start; ++i) agents.push_back(makeRandomAgent(p));
        int maxStartPredators = std::max(0, int(std::round(p.maxPredators)));
        int startPred = std::min(maxStartPredators, std::max(0, int(std::round(p.startPredators))));
        predators.reserve(std::min(startPred * 2 + 16, std::max(1, maxStartPredators)));
        for (int i = 0; i < startPred; ++i) predators.push_back(makeRandomPredator(p));
        addFood(std::max(0, int(std::round(p.targetFood))), p);
    }

    void fillFood(const SimParams& p) {
        if (foods.size() < std::size_t(std::max(0, int(std::round(p.targetFood))))) {
            addFood(int(std::round(p.targetFood)) - int(foods.size()), p);
        }
    }

    void step(const SimParams& p, float dt) {
        params = p;
        syncArchitectureIfNeeded(p);
        { ScopedTimer t(profile.vision); buildFoodGrid(p); sense(p); }
        { ScopedTimer t(profile.brain); brain(p); }
        { ScopedTimer t(profile.physics); physics(p, dt); }
        { ScopedTimer t(profile.collision); if (p.collision) collideAgents(p); }
        { ScopedTimer t(profile.interaction); eatFood(p); predatorHunt(p); replenishFood(p, dt); }
        { ScopedTimer t(profile.evolution); reproduceAndDie(p); }
        simulatedSeconds += dt;
        ++steps;
    }

    void render(sf::RenderTarget& target, const sf::Font* font, const SimParams& p) {
        ScopedTimer t(profile.render);
        target.clear(sf::Color(5, 9, 14));
        drawDish(target, p);
        if (p.showVision) drawVision(target, p);
        drawFood(target, p);
        drawAgents(target, p);
        if (font != nullptr) {
            std::ostringstream ss;
            ss << "org " << agents.size() << " | comida " << foods.size()
               << " | tempo " << std::fixed << std::setprecision(1) << simulatedSeconds
               << "s | eats " << totalEats << " | pred " << predators.size();
            sf::Text text(ss.str(), *font, 18);
            text.setFillColor(sf::Color(225, 242, 252));
            text.setPosition(16.0f, 14.0f);
            target.draw(text);
        }
    }

    int agentCount() const { return int(agents.size()); }
    int predatorCount() const { return int(predators.size()); }
    int foodCount() const { return int(foods.size()); }
    double simTime() const { return simulatedSeconds; }
    int generation = 0;

    float bestEats() const {
        float best = 0.0f;
        for (const auto& a : agents) best = std::max(best, a.eats);
        return best;
    }

    float avgEats() const {
        if (agents.empty()) return 0.0f;
        float total = 0.0f;
        for (const auto& a : agents) total += a.eats;
        return total / float(agents.size());
    }

    Profile profile;
    int steps = 0;
    int totalEats = 0;
    int totalPredations = 0;

private:
    std::mt19937 rng;
    SimParams params;
    std::vector<Agent> agents;
    std::vector<Agent> predators;
    std::vector<Food> foods;
    std::vector<std::vector<int>> foodGrid;
    int foodCols = 1;
    int foodRows = 1;
    float foodCell = 50.0f;
    float foodAccumulator = 0.0f;
    int lastInputSize = 0;
    int lastHiddenLayers = 1;
    int lastNeurons = 12;
    double simulatedSeconds = 0.0;

    int retinas(const SimParams& p) const { return std::max(1, int(std::round(p.retinas))); }
    int distanceBands(const SimParams& p) const { return std::max(1, int(std::round(p.distanceBands))); }
    int activeChannels(const SimParams& p) const {
        int c = 0;
        if (p.channelD) ++c;
        if (p.channelR) ++c;
        if (p.channelG) ++c;
        if (p.channelB) ++c;
        return std::max(1, c);
    }
    int inputSize(const SimParams& p) const { return retinas(p) * activeChannels(p); }
    int hiddenLayers(const SimParams& p) const { return std::max(1, int(std::round(p.hiddenLayers))); }
    int neuronsPerLayer(const SimParams& p) const { return std::max(1, int(std::round(p.neuronsPerLayer))); }
    float radiusFor(const Agent& a, const SimParams& p) const { return a.predator ? p.predatorRadius : p.agentRadius; }

    std::vector<int> layerSizes(const SimParams& p) const {
        std::vector<int> sizes;
        sizes.push_back(inputSize(p));
        for (int i = 0; i < hiddenLayers(p); ++i) sizes.push_back(neuronsPerLayer(p));
        sizes.push_back(2);
        return sizes;
    }

    float randf(float lo, float hi) {
        std::uniform_real_distribution<float> d(lo, hi);
        return d(rng);
    }

    void randomPoint(float margin, const SimParams& p, float& x, float& y) {
        float a = randf(0.0f, TAU);
        float r = std::sqrt(randf(0.0f, 1.0f)) * std::max(1.0f, p.substrateRadius - margin);
        x = CX + std::cos(a) * r;
        y = CY + std::sin(a) * r;
    }

    void initBrain(Agent& a, const SimParams& p) {
        if (p.useNeat) a.brain.randomizeNeat(inputSize(p), int(std::round(p.neatMaxConnections)), rng);
        else a.brain.randomize(layerSizes(p), rng);
        a.input.assign(std::size_t(inputSize(p)), 0.0f);
        a.scratchA.clear();
        a.scratchB.clear();
    }

    Agent makeRandomAgent(const SimParams& p) {
        Agent a;
        randomPoint(20.0f, p, a.x, a.y);
        a.angle = randf(0.0f, TAU);
        a.vx = std::cos(a.angle) * randf(1.0f, 18.0f);
        a.vy = std::sin(a.angle) * randf(1.0f, 18.0f);
        a.energy = 55.0f;
        a.predator = false;
        initBrain(a, p);
        return a;
    }

    Agent makeRandomPredator(const SimParams& p) {
        Agent a;
        randomPoint(20.0f, p, a.x, a.y);
        a.angle = randf(0.0f, TAU);
        a.vx = std::cos(a.angle) * randf(1.0f, 18.0f);
        a.vy = std::sin(a.angle) * randf(1.0f, 18.0f);
        a.energy = 80.0f;
        a.predator = true;
        initBrain(a, p);
        return a;
    }

    Agent makeChild(const Agent& parent, const SimParams& p) {
        Agent c = parent;
        c.brain.mutate(p.mutationRate, p.mutationStrength, p.neatAddConnectionRate, p.neatAddNodeRate,
                       int(std::round(p.neatMaxHidden)), int(std::round(p.neatMaxConnections)), rng);
        c.energy = std::max(12.0f, parent.energy * 0.42f);
        c.age = 0.0f;
        c.cooldown = p.reproductionCooldown;
        c.eats = 0.0f;
        float r = radiusFor(c, p);
        c.x += randf(-r * 2.0f, r * 2.0f);
        c.y += randf(-r * 2.0f, r * 2.0f);
        keepInside(c, p);
        return c;
    }

    void addFood(int count, const SimParams& p) {
        if (count <= 0) return;
        foods.reserve(foods.size() + std::size_t(count));
        for (int i = 0; i < count; ++i) {
            Food f;
            randomPoint(12.0f, p, f.x, f.y);
            foods.push_back(f);
        }
    }

    void syncArchitectureIfNeeded(const SimParams& p) {
        int in = inputSize(p);
        int layers = hiddenLayers(p);
        int neurons = neuronsPerLayer(p);
        bool modeChanged = (!agents.empty() && (agents.front().brain.neat != p.useNeat)) ||
                           (!predators.empty() && (predators.front().brain.neat != p.useNeat));
        if (in == lastInputSize && layers == lastHiddenLayers && neurons == lastNeurons && !modeChanged) return;
        lastInputSize = in;
        lastHiddenLayers = layers;
        lastNeurons = neurons;
        for (auto& a : agents) {
            initBrain(a, p);
        }
        for (auto& a : predators) {
            initBrain(a, p);
        }
    }

    sf::Color preyColor(const SimParams& p) const {
        return sf::Color(sf::Uint8(clampf(p.preyR, 0, 255)), sf::Uint8(clampf(p.preyG, 0, 255)), sf::Uint8(clampf(p.preyB, 0, 255)));
    }
    sf::Color predatorColor(const SimParams& p) const {
        return sf::Color(sf::Uint8(clampf(p.predatorR, 0, 255)), sf::Uint8(clampf(p.predatorG, 0, 255)), sf::Uint8(clampf(p.predatorB, 0, 255)));
    }
    sf::Color foodColor(const SimParams& p) const {
        return sf::Color(sf::Uint8(clampf(p.foodR, 0, 255)), sf::Uint8(clampf(p.foodG, 0, 255)), sf::Uint8(clampf(p.foodB, 0, 255)));
    }

    float distanceActivation(float dist, const SimParams& p) const {
        float norm = clampf(dist / std::max(1e-6f, p.visionRadius), 0.0f, 1.0f);
        int subdivisions = distanceBands(p);
        float representative = norm;
        if (subdivisions > 1) {
            int band = int(std::floor(std::sqrt(norm) * float(subdivisions)));
            band = std::max(0, std::min(subdivisions - 1, band));
            float left = std::pow(float(band) / float(subdivisions), 2.0f);
            float right = std::pow(float(band + 1) / float(subdivisions), 2.0f);
            representative = (left + right) * 0.5f;
        }
        return clampf(1.0f - representative, 0.0f, 1.0f);
    }

    void addSignal(Agent& viewer, const SimParams& p, float tx, float ty, sf::Color color) {
        int bins = retinas(p);
        int channels = activeChannels(p);
        float half = VISION_FOV * 0.5f;
        float dx = tx - viewer.x;
        float dy = ty - viewer.y;
        float d2 = dx * dx + dy * dy;
        if (d2 >= p.visionRadius * p.visionRadius) return;
        float rel = wrapPi(std::atan2(dy, dx) - viewer.angle);
        if (std::abs(rel) > half) return;
        float dist = std::sqrt(std::max(d2, 1e-6f));
        int b = std::max(0, std::min(bins - 1, int(std::floor(((rel + half) / VISION_FOV) * float(bins)))));
        float intensity = distanceActivation(dist, p);
        int ch = 0;
        auto put = [&](float value) {
            float& cell = viewer.input[std::size_t(b * channels + ch)];
            if (value > cell) cell = value;
            ++ch;
        };
        if (p.channelD) put(intensity);
        if (p.channelR) put(intensity * float(color.r) / 255.0f);
        if (p.channelG) put(intensity * float(color.g) / 255.0f);
        if (p.channelB) put(intensity * float(color.b) / 255.0f);
        if (ch == 0) viewer.input[std::size_t(b)] = intensity;
    }

    void buildFoodGrid(const SimParams& p) {
        foodCell = std::max(24.0f, p.visionRadius * 0.5f);
        foodCols = std::max(1, int(std::ceil(float(SIM_W) / foodCell)));
        foodRows = std::max(1, int(std::ceil(float(WINDOW_H) / foodCell)));
        foodGrid.assign(std::size_t(foodCols * foodRows), {});
        for (int i = 0; i < int(foods.size()); ++i) {
            int cx = std::max(0, std::min(foodCols - 1, int(foods[i].x / foodCell)));
            int cy = std::max(0, std::min(foodRows - 1, int(foods[i].y / foodCell)));
            foodGrid[std::size_t(cy * foodCols + cx)].push_back(i);
        }
    }

    void sense(const SimParams& p) {
        const sf::Color foodC = foodColor(p);
        const sf::Color predatorC = predatorColor(p);
        const sf::Color preyC = preyColor(p);
        int radiusCells = std::max(1, int(std::ceil(p.visionRadius / foodCell)));
        for (auto& a : agents) {
            a.input.assign(std::size_t(inputSize(p)), 0.0f);
            int cx = std::max(0, std::min(foodCols - 1, int(a.x / foodCell)));
            int cy = std::max(0, std::min(foodRows - 1, int(a.y / foodCell)));
            for (int gy = std::max(0, cy - radiusCells); gy <= std::min(foodRows - 1, cy + radiusCells); ++gy) {
                for (int gx = std::max(0, cx - radiusCells); gx <= std::min(foodCols - 1, cx + radiusCells); ++gx) {
                    for (int idx : foodGrid[std::size_t(gy * foodCols + gx)]) {
                        const Food& f = foods[std::size_t(idx)];
                        addSignal(a, p, f.x, f.y, foodC);
                    }
                }
            }
            for (const auto& pred : predators) addSignal(a, p, pred.x, pred.y, predatorC);
        }
        for (auto& pred : predators) {
            pred.input.assign(std::size_t(inputSize(p)), 0.0f);
            for (const auto& prey : agents) addSignal(pred, p, prey.x, prey.y, preyC);
        }
    }

    void brain(const SimParams& p) {
        for (auto& a : agents) updateBrainFor(a, p, 1.0f);
        for (auto& a : predators) updateBrainFor(a, p, 1.15f);
    }

    void updateBrainFor(Agent& a, const SimParams& p, float speedMultiplier) {
        float turn = 0.0f;
        float throttle = 0.0f;
        a.brain.forward(a.input, a.scratchA, a.scratchB, turn, throttle);
        float desiredTurn = std::tanh(turn) * p.turnRate;
        float desiredSpeed = sigmoid(throttle) * p.maxSpeed * speedMultiplier;
        a.angle += desiredTurn * FIXED_DT;
        float tx = std::cos(a.angle) * desiredSpeed;
        float ty = std::sin(a.angle) * desiredSpeed;
        a.vx += (tx - a.vx) * 0.18f;
        a.vy += (ty - a.vy) * 0.18f;
    }

    void physics(const SimParams& p, float dt) {
        for (auto& a : agents) integrateAgent(a, p, dt, 1.0f);
        for (auto& a : predators) integrateAgent(a, p, dt, 1.25f);
    }

    void integrateAgent(Agent& a, const SimParams& p, float dt, float costMultiplier) {
        float speed = std::sqrt(a.vx * a.vx + a.vy * a.vy);
        a.x += a.vx * dt;
        a.y += a.vy * dt;
        float speedRatio = clampf(speed / std::max(1.0f, p.maxSpeed), 0.0f, 1.5f);
        a.energy -= (p.idleCost + p.moveCost * speed + p.vmaxCost * speedRatio * speedRatio) * dt * costMultiplier;
        a.age += dt;
        a.cooldown = std::max(0.0f, a.cooldown - dt);
        keepInside(a, p);
    }

    void keepInside(Agent& a, const SimParams& p) {
        float dx = a.x - CX;
        float dy = a.y - CY;
        float dist = std::sqrt(dx * dx + dy * dy);
        float limit = p.substrateRadius - radiusFor(a, p);
        if (dist > limit) {
            float inv = 1.0f / std::max(dist, 1e-6f);
            float nx = dx * inv;
            float ny = dy * inv;
            a.x = CX + nx * limit;
            a.y = CY + ny * limit;
            float vn = a.vx * nx + a.vy * ny;
            if (vn > 0.0f) {
                a.vx -= 1.65f * vn * nx;
                a.vy -= 1.65f * vn * ny;
            }
            a.angle = std::atan2(a.vy, a.vx);
        }
    }

    void resolveCollision(Agent& a, Agent& b, const SimParams& p) {
        float minDist = radiusFor(a, p) + radiusFor(b, p);
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float d2 = dx * dx + dy * dy;
        float min2 = minDist * minDist;
        if (d2 <= 1e-6f || d2 >= min2) return;
        float dist = std::sqrt(d2);
        float nx = dx / dist;
        float ny = dy / dist;
        float overlap = minDist - dist;
        a.x -= nx * overlap * 0.5f;
        a.y -= ny * overlap * 0.5f;
        b.x += nx * overlap * 0.5f;
        b.y += ny * overlap * 0.5f;
        float rvx = b.vx - a.vx;
        float rvy = b.vy - a.vy;
        float vn = rvx * nx + rvy * ny;
        if (vn < 0.0f) {
            float impulse = -vn * 0.18f;
            a.vx -= nx * impulse;
            a.vy -= ny * impulse;
            b.vx += nx * impulse;
            b.vy += ny * impulse;
        }
        keepInside(a, p);
        keepInside(b, p);
    }

    void collideAgents(const SimParams& p) {
        float cell = std::max(8.0f, std::max(p.agentRadius, p.predatorRadius) * 2.4f);
        int cols = std::max(1, int(std::ceil(float(SIM_W) / cell)));
        int rows = std::max(1, int(std::ceil(float(WINDOW_H) / cell)));
        std::vector<std::vector<int>> grid(std::size_t(cols * rows));
        for (int i = 0; i < int(agents.size()); ++i) {
            int gx = std::max(0, std::min(cols - 1, int(agents[std::size_t(i)].x / cell)));
            int gy = std::max(0, std::min(rows - 1, int(agents[std::size_t(i)].y / cell)));
            grid[std::size_t(gy * cols + gx)].push_back(i);
        }
        float minDist = p.agentRadius * 2.0f;
        float min2 = minDist * minDist;
        for (int gy = 0; gy < rows; ++gy) {
            for (int gx = 0; gx < cols; ++gx) {
                for (int i : grid[std::size_t(gy * cols + gx)]) {
                    for (int yy = std::max(0, gy - 1); yy <= std::min(rows - 1, gy + 1); ++yy) {
                        for (int xx = std::max(0, gx - 1); xx <= std::min(cols - 1, gx + 1); ++xx) {
                            for (int j : grid[std::size_t(yy * cols + xx)]) {
                                if (j <= i) continue;
                                resolveCollision(agents[std::size_t(i)], agents[std::size_t(j)], p);
                            }
                        }
                    }
                }
            }
        }
        for (std::size_t i = 0; i < predators.size(); ++i) {
            for (std::size_t j = i + 1; j < predators.size(); ++j) resolveCollision(predators[i], predators[j], p);
        }
        for (auto& prey : agents) {
            for (auto& pred : predators) resolveCollision(prey, pred, p);
        }
    }

    void eatFood(const SimParams& p) {
        if (foods.empty() || agents.empty()) return;
        std::vector<char> eaten(foods.size(), 0);
        float eatDist = p.agentRadius + p.foodRadius;
        float eat2 = eatDist * eatDist;
        int radiusCells = std::max(1, int(std::ceil(eatDist / foodCell)));
        for (auto& a : agents) {
            int cx = std::max(0, std::min(foodCols - 1, int(a.x / foodCell)));
            int cy = std::max(0, std::min(foodRows - 1, int(a.y / foodCell)));
            bool ate = false;
            for (int gy = std::max(0, cy - radiusCells); gy <= std::min(foodRows - 1, cy + radiusCells) && !ate; ++gy) {
                for (int gx = std::max(0, cx - radiusCells); gx <= std::min(foodCols - 1, cx + radiusCells) && !ate; ++gx) {
                    for (int idx : foodGrid[std::size_t(gy * foodCols + gx)]) {
                        if (idx < 0 || idx >= int(foods.size()) || eaten[std::size_t(idx)]) continue;
                        const Food& f = foods[std::size_t(idx)];
                        float dx = f.x - a.x;
                        float dy = f.y - a.y;
                        if (dx * dx + dy * dy < eat2) {
                            eaten[std::size_t(idx)] = 1;
                            a.energy += p.foodEnergy;
                            a.eats += 1.0f;
                            ++totalEats;
                            ate = true;
                            break;
                        }
                    }
                }
            }
        }
        if (std::any_of(eaten.begin(), eaten.end(), [](char v) { return v != 0; })) {
            std::vector<Food> kept;
            kept.reserve(foods.size());
            for (std::size_t i = 0; i < foods.size(); ++i) if (!eaten[i]) kept.push_back(foods[i]);
            foods.swap(kept);
        }
    }

    void predatorHunt(const SimParams& p) {
        if (predators.empty() || agents.empty()) return;
        std::vector<char> eaten(agents.size(), 0);
        float huntDist = p.predatorRadius + p.agentRadius;
        float hunt2 = huntDist * huntDist;
        for (auto& pred : predators) {
            int best = -1;
            float bestD2 = hunt2;
            for (int i = 0; i < int(agents.size()); ++i) {
                if (eaten[std::size_t(i)]) continue;
                float dx = agents[std::size_t(i)].x - pred.x;
                float dy = agents[std::size_t(i)].y - pred.y;
                float d2 = dx * dx + dy * dy;
                if (d2 < bestD2) {
                    bestD2 = d2;
                    best = i;
                }
            }
            if (best >= 0) {
                eaten[std::size_t(best)] = 1;
                pred.energy += p.foodEnergy * 3.2f;
                pred.eats += 1.0f;
                ++totalPredations;
            }
        }
        if (std::any_of(eaten.begin(), eaten.end(), [](char v) { return v != 0; })) {
            std::vector<Agent> kept;
            kept.reserve(agents.size());
            for (std::size_t i = 0; i < agents.size(); ++i) if (!eaten[i]) kept.push_back(std::move(agents[i]));
            agents.swap(kept);
        }
    }

    void replenishFood(const SimParams& p, float dt) {
        int target = std::max(0, int(std::round(p.targetFood)));
        if (int(foods.size()) > target) {
            foods.resize(std::size_t(target));
            return;
        }
        foodAccumulator += p.foodRegenPerSecond * dt;
        int add = int(foodAccumulator);
        if (add <= 0) return;
        foodAccumulator -= float(add);
        addFood(std::min(add, target - int(foods.size())), p);
    }

    void reproduceAndDie(const SimParams& p) {
        int maxAgents = std::max(1, int(std::round(p.maxAgents)));
        std::vector<Agent> births;
        births.reserve(std::min(128, maxAgents));
        for (auto& a : agents) {
            if (int(agents.size() + births.size()) >= maxAgents) break;
            if (a.energy >= p.reproductionEnergy && a.cooldown <= 0.0f) {
                float parentEnergy = a.energy;
                Agent c = makeChild(a, p);
                a.energy = parentEnergy * 0.55f;
                a.cooldown = p.reproductionCooldown;
                births.push_back(std::move(c));
            }
        }
        for (auto& b : births) agents.push_back(std::move(b));

        int maxPredators = std::max(0, int(std::round(p.maxPredators)));
        std::vector<Agent> predatorBirths;
        predatorBirths.reserve(std::min(64, std::max(1, maxPredators)));
        for (auto& a : predators) {
            if (int(predators.size() + predatorBirths.size()) >= maxPredators) break;
            if (a.energy >= p.reproductionEnergy && a.cooldown <= 0.0f) {
                float parentEnergy = a.energy;
                Agent c = makeChild(a, p);
                a.energy = parentEnergy * 0.55f;
                a.cooldown = p.reproductionCooldown;
                predatorBirths.push_back(std::move(c));
            }
        }
        for (auto& b : predatorBirths) predators.push_back(std::move(b));

        agents.erase(std::remove_if(agents.begin(), agents.end(), [](const Agent& a) {
            return a.energy <= 0.0f;
        }), agents.end());
        predators.erase(std::remove_if(predators.begin(), predators.end(), [](const Agent& a) {
            return a.energy <= 0.0f;
        }), predators.end());

        int minAgents = std::max(0, int(std::round(p.minAgents)));
        minAgents = std::min(minAgents, maxAgents);
        while (int(agents.size()) < minAgents) {
            if (!agents.empty()) {
                std::size_t best = 0;
                for (std::size_t i = 1; i < agents.size(); ++i)
                    if (agents[i].eats > agents[best].eats) best = i;
                agents.push_back(makeChild(agents[best], p));
            } else {
                agents.push_back(makeRandomAgent(p));
            }
        }
        int minPredators = std::max(0, int(std::round(p.minPredators)));
        minPredators = std::min(minPredators, maxPredators);
        while (int(predators.size()) < minPredators) {
            if (!predators.empty()) {
                std::size_t best = 0;
                for (std::size_t i = 1; i < predators.size(); ++i)
                    if (predators[i].eats > predators[best].eats) best = i;
                Agent child = makeChild(predators[best], p);
                child.energy = 80.0f;
                predators.push_back(std::move(child));
            } else {
                predators.push_back(makeRandomPredator(p));
            }
        }
    }

    void drawDish(sf::RenderTarget& target, const SimParams& p) {
        sf::CircleShape dish(p.substrateRadius, 192);
        dish.setOrigin(p.substrateRadius, p.substrateRadius);
        dish.setPosition(CX, CY);
        dish.setFillColor(sf::Color(12, 24, 31));
        dish.setOutlineThickness(4.0f);
        dish.setOutlineColor(sf::Color(90, 210, 235, 180));
        target.draw(dish);
        sf::CircleShape inner(std::max(1.0f, p.substrateRadius - 16.0f), 192);
        inner.setOrigin(inner.getRadius(), inner.getRadius());
        inner.setPosition(CX - 14.0f, CY - 14.0f);
        inner.setFillColor(sf::Color::Transparent);
        inner.setOutlineThickness(1.5f);
        inner.setOutlineColor(sf::Color(220, 255, 255, 38));
        target.draw(inner);
    }

    void drawVision(sf::RenderTarget& target, const SimParams& p) {
        if (agents.empty()) return;
        int stride = std::max(1, int(agents.size()) / 90);
        float half = VISION_FOV * 0.5f;
        for (int i = 0; i < int(agents.size()); i += stride) {
            const Agent& a = agents[std::size_t(i)];
            sf::VertexArray fan(sf::TriangleFan, std::size_t(retinas(p) + 2));
            fan[0].position = {a.x, a.y};
            fan[0].color = sf::Color(70, 180, 255, 20);
            for (std::size_t k = 1; k < fan.getVertexCount(); ++k) {
                float u = float(k - 1) / float(fan.getVertexCount() - 2);
                float ang = a.angle - half + VISION_FOV * u;
                fan[k].position = {a.x + std::cos(ang) * p.visionRadius, a.y + std::sin(ang) * p.visionRadius};
                fan[k].color = sf::Color(70, 180, 255, 0);
            }
            target.draw(fan);
        }
    }

    void drawFood(sf::RenderTarget& target, const SimParams& p) {
        sf::CircleShape fshape(p.foodRadius, 18);
        fshape.setOrigin(p.foodRadius, p.foodRadius);
        fshape.setFillColor(foodColor(p));
        for (const auto& f : foods) {
            fshape.setPosition(f.x, f.y);
            target.draw(fshape);
        }
    }

    void drawAgents(sf::RenderTarget& target, const SimParams& p) {
        auto drawOne = [&](const Agent& a, sf::Color color) {
            float r = radiusFor(a, p);
            sf::CircleShape body(r, 24);
            body.setOrigin(r, r);
            body.setFillColor(color);
            body.setPosition(a.x, a.y);
            target.draw(body);
            sf::CircleShape eye(std::max(1.4f, r * 0.25f), 12);
            eye.setOrigin(eye.getRadius(), eye.getRadius());
            eye.setFillColor(sf::Color(4, 15, 12));
            eye.setPosition(a.x + std::cos(a.angle) * r, a.y + std::sin(a.angle) * r);
            target.draw(eye);
        };
        sf::Color preyC = preyColor(p);
        sf::Color predC = predatorColor(p);
        for (const auto& a : agents) drawOne(a, preyC);
        for (const auto& a : predators) drawOne(a, predC);
    }
};

bool loadFont(sf::Font& font) {
    const std::vector<std::string> candidates = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf"
    };
    for (const auto& path : candidates) if (font.loadFromFile(path)) return true;
    return false;
}

std::string jsonResult(const Args& args, const Sim& sim, double wall) {
    auto pct = [&](double v) { return 100.0 * v / std::max(1e-9, wall); };
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{\n";
    ss << "  \"engine\": \"cpp_sfml_interactive\",\n";
    ss << "  \"agents\": " << sim.agentCount() << ",\n";
    ss << "  \"predators\": " << sim.predatorCount() << ",\n";
    ss << "  \"foods\": " << sim.foodCount() << ",\n";
    ss << "  \"steps\": " << sim.steps << ",\n";
    ss << "  \"simulated_seconds\": " << sim.simTime() << ",\n";
    ss << "  \"wall_seconds\": " << wall << ",\n";
    ss << "  \"steps_per_wall_second\": " << sim.steps / std::max(1e-9, wall) << ",\n";
    ss << "  \"total_eats\": " << sim.totalEats << ",\n";
    ss << "  \"total_predations\": " << sim.totalPredations << ",\n";
    ss << "  \"profile_seconds\": {\n";
    ss << "    \"vision\": " << sim.profile.vision << ",\n";
    ss << "    \"brain\": " << sim.profile.brain << ",\n";
    ss << "    \"physics\": " << sim.profile.physics << ",\n";
    ss << "    \"collision\": " << sim.profile.collision << ",\n";
    ss << "    \"interaction\": " << sim.profile.interaction << ",\n";
    ss << "    \"evolution\": " << sim.profile.evolution << ",\n";
    ss << "    \"render\": " << sim.profile.render << "\n";
    ss << "  },\n";
    ss << "  \"profile_percent_wall\": {\n";
    ss << "    \"vision\": " << pct(sim.profile.vision) << ",\n";
    ss << "    \"brain\": " << pct(sim.profile.brain) << ",\n";
    ss << "    \"physics\": " << pct(sim.profile.physics) << ",\n";
    ss << "    \"collision\": " << pct(sim.profile.collision) << ",\n";
    ss << "    \"interaction\": " << pct(sim.profile.interaction) << ",\n";
    ss << "    \"evolution\": " << pct(sim.profile.evolution) << ",\n";
    ss << "    \"render\": " << pct(sim.profile.render) << "\n";
    ss << "  },\n";
    ss << "  \"config\": {\"agents\": " << args.agents << ", \"predators\": " << args.predators
       << ", \"foods\": " << args.foods << ", \"bins\": " << args.bins
       << ", \"steps\": " << args.steps << ", \"seed\": " << args.seed
       << ", \"neat\": " << (args.useNeat ? "true" : "false")
       << ", \"channels\": \""
       << (args.channelD ? "D" : "") << (args.channelR ? "R" : "")
       << (args.channelG ? "G" : "") << (args.channelB ? "B" : "") << "\"}\n";
    ss << "}\n";
    return ss.str();
}

void runBenchmark(const Args& args) {
    sf::RenderTexture texture;
    texture.create(SIM_W, WINDOW_H);
    sf::Font font;
    bool hasFont = loadFont(font);
    Sim sim(args);
    SimParams p;
    p.retinas = float(args.bins);
    p.targetFood = float(args.foods);
    p.startAgents = float(args.agents);
    p.startPredators = float(args.predators);
    p.useNeat = args.useNeat;
    p.channelD = args.channelD;
    p.channelR = args.channelR;
    p.channelG = args.channelG;
    p.channelB = args.channelB;
    sim.reset(p);
    double t0 = nowSeconds();
    for (int i = 0; i < args.steps; ++i) {
        sim.step(p, FIXED_DT);
        sim.render(texture, hasFont ? &font : nullptr, p);
        texture.display();
    }
    double wall = nowSeconds() - t0;
    std::string json = jsonResult(args, sim, wall);
    if (!args.output.empty()) {
        std::ofstream out(args.output, std::ios::binary);
        out << json;
    }
    std::cout << json;
}

void runVisual(const Args& args) {
    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;
    sf::RenderWindow window;
    bool fullscreen = false;
    auto recreateWindow = [&](bool full) {
        fullscreen = full;
        sf::VideoMode mode = full ? sf::VideoMode::getDesktopMode() : sf::VideoMode(WINDOW_W, WINDOW_H);
        sf::Uint32 style = full ? sf::Style::Fullscreen : (sf::Style::Titlebar | sf::Style::Close);
        window.create(mode, "Learning Compare - C++/SFML Advanced", style, settings);
        window.setVerticalSyncEnabled(true);
    };
    recreateWindow(false);
    sf::Font font;
    bool hasFont = loadFont(font);
    if (!hasFont) std::cerr << "Fonte nao encontrada; textos podem nao aparecer.\n";

    ControlPanel panel(args);
    Sim sim(args);
    SimParams p = panel.params();
    sim.reset(p);

    sf::Clock clock;
    double simAccumulator = 0.0;
    double stepsSinceSample = 0.0;
    double sampleTimer = nowSeconds();
    double configSaveTimer = nowSeconds();
    double stepsPerSecond = 0.0;
    sf::View uiView(sf::FloatRect(0.0f, 0.0f, float(WINDOW_W), float(WINDOW_H)));
    sf::Vector2f viewCenter(CX, CY);
    bool panning = false;
    sf::Vector2f lastMouse(0.0f, 0.0f);

    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window), uiView);
            bool used = false;
            if (event.type == sf::Event::MouseWheelScrolled && mouse.x < SIM_W) {
                panel.multiplyZoom(event.mouseWheelScroll.delta > 0.0f ? 1.12f : 1.0f / 1.12f);
                used = true;
            } else {
                used = panel.handle(event, mouse);
            }
            if (!used && event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Right && mouse.x < SIM_W) {
                panning = true;
                lastMouse = mouse;
                used = true;
            }
            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Right) {
                panning = false;
            }
            if (event.type == sf::Event::MouseMoved && panning) {
                p = panel.params();
                sf::Vector2f delta = mouse - lastMouse;
                viewCenter -= delta / std::max(0.001f, p.zoom);
                lastMouse = mouse;
                used = true;
            }
            if (!used && event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) window.close();
                if (event.key.code == sf::Keyboard::F11) {
                    panel.setFullscreen(!fullscreen);
                }
                if (event.key.code == sf::Keyboard::Space) {
                    p = panel.params();
                    panel.setPaused(!p.paused);
                }
                if (event.key.code == sf::Keyboard::V) {
                    panel.toggleVision();
                }
                if (event.key.code == sf::Keyboard::R) panel.requestResetNow();
            }
        }

        p = panel.params();
        if (p.fullscreen != fullscreen) {
            recreateWindow(p.fullscreen);
        }
        if (panel.consumeResetRequest()) {
            sim.reset(p);
            simAccumulator = 0.0;
            viewCenter = sf::Vector2f(CX, CY);
        }
        if (panel.consumeFoodFillRequest()) sim.fillFood(p);

        float realDt = std::min(0.25f, clock.restart().asSeconds());
        int frameSteps = 0;
        if (!p.paused) {
            simAccumulator += double(realDt) * double(p.timeScale);
            int maxFrameSteps = 500;
            while (simAccumulator >= FIXED_DT && frameSteps < maxFrameSteps) {
                sim.step(p, FIXED_DT);
                simAccumulator -= FIXED_DT;
                ++frameSteps;
            }
            if (frameSteps >= maxFrameSteps) simAccumulator = 0.0;
        }
        stepsSinceSample += frameSteps;
        double now = nowSeconds();
        if (now - sampleTimer >= 0.5) {
            stepsPerSecond = stepsSinceSample / std::max(1e-9, now - sampleTimer);
            stepsSinceSample = 0.0;
            sampleTimer = now;
        }
        if (now - configSaveTimer >= 1.0) {
            panel.saveConfig(CONFIG_FILE);
            configSaveTimer = now;
        }

        float zoom = std::max(0.001f, p.zoom);
        sf::View simView;
        simView.setCenter(viewCenter);
        simView.setSize(float(SIM_W) / zoom, float(WINDOW_H) / zoom);
        simView.setViewport(sf::FloatRect(0.0f, 0.0f, float(SIM_W) / float(WINDOW_W), 1.0f));
        window.setView(simView);
        sim.render(window, nullptr, p);
        window.setView(uiView);
        if (hasFont) {
            panel.draw(window, font, sim.agentCount(), sim.predatorCount(), sim.foodCount(), sim.simTime(), stepsPerSecond,
                       sim.profile, sim.generation, sim.totalEats, sim.totalPredations, sim.bestEats(), sim.avgEats());
        }
        window.display();
    }
    panel.saveConfig(CONFIG_FILE);
}

} // namespace

int main(int argc, char** argv) {
    Args args = parseArgs(argc, argv);
    if (args.visual) runVisual(args);
    else runBenchmark(args);
    return 0;
}
