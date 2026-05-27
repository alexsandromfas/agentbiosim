#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr float WINDOW_W = 1280.0f;
constexpr float WINDOW_H = 760.0f;
constexpr float MENU_W = 320.0f;
constexpr float SIM_W = WINDOW_W - MENU_W;

float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

float length(sf::Vector2f v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

sf::Vector2f normalize(sf::Vector2f v) {
    float l = length(v);
    if (l <= 1e-5f) return {0.0f, 0.0f};
    return {v.x / l, v.y / l};
}

sf::Color mix(sf::Color a, sf::Color b, float t) {
    t = clampf(t, 0.0f, 1.0f);
    auto lerp = [t](sf::Uint8 x, sf::Uint8 y) -> sf::Uint8 {
        return static_cast<sf::Uint8>(std::round(float(x) + (float(y) - float(x)) * t));
    };
    return {lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), lerp(a.a, b.a)};
}

std::string fixedText(float value, int precision = 1) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    return ss.str();
}

bool loadUIFont(sf::Font& font) {
    const std::vector<std::string> candidates = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf"
    };
    for (const auto& path : candidates) {
        if (font.loadFromFile(path)) return true;
    }
    return false;
}

struct Slider {
    std::string label;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float value = 0.0f;
    sf::FloatRect rect;
    bool dragging = false;

    float normalized() const {
        return (value - minValue) / std::max(0.0001f, maxValue - minValue);
    }

    void setFromMouse(float mx) {
        float t = clampf((mx - rect.left) / rect.width, 0.0f, 1.0f);
        value = minValue + (maxValue - minValue) * t;
    }

    bool handleEvent(const sf::Event& e, sf::Vector2f mouse) {
        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
            if (rect.contains(mouse)) {
                dragging = true;
                setFromMouse(mouse.x);
                return true;
            }
        }
        if (e.type == sf::Event::MouseButtonReleased && e.mouseButton.button == sf::Mouse::Left) {
            dragging = false;
        }
        if (e.type == sf::Event::MouseMoved && dragging) {
            setFromMouse(mouse.x);
            return true;
        }
        return false;
    }

    void draw(sf::RenderTarget& target, const sf::Font& font) const {
        sf::Text title(label, font, 15);
        title.setFillColor(sf::Color(220, 232, 245));
        title.setPosition(rect.left, rect.top - 25.0f);
        target.draw(title);

        sf::Text valueText(fixedText(value, (maxValue - minValue > 30.0f) ? 0 : 2), font, 14);
        valueText.setFillColor(sf::Color(140, 210, 255));
        valueText.setPosition(rect.left + rect.width - 62.0f, rect.top - 25.0f);
        target.draw(valueText);

        sf::RectangleShape rail({rect.width, rect.height});
        rail.setPosition(rect.left, rect.top);
        rail.setFillColor(sf::Color(31, 43, 57));
        target.draw(rail);

        sf::RectangleShape fill({rect.width * normalized(), rect.height});
        fill.setPosition(rect.left, rect.top);
        fill.setFillColor(sf::Color(66, 184, 240));
        target.draw(fill);

        sf::CircleShape knob(8.0f, 28);
        knob.setOrigin(8.0f, 8.0f);
        knob.setPosition(rect.left + rect.width * normalized(), rect.top + rect.height * 0.5f);
        knob.setFillColor(sf::Color(235, 250, 255));
        knob.setOutlineThickness(2.0f);
        knob.setOutlineColor(sf::Color(80, 210, 255));
        target.draw(knob);
    }
};

struct Toggle {
    std::string label;
    bool value = false;
    sf::FloatRect rect;

    bool handleEvent(const sf::Event& e, sf::Vector2f mouse) {
        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left && rect.contains(mouse)) {
            value = !value;
            return true;
        }
        return false;
    }

    void draw(sf::RenderTarget& target, const sf::Font& font) const {
        sf::RectangleShape box({rect.width, rect.height});
        box.setPosition(rect.left, rect.top);
        box.setFillColor(value ? sf::Color(56, 148, 210) : sf::Color(30, 41, 54));
        box.setOutlineThickness(1.0f);
        box.setOutlineColor(value ? sf::Color(130, 225, 255) : sf::Color(71, 83, 96));
        target.draw(box);

        sf::CircleShape indicator(8.0f, 24);
        indicator.setOrigin(8.0f, 8.0f);
        indicator.setPosition(rect.left + (value ? rect.width - 14.0f : 14.0f), rect.top + rect.height * 0.5f);
        indicator.setFillColor(value ? sf::Color(240, 252, 255) : sf::Color(115, 126, 137));
        target.draw(indicator);

        sf::Text text(label, font, 15);
        text.setFillColor(sf::Color(220, 232, 245));
        text.setPosition(rect.left + rect.width + 10.0f, rect.top + 2.0f);
        target.draw(text);
    }
};

struct Food {
    sf::Vector2f pos;
    float r = 3.0f;
};

struct Agent {
    sf::Vector2f pos;
    sf::Vector2f vel;
    float angle = 0.0f;
    float angularVel = 0.0f;
    float radius = 8.0f;
    float phase = 0.0f;
    bool predator = false;
    sf::Color color;
    std::vector<sf::Vector2f> trail;
};

class VisualMock {
public:
    VisualMock()
        : rng(std::random_device{}()) {
        fontLoaded = loadUIFont(font);
        resetControls();
        resizePopulation();
    }

    void run() {
        sf::ContextSettings settings;
        settings.antialiasingLevel = 8;
        sf::RenderWindow window(sf::VideoMode(static_cast<unsigned>(WINDOW_W), static_cast<unsigned>(WINDOW_H)),
                                "AgentBioSim SFML Visual Mock", sf::Style::Titlebar | sf::Style::Close, settings);
        window.setVerticalSyncEnabled(true);

        sf::Clock clock;
        while (window.isOpen()) {
            sf::Event event{};
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) window.close();
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) window.close();
                handleEvent(event, window);
            }

            float dt = std::min(0.033f, clock.restart().asSeconds());
            update(dt);
            draw(window);
        }
    }

private:
    enum SliderId {
        S_ORGANISMS,
        S_FOOD,
        S_PREDATORS,
        S_SPEED,
        S_AGENT_SIZE,
        S_VISCOSITY,
        S_GLOW,
        S_VISION_RANGE,
        S_TRAIL,
        S_WOBBLE,
        S_COUNT
    };

    enum ToggleId {
        T_TRAILS,
        T_VISION,
        T_GRADIENT,
        T_GLASS,
        T_ELASTIC,
        T_FOOD_GLOW,
        T_COUNT
    };

    sf::Font font;
    bool fontLoaded = false;
    std::mt19937 rng;
    std::vector<Slider> sliders;
    std::vector<Toggle> toggles;
    std::vector<Agent> agents;
    std::vector<Food> foods;
    sf::Vector2f dishCenter{SIM_W * 0.5f, WINDOW_H * 0.52f};
    float dishRadius = 325.0f;

    float randf(float lo, float hi) {
        std::uniform_real_distribution<float> dist(lo, hi);
        return dist(rng);
    }

    sf::Vector2f randomPointInDish(float margin = 0.0f) {
        float a = randf(0.0f, PI * 2.0f);
        float r = std::sqrt(randf(0.0f, 1.0f)) * std::max(1.0f, dishRadius - margin);
        return {dishCenter.x + std::cos(a) * r, dishCenter.y + std::sin(a) * r};
    }

    bool inDish(sf::Vector2f p, float margin = 0.0f) const {
        return length(p - dishCenter) <= dishRadius - margin;
    }

    void resetControls() {
        sliders.resize(S_COUNT);
        const float x = SIM_W + 34.0f;
        float y = 106.0f;
        auto addSlider = [&](int id, const std::string& label, float lo, float hi, float v) {
            sliders[id] = Slider{label, lo, hi, v, sf::FloatRect(x, y, 215.0f, 8.0f), false};
            y += 56.0f;
        };
        addSlider(S_ORGANISMS, "Organismos", 20.0f, 800.0f, 180.0f);
        addSlider(S_FOOD, "Comida", 20.0f, 700.0f, 180.0f);
        addSlider(S_PREDATORS, "Predadores", 0.0f, 80.0f, 14.0f);
        addSlider(S_SPEED, "Escala visual", 0.1f, 4.0f, 1.0f);
        addSlider(S_AGENT_SIZE, "Tamanho corpo", 3.0f, 15.0f, 7.5f);
        addSlider(S_VISCOSITY, "Viscosidade", 0.0f, 3.0f, 1.1f);
        addSlider(S_GLOW, "Halo / brilho", 0.0f, 1.0f, 0.42f);
        addSlider(S_VISION_RANGE, "Alcance cones", 45.0f, 240.0f, 130.0f);
        addSlider(S_TRAIL, "Memoria trilha", 2.0f, 60.0f, 18.0f);
        addSlider(S_WOBBLE, "Elasticidade visual", 0.0f, 1.0f, 0.38f);

        toggles.resize(T_COUNT);
        y += 8.0f;
        auto addToggle = [&](int id, const std::string& label, bool value) {
            toggles[id] = Toggle{label, value, sf::FloatRect(x, y, 44.0f, 22.0f)};
            y += 34.0f;
        };
        addToggle(T_TRAILS, "Trilhas suaves", true);
        addToggle(T_VISION, "Campos de visao", false);
        addToggle(T_GRADIENT, "Gradiente no substrato", true);
        addToggle(T_GLASS, "Borda tipo Petri", true);
        addToggle(T_ELASTIC, "Corpos elastizados", true);
        addToggle(T_FOOD_GLOW, "Comida luminosa", true);
    }

    void resizePopulation() {
        int targetAgents = static_cast<int>(std::round(sliders[S_ORGANISMS].value));
        int targetPred = static_cast<int>(std::round(sliders[S_PREDATORS].value));
        int targetFood = static_cast<int>(std::round(sliders[S_FOOD].value));

        targetPred = std::min(targetPred, targetAgents);
        int targetPrey = std::max(0, targetAgents - targetPred);

        int currentPred = 0;
        for (const auto& a : agents) if (a.predator) currentPred++;
        int currentPrey = static_cast<int>(agents.size()) - currentPred;

        while (currentPrey < targetPrey) {
            agents.push_back(makeAgent(false));
            currentPrey++;
        }
        while (currentPred < targetPred) {
            agents.push_back(makeAgent(true));
            currentPred++;
        }
        while (currentPrey > targetPrey) {
            auto it = std::find_if(agents.begin(), agents.end(), [](const Agent& a) { return !a.predator; });
            if (it == agents.end()) break;
            agents.erase(it);
            currentPrey--;
        }
        while (currentPred > targetPred) {
            auto it = std::find_if(agents.begin(), agents.end(), [](const Agent& a) { return a.predator; });
            if (it == agents.end()) break;
            agents.erase(it);
            currentPred--;
        }

        while (static_cast<int>(foods.size()) < targetFood) foods.push_back(makeFood());
        while (static_cast<int>(foods.size()) > targetFood) foods.pop_back();
    }

    Agent makeAgent(bool predator) {
        Agent a;
        a.predator = predator;
        a.pos = randomPointInDish(30.0f);
        float angle = randf(0.0f, PI * 2.0f);
        a.vel = {std::cos(angle) * randf(10.0f, 45.0f), std::sin(angle) * randf(10.0f, 45.0f)};
        a.angle = angle;
        a.radius = sliders[S_AGENT_SIZE].value * (predator ? 1.35f : 1.0f);
        a.phase = randf(0.0f, PI * 2.0f);
        a.color = predator ? sf::Color(235, 78, 82) : sf::Color(76, 230, 160);
        return a;
    }

    Food makeFood() {
        Food f;
        f.pos = randomPointInDish(18.0f);
        f.r = randf(2.4f, 4.5f);
        return f;
    }

    void handleEvent(const sf::Event& event, sf::RenderWindow& window) {
        sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        bool used = false;
        for (auto& s : sliders) used = s.handleEvent(event, mouse) || used;
        for (auto& t : toggles) used = t.handleEvent(event, mouse) || used;

        if (!used && event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            if (mouse.x < SIM_W && inDish(mouse, 8.0f)) {
                Food f;
                f.pos = mouse;
                f.r = randf(2.8f, 5.0f);
                foods.push_back(f);
                sliders[S_FOOD].value = static_cast<float>(foods.size());
            }
        }
    }

    void update(float dt) {
        resizePopulation();
        const float speed = sliders[S_SPEED].value;
        const float viscosity = sliders[S_VISCOSITY].value;
        const float baseRadius = sliders[S_AGENT_SIZE].value;
        const bool elastic = toggles[T_ELASTIC].value;
        const float wobble = sliders[S_WOBBLE].value;

        for (auto& a : agents) {
            a.radius = baseRadius * (a.predator ? 1.35f : 1.0f);
            a.phase += dt * (2.0f + speed);

            sf::Vector2f target = randomPointInDish(0.0f);
            float best = 1e30f;
            if (a.predator) {
                for (const auto& prey : agents) {
                    if (prey.predator) continue;
                    float d = length(prey.pos - a.pos);
                    if (d < best) {
                        best = d;
                        target = prey.pos;
                    }
                }
            } else if (!foods.empty()) {
                for (const auto& f : foods) {
                    float d = length(f.pos - a.pos);
                    if (d < best) {
                        best = d;
                        target = f.pos;
                    }
                }
                for (const auto& p : agents) {
                    if (!p.predator) continue;
                    float d = length(p.pos - a.pos);
                    if (d < 120.0f) target = a.pos - (p.pos - a.pos) * 2.0f;
                }
            }

            sf::Vector2f desired = normalize(target - a.pos) * (a.predator ? 115.0f : 85.0f) * speed;
            sf::Vector2f wander{std::cos(a.phase * 1.7f + a.pos.y * 0.01f), std::sin(a.phase * 1.3f + a.pos.x * 0.01f)};
            desired += wander * (28.0f + wobble * 26.0f);
            a.vel += (desired - a.vel) * dt * (1.8f + speed);
            a.vel *= std::max(0.0f, 1.0f - dt * viscosity * 0.55f);
            a.pos += a.vel * dt;

            if (!inDish(a.pos, a.radius + 3.0f)) {
                sf::Vector2f n = normalize(a.pos - dishCenter);
                a.pos = dishCenter + n * (dishRadius - a.radius - 3.0f);
                a.vel -= n * (1.7f * (a.vel.x * n.x + a.vel.y * n.y));
            }

            if (length(a.vel) > 1.0f) {
                float targetAngle = std::atan2(a.vel.y, a.vel.x);
                float delta = std::atan2(std::sin(targetAngle - a.angle), std::cos(targetAngle - a.angle));
                a.angularVel += delta * dt * 8.0f;
                a.angularVel *= std::max(0.0f, 1.0f - dt * (4.0f + viscosity));
                a.angle += a.angularVel;
            }

            if (toggles[T_TRAILS].value) {
                a.trail.push_back(a.pos);
                int maxTrail = static_cast<int>(sliders[S_TRAIL].value);
                while (static_cast<int>(a.trail.size()) > maxTrail) a.trail.erase(a.trail.begin());
            } else {
                a.trail.clear();
            }
        }

        solveAgentOverlap(dt);
        consumeFoodAndPredation();
    }

    void solveAgentOverlap(float dt) {
        const bool elastic = toggles[T_ELASTIC].value;
        if (!elastic) return;
        for (std::size_t i = 0; i < agents.size(); ++i) {
            for (std::size_t j = i + 1; j < agents.size(); ++j) {
                sf::Vector2f delta = agents[j].pos - agents[i].pos;
                float dist = length(delta);
                float minDist = agents[i].radius + agents[j].radius + 1.0f;
                if (dist > 0.001f && dist < minDist) {
                    sf::Vector2f n = delta / dist;
                    float overlap = minDist - dist;
                    agents[i].pos -= n * (overlap * 0.38f);
                    agents[j].pos += n * (overlap * 0.38f);
                    sf::Vector2f impulse = n * (overlap * 22.0f * dt);
                    agents[i].vel -= impulse;
                    agents[j].vel += impulse;
                }
            }
        }
    }

    void consumeFoodAndPredation() {
        for (auto& a : agents) {
            if (a.predator) continue;
            for (auto& f : foods) {
                if (length(f.pos - a.pos) < a.radius + f.r + 2.0f) {
                    f = makeFood();
                    a.vel *= 0.72f;
                    break;
                }
            }
        }
        for (auto& predator : agents) {
            if (!predator.predator) continue;
            for (auto& prey : agents) {
                if (prey.predator) continue;
                if (length(prey.pos - predator.pos) < predator.radius + prey.radius) {
                    prey = makeAgent(false);
                    predator.vel *= 0.65f;
                    break;
                }
            }
        }
    }

    void draw(sf::RenderWindow& window) {
        window.clear(sf::Color(6, 9, 14));
        drawBackground(window);
        drawDish(window);
        drawFoods(window);
        drawAgents(window);
        drawMenu(window);
        window.display();
    }

    void drawBackground(sf::RenderTarget& target) {
        sf::VertexArray bg(sf::Quads, 4);
        bg[0].position = {0.0f, 0.0f};
        bg[1].position = {WINDOW_W, 0.0f};
        bg[2].position = {WINDOW_W, WINDOW_H};
        bg[3].position = {0.0f, WINDOW_H};
        bg[0].color = bg[1].color = sf::Color(9, 16, 24);
        bg[2].color = bg[3].color = sf::Color(3, 7, 12);
        target.draw(bg);
    }

    void drawDish(sf::RenderTarget& target) {
        if (toggles[T_GRADIENT].value) {
            for (int y = static_cast<int>(dishCenter.y - dishRadius); y <= static_cast<int>(dishCenter.y + dishRadius); ++y) {
                float dy = float(y) - dishCenter.y;
                float dx = std::sqrt(std::max(0.0f, dishRadius * dishRadius - dy * dy));
                float t = (dy + dishRadius) / (dishRadius * 2.0f);
                sf::Vertex line[] = {
                    sf::Vertex({dishCenter.x - dx, float(y)}, mix(sf::Color(18, 31, 42), sf::Color(5, 13, 18), t)),
                    sf::Vertex({dishCenter.x + dx, float(y)}, mix(sf::Color(18, 31, 42), sf::Color(5, 13, 18), t))
                };
                target.draw(line, 2, sf::Lines);
            }
        } else {
            sf::CircleShape dish(dishRadius, 160);
            dish.setOrigin(dishRadius, dishRadius);
            dish.setPosition(dishCenter);
            dish.setFillColor(sf::Color(10, 19, 25));
            target.draw(dish);
        }

        if (toggles[T_GLASS].value) {
            sf::CircleShape glass(dishRadius + 9.0f, 192);
            glass.setOrigin(dishRadius + 9.0f, dishRadius + 9.0f);
            glass.setPosition(dishCenter);
            glass.setFillColor(sf::Color::Transparent);
            glass.setOutlineThickness(8.0f);
            glass.setOutlineColor(sf::Color(95, 210, 230, 98));
            target.draw(glass);

            sf::CircleShape highlight(dishRadius - 14.0f, 192);
            highlight.setOrigin(dishRadius - 14.0f, dishRadius - 14.0f);
            highlight.setPosition(dishCenter + sf::Vector2f(-18.0f, -18.0f));
            highlight.setFillColor(sf::Color::Transparent);
            highlight.setOutlineThickness(2.0f);
            highlight.setOutlineColor(sf::Color(210, 255, 255, 35));
            target.draw(highlight);
        }
    }

    void drawFoods(sf::RenderTarget& target) {
        for (const auto& f : foods) {
            if (toggles[T_FOOD_GLOW].value) {
                sf::CircleShape halo(f.r * 4.0f, 28);
                halo.setOrigin(f.r * 4.0f, f.r * 4.0f);
                halo.setPosition(f.pos);
                halo.setFillColor(sf::Color(255, 205, 78, static_cast<sf::Uint8>(42 + sliders[S_GLOW].value * 55)));
                target.draw(halo);
            }
            sf::CircleShape c(f.r, 20);
            c.setOrigin(f.r, f.r);
            c.setPosition(f.pos);
            c.setFillColor(sf::Color(245, 190, 75));
            target.draw(c);
        }
    }

    void drawAgents(sf::RenderTarget& target) {
        for (const auto& a : agents) {
            if (toggles[T_TRAILS].value && a.trail.size() > 2) {
                sf::VertexArray strip(sf::LineStrip, a.trail.size());
                for (std::size_t i = 0; i < a.trail.size(); ++i) {
                    float t = float(i) / float(a.trail.size() - 1);
                    strip[i].position = a.trail[i];
                    sf::Color color = a.color;
                    color.a = static_cast<sf::Uint8>(15 + 90 * t);
                    strip[i].color = color;
                }
                target.draw(strip);
            }
        }

        if (toggles[T_VISION].value) {
            for (const auto& a : agents) {
                if (a.predator) continue;
                drawVisionCone(target, a);
            }
        }

        for (const auto& a : agents) {
            const float glow = sliders[S_GLOW].value;
            if (glow > 0.01f) {
                sf::CircleShape halo(a.radius * (2.7f + glow), 48);
                halo.setOrigin(a.radius * (2.7f + glow), a.radius * (2.7f + glow));
                halo.setPosition(a.pos);
                sf::Color hc = a.color;
                hc.a = static_cast<sf::Uint8>(28 * glow);
                halo.setFillColor(hc);
                target.draw(halo);
            }

            float stretch = toggles[T_ELASTIC].value ? (1.25f + 0.12f * std::sin(a.phase * 2.0f) * sliders[S_WOBBLE].value) : 1.25f;
            sf::CircleShape body(1.0f, 44);
            body.setOrigin(1.0f, 1.0f);
            body.setPosition(a.pos);
            body.setRotation(a.angle * 180.0f / PI);
            body.setScale(a.radius * stretch, a.radius * 0.82f);
            body.setFillColor(a.color);
            body.setOutlineThickness(0.12f);
            body.setOutlineColor(sf::Color(245, 255, 255, 90));
            target.draw(body);

            sf::Vector2f head = a.pos + sf::Vector2f(std::cos(a.angle), std::sin(a.angle)) * (a.radius * stretch * 0.78f);
            sf::CircleShape eye(std::max(1.4f, a.radius * 0.22f), 20);
            eye.setOrigin(eye.getRadius(), eye.getRadius());
            eye.setPosition(head);
            eye.setFillColor(a.predator ? sf::Color(30, 3, 8) : sf::Color(2, 15, 12));
            target.draw(eye);
        }
    }

    void drawVisionCone(sf::RenderTarget& target, const Agent& a) {
        float range = sliders[S_VISION_RANGE].value;
        float half = 55.0f * PI / 180.0f;
        sf::VertexArray fan(sf::TriangleFan, 28);
        sf::Color inner(100, 220, 255, 20);
        sf::Color outer(100, 220, 255, 0);
        fan[0].position = a.pos;
        fan[0].color = inner;
        for (std::size_t i = 1; i < fan.getVertexCount(); ++i) {
            float t = float(i - 1) / float(fan.getVertexCount() - 2);
            float ang = a.angle - half + 2.0f * half * t;
            fan[i].position = a.pos + sf::Vector2f(std::cos(ang), std::sin(ang)) * range;
            fan[i].color = outer;
        }
        target.draw(fan);
    }

    void drawMenu(sf::RenderTarget& target) {
        sf::RectangleShape panel({MENU_W, WINDOW_H});
        panel.setPosition(SIM_W, 0.0f);
        panel.setFillColor(sf::Color(12, 18, 25, 238));
        target.draw(panel);

        sf::RectangleShape edge({2.0f, WINDOW_H});
        edge.setPosition(SIM_W, 0.0f);
        edge.setFillColor(sf::Color(70, 190, 230, 120));
        target.draw(edge);

        if (fontLoaded) {
            sf::Text title("Visual Lab SFML", font, 25);
            title.setFillColor(sf::Color(238, 248, 255));
            title.setPosition(SIM_W + 28.0f, 26.0f);
            target.draw(title);

            sf::Text subtitle("mock isolado C++ / CMake", font, 14);
            subtitle.setFillColor(sf::Color(123, 150, 171));
            subtitle.setPosition(SIM_W + 30.0f, 58.0f);
            target.draw(subtitle);

            for (const auto& s : sliders) s.draw(target, font);
            for (const auto& t : toggles) t.draw(target, font);

            sf::Text hint("Clique na placa para adicionar comida.\nESC fecha a janela.", font, 14);
            hint.setFillColor(sf::Color(122, 144, 163));
            hint.setPosition(SIM_W + 30.0f, WINDOW_H - 62.0f);
            target.draw(hint);
        }
    }
};

} // namespace

int main() {
    VisualMock app;
    app.run();
    return 0;
}
