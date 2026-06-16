#include "io/SaveFile.hpp"

#include "io/Json.hpp"
#include "neural/BrainConfig.hpp"
#include "neural/BrainSerializer.hpp"

#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace agentbiosim::io
{
namespace
{
using namespace agentbiosim::simulation;

// ---- primitive helpers -----------------------------------------------------
Json toJson(const Vec2 v)
{
    Json j = Json::makeObject();
    j.set("x", Json(v.x));
    j.set("y", Json(v.y));
    return j;
}
Vec2 vec2From(const Json* j)
{
    Vec2 v;
    if (j != nullptr) { v.x = j->getDouble("x"); v.y = j->getDouble("y"); }
    return v;
}

Json toJson(const ColorRgb c)
{
    Json j = Json::makeArray();
    j.push(Json(static_cast<int>(c.r)));
    j.push(Json(static_cast<int>(c.g)));
    j.push(Json(static_cast<int>(c.b)));
    return j;
}
ColorRgb colorFrom(const Json* j)
{
    ColorRgb c{220, 220, 220};
    if (j != nullptr && j->isArray() && j->size() >= 3)
    {
        c.r = static_cast<std::uint8_t>(j->at(0).asInt());
        c.g = static_cast<std::uint8_t>(j->at(1).asInt());
        c.b = static_cast<std::uint8_t>(j->at(2).asInt());
    }
    return c;
}

// Fase 32.1: weight/bias arrays are stored as base64 of the raw IEEE-754 bytes
// instead of one JSON number per value. This is EXACT (so brain checksums still
// match — no precision loss) and collapses the node count from one-node-per-weight
// to one-node-per-array. A dense save was ~120M JSON nodes and multiple GB of
// "%.17g" text, which both bloated the file AND made loading run out of memory.
// Base64 of the little-endian bytes is ~11 chars/double vs ~24, and parses with a
// single memcpy. (Same-machine save format; not meant to be portable across
// endianness, which matches the project's Windows-only target.)
constexpr char kB64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const unsigned char* data, const std::size_t n)
{
    std::string out;
    out.reserve((n + 2U) / 3U * 4U);
    std::size_t i = 0;
    for (; i + 3U <= n; i += 3U)
    {
        const unsigned int v = (static_cast<unsigned int>(data[i]) << 16) |
                               (static_cast<unsigned int>(data[i + 1]) << 8) |
                               static_cast<unsigned int>(data[i + 2]);
        out.push_back(kB64Chars[(v >> 18) & 0x3F]);
        out.push_back(kB64Chars[(v >> 12) & 0x3F]);
        out.push_back(kB64Chars[(v >> 6) & 0x3F]);
        out.push_back(kB64Chars[v & 0x3F]);
    }
    if (i < n)
    {
        const std::size_t rem = n - i;
        unsigned int v = static_cast<unsigned int>(data[i]) << 16;
        if (rem == 2U) v |= static_cast<unsigned int>(data[i + 1]) << 8;
        out.push_back(kB64Chars[(v >> 18) & 0x3F]);
        out.push_back(kB64Chars[(v >> 12) & 0x3F]);
        out.push_back(rem == 2U ? kB64Chars[(v >> 6) & 0x3F] : '=');
        out.push_back('=');
    }
    return out;
}

std::vector<unsigned char> base64Decode(const std::string& s)
{
    const auto val = [](const char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<unsigned char> out;
    out.reserve(s.size() / 4U * 3U);
    int buffer = 0;
    int bits = 0;
    for (const char c : s)
    {
        const int d = val(c);
        if (d < 0) continue;  // skips '=', whitespace, etc.
        buffer = (buffer << 6) | d;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

Json doublesToJson(const std::vector<double>& v)
{
    if (v.empty()) return Json(std::string{});
    const auto* bytes = reinterpret_cast<const unsigned char*>(v.data());
    return Json(base64Encode(bytes, v.size() * sizeof(double)));
}
std::vector<double> doublesFrom(const Json* a)
{
    std::vector<double> v;
    if (a == nullptr) return v;
    if (a->type() == Json::Type::String)
    {
        const std::vector<unsigned char> bytes = base64Decode(a->asString());
        v.resize(bytes.size() / sizeof(double));
        if (!v.empty()) std::memcpy(v.data(), bytes.data(), v.size() * sizeof(double));
        return v;
    }
    // Backward compatibility: older saves stored a JSON array of numbers.
    if (a->isArray())
    {
        v.reserve(a->size());
        for (const auto& e : a->items()) v.push_back(e.asDouble());
    }
    return v;
}
Json doubles2DToJson(const std::vector<std::vector<double>>& v)
{
    Json a = Json::makeArray();
    for (const auto& row : v) a.push(doublesToJson(row));
    return a;
}
std::vector<std::vector<double>> doubles2DFrom(const Json* a)
{
    std::vector<std::vector<double>> v;
    if (a != nullptr && a->isArray())
    {
        for (const auto& row : a->items()) v.push_back(doublesFrom(&row));
    }
    return v;
}
Json sizesToJson(const std::vector<std::size_t>& v)
{
    Json a = Json::makeArray();
    for (const std::size_t s : v) a.push(Json(static_cast<std::uint64_t>(s)));
    return a;
}
std::vector<std::size_t> sizesFrom(const Json* a)
{
    std::vector<std::size_t> v;
    if (a != nullptr && a->isArray())
    {
        for (const auto& e : a->items()) v.push_back(static_cast<std::size_t>(e.asInt()));
    }
    return v;
}

// ---- DietConfig ------------------------------------------------------------
Json toJson(const DietConfig& d)
{
    Json j = Json::makeObject();
    j.set("eatFood", Json(d.eatFood));
    j.set("eatAgents", Json(d.eatAgents));
    j.set("eatSameSpecies", Json(d.eatSameSpecies));
    j.set("foodEfficiency", Json(d.foodEfficiency));
    j.set("agentEfficiency", Json(d.agentEfficiency));
    j.set("corpseToFood", Json(d.corpseToFood));
    return j;
}
DietConfig dietFrom(const Json* j)
{
    DietConfig d;
    if (j != nullptr)
    {
        d.eatFood = j->getBool("eatFood", true);
        d.eatAgents = j->getBool("eatAgents", false);
        d.eatSameSpecies = j->getBool("eatSameSpecies", false);
        d.foodEfficiency = j->getDouble("foodEfficiency", 1.0);
        d.agentEfficiency = j->getDouble("agentEfficiency", 0.7);
        d.corpseToFood = j->getBool("corpseToFood", false);
    }
    return d;
}

// ---- VisionConfig (Microfase 32.5) -----------------------------------------
Json toJson(const VisionConfig& v)
{
    Json j = Json::makeObject();
    j.set("seeFood", Json(v.seeFood));
    j.set("seeAgents", Json(v.seeAgents));
    j.set("seePredators", Json(v.seePredators));
    j.set("seeObstacles", Json(v.seeObstacles));
    j.set("seeAll", Json(v.seeAll));
    j.set("seeThroughWalls", Json(v.seeThroughWalls));
    return j;
}
VisionConfig visionFrom(const Json* j)
{
    VisionConfig v;
    if (j != nullptr)
    {
        v.seeFood = j->getBool("seeFood", true);
        v.seeAgents = j->getBool("seeAgents", false);
        v.seePredators = j->getBool("seePredators", false);
        v.seeObstacles = j->getBool("seeObstacles", false);
        v.seeAll = j->getBool("seeAll", false);
        v.seeThroughWalls = j->getBool("seeThroughWalls", true);
    }
    return v;
}

// ---- BrainConfig (topology + reconstruction recipe) ------------------------
Json toJson(const neural::BrainConfig& c)
{
    Json j = Json::makeObject();
    j.set("requestedType", Json(static_cast<int>(c.requestedType)));
    j.set("type", Json(static_cast<int>(c.type)));
    j.set("inputSize", Json(static_cast<std::uint64_t>(c.inputSize)));
    j.set("outputSize", Json(static_cast<std::uint64_t>(c.outputSize)));
    j.set("hiddenLayers", sizesToJson(c.hiddenLayers));
    j.set("mutationRate", Json(c.mutationRate));
    j.set("mutationStrength", Json(c.mutationStrength));
    j.set("structuralJitter", Json(c.structuralJitter));
    j.set("initStd", Json(c.initStd));
    j.set("randomBiases", Json(c.randomBiases));

    Json f = Json::makeObject();
    f.set("gateInit", Json(c.future.gateInit));
    f.set("gateMin", Json(c.future.gateMin));
    f.set("gateMax", Json(c.future.gateMax));
    f.set("gateMutationRate", Json(c.future.gateMutationRate));
    f.set("gateMutationStrength", Json(c.future.gateMutationStrength));
    f.set("shortcutInitStd", Json(c.future.shortcutInitStd));
    f.set("shortcutScale", Json(c.future.shortcutScale));
    f.set("shortcutMutationRate", Json(c.future.shortcutMutationRate));
    f.set("shortcutMutationStrength", Json(c.future.shortcutMutationStrength));
    f.set("rnnRecurrentInitStd", Json(c.future.rnnRecurrentInitStd));
    f.set("rnnRecurrentScale", Json(c.future.rnnRecurrentScale));
    f.set("rnnMemoryDecay", Json(c.future.rnnMemoryDecay));
    f.set("rnnStateClip", Json(c.future.rnnStateClip));
    f.set("rnnResetStateOnCopy", Json(c.future.rnnResetStateOnCopy));
    f.set("rnnMutationRate", Json(c.future.rnnMutationRate));
    f.set("rnnMutationStrength", Json(c.future.rnnMutationStrength));
    j.set("future", std::move(f));

    Json n = Json::makeObject();
    n.set("initialTopology", Json(c.neat.initialTopology));
    n.set("weightInitStd", Json(c.neat.weightInitStd));
    n.set("weightMutationRate", Json(c.neat.weightMutationRate));
    n.set("weightMutationStrength", Json(c.neat.weightMutationStrength));
    n.set("addConnectionRate", Json(c.neat.addConnectionRate));
    n.set("addNodeRate", Json(c.neat.addNodeRate));
    n.set("toggleConnectionRate", Json(c.neat.toggleConnectionRate));
    n.set("removeConnectionRate", Json(c.neat.removeConnectionRate));
    n.set("resetWeightRate", Json(c.neat.resetWeightRate));
    n.set("maxHiddenNodes", Json(c.neat.maxHiddenNodes));
    n.set("maxConnections", Json(c.neat.maxConnections));
    n.set("recurrentConnectionRate", Json(c.neat.recurrentConnectionRate));
    n.set("memoryDecay", Json(c.neat.memoryDecay));
    n.set("stateClip", Json(c.neat.stateClip));
    n.set("resetStateOnCopy", Json(c.neat.resetStateOnCopy));
    j.set("neat", std::move(n));
    return j;
}
neural::BrainConfig brainConfigFrom(const Json* j)
{
    neural::BrainConfig c;
    if (j == nullptr) return c;
    c.requestedType = static_cast<neural::BrainType>(j->getInt("requestedType"));
    c.type = static_cast<neural::BrainType>(j->getInt("type"));
    c.inputSize = static_cast<std::size_t>(j->getInt("inputSize", 4));
    c.outputSize = static_cast<std::size_t>(j->getInt("outputSize", 2));
    c.hiddenLayers = sizesFrom(j->find("hiddenLayers"));
    c.mutationRate = j->getDouble("mutationRate", 0.05);
    c.mutationStrength = j->getDouble("mutationStrength", 0.08);
    c.structuralJitter = static_cast<int>(j->getInt("structuralJitter"));
    c.initStd = j->getDouble("initStd", 1.0);
    c.randomBiases = j->getBool("randomBiases", true);
    if (const Json* f = j->find("future"))
    {
        c.future.gateInit = f->getDouble("gateInit", 1.0);
        c.future.gateMin = f->getDouble("gateMin", 0.0);
        c.future.gateMax = f->getDouble("gateMax", 2.0);
        c.future.gateMutationRate = f->getDouble("gateMutationRate", -1.0);
        c.future.gateMutationStrength = f->getDouble("gateMutationStrength", -1.0);
        c.future.shortcutInitStd = f->getDouble("shortcutInitStd", 0.05);
        c.future.shortcutScale = f->getDouble("shortcutScale", 0.25);
        c.future.shortcutMutationRate = f->getDouble("shortcutMutationRate", -1.0);
        c.future.shortcutMutationStrength = f->getDouble("shortcutMutationStrength", -1.0);
        c.future.rnnRecurrentInitStd = f->getDouble("rnnRecurrentInitStd", 0.08);
        c.future.rnnRecurrentScale = f->getDouble("rnnRecurrentScale", 0.35);
        c.future.rnnMemoryDecay = f->getDouble("rnnMemoryDecay", 0.6);
        c.future.rnnStateClip = f->getDouble("rnnStateClip", 1.0);
        c.future.rnnResetStateOnCopy = f->getBool("rnnResetStateOnCopy", true);
        c.future.rnnMutationRate = f->getDouble("rnnMutationRate", -1.0);
        c.future.rnnMutationStrength = f->getDouble("rnnMutationStrength", -1.0);
    }
    if (const Json* n = j->find("neat"))
    {
        c.neat.initialTopology = n->getString("initialTopology", "minimal");
        c.neat.weightInitStd = n->getDouble("weightInitStd", 0.6);
        c.neat.weightMutationRate = n->getDouble("weightMutationRate", -1.0);
        c.neat.weightMutationStrength = n->getDouble("weightMutationStrength", -1.0);
        c.neat.addConnectionRate = n->getDouble("addConnectionRate", 0.08);
        c.neat.addNodeRate = n->getDouble("addNodeRate", 0.03);
        c.neat.toggleConnectionRate = n->getDouble("toggleConnectionRate", 0.01);
        c.neat.removeConnectionRate = n->getDouble("removeConnectionRate", 0.0);
        c.neat.resetWeightRate = n->getDouble("resetWeightRate", 0.02);
        c.neat.maxHiddenNodes = static_cast<int>(n->getInt("maxHiddenNodes", 64));
        c.neat.maxConnections = static_cast<int>(n->getInt("maxConnections", 512));
        c.neat.recurrentConnectionRate = n->getDouble("recurrentConnectionRate", 0.12);
        c.neat.memoryDecay = n->getDouble("memoryDecay", 0.85);
        c.neat.stateClip = n->getDouble("stateClip", 1.0);
        c.neat.resetStateOnCopy = n->getBool("resetStateOnCopy", true);
    }
    return c;
}

// ---- BrainSnapshot ---------------------------------------------------------
Json toJson(const neural::BrainSnapshot& b)
{
    Json j = Json::makeObject();
    j.set("config", toJson(b.config));
    j.set("layerSizes", sizesToJson(b.layerSizes));
    j.set("weights", doubles2DToJson(b.weights));
    j.set("biases", doubles2DToJson(b.biases));
    j.set("gates", doubles2DToJson(b.gates));
    j.set("shortcutWeights", doublesToJson(b.shortcutWeights));
    j.set("shortcutBias", doublesToJson(b.shortcutBias));
    j.set("recurrentWeights", doublesToJson(b.recurrentWeights));
    j.set("recurrentState", doublesToJson(b.recurrentState));
    Json nodes = Json::makeArray();
    for (const auto& n : b.neatNodes)
    {
        Json o = Json::makeObject();
        o.set("id", Json(n.id));
        o.set("kind", Json(n.kind));
        o.set("layer", Json(n.layer));
        o.set("activation", Json(n.activation));
        nodes.push(std::move(o));
    }
    j.set("neatNodes", std::move(nodes));
    Json conns = Json::makeArray();
    for (const auto& c : b.neatConnections)
    {
        Json o = Json::makeObject();
        o.set("src", Json(c.src));
        o.set("dst", Json(c.dst));
        o.set("weight", Json(c.weight));
        o.set("enabled", Json(c.enabled));
        o.set("recurrent", Json(c.recurrent));
        o.set("innovation", Json(c.innovation));
        conns.push(std::move(o));
    }
    j.set("neatConnections", std::move(conns));
    Json state = Json::makeArray();
    for (const auto& kv : b.neatState)
    {
        Json o = Json::makeObject();
        o.set("id", Json(kv.first));
        o.set("v", Json(kv.second));
        state.push(std::move(o));
    }
    j.set("neatState", std::move(state));
    j.set("neatNextNodeId", Json(b.neatNextNodeId));
    j.set("neatNextInnovation", Json(b.neatNextInnovation));
    return j;
}
neural::BrainSnapshot brainSnapshotFrom(const Json* j)
{
    neural::BrainSnapshot b;
    if (j == nullptr) return b;
    b.config = brainConfigFrom(j->find("config"));
    b.layerSizes = sizesFrom(j->find("layerSizes"));
    b.weights = doubles2DFrom(j->find("weights"));
    b.biases = doubles2DFrom(j->find("biases"));
    b.gates = doubles2DFrom(j->find("gates"));
    b.shortcutWeights = doublesFrom(j->find("shortcutWeights"));
    b.shortcutBias = doublesFrom(j->find("shortcutBias"));
    b.recurrentWeights = doublesFrom(j->find("recurrentWeights"));
    b.recurrentState = doublesFrom(j->find("recurrentState"));
    if (const Json* nodes = j->find("neatNodes"))
    {
        for (const auto& o : nodes->items())
        {
            neural::NeatNodeData n;
            n.id = static_cast<std::int32_t>(o.getInt("id"));
            n.kind = static_cast<int>(o.getInt("kind", 1));
            n.layer = o.getDouble("layer", 0.5);
            n.activation = static_cast<int>(o.getInt("activation", 1));
            b.neatNodes.push_back(n);
        }
    }
    if (const Json* conns = j->find("neatConnections"))
    {
        for (const auto& o : conns->items())
        {
            neural::NeatConnData c;
            c.src = static_cast<std::int32_t>(o.getInt("src"));
            c.dst = static_cast<std::int32_t>(o.getInt("dst"));
            c.weight = o.getDouble("weight");
            c.enabled = o.getBool("enabled", true);
            c.recurrent = o.getBool("recurrent", false);
            c.innovation = static_cast<std::int32_t>(o.getInt("innovation"));
            b.neatConnections.push_back(c);
        }
    }
    if (const Json* state = j->find("neatState"))
    {
        for (const auto& o : state->items())
        {
            b.neatState.emplace_back(static_cast<std::int32_t>(o.getInt("id")), o.getDouble("v"));
        }
    }
    b.neatNextNodeId = static_cast<std::int32_t>(j->getInt("neatNextNodeId"));
    b.neatNextInnovation = static_cast<std::int32_t>(j->getInt("neatNextInnovation", 1));
    return b;
}

// ---- AgentSpawn ------------------------------------------------------------
Json toJson(const EntityId id, const AgentSpawn& a)
{
    Json j = Json::makeObject();
    j.set("id", Json(id.value));
    j.set("pos", toJson(a.position));
    j.set("vel", toJson(a.velocity));
    j.set("angle", Json(a.angle));
    j.set("angVel", Json(a.angularVelocity));
    j.set("radius", Json(a.radius));
    j.set("energy", Json(a.energy));
    j.set("age", Json(a.age));
    j.set("reproCooldown", Json(a.reproductionCooldown));
    j.set("color", toJson(a.color));
    j.set("speciesId", Json(static_cast<std::uint64_t>(a.speciesId)));
    j.set("genomeId", Json(a.genomeId));
    j.set("typeCode", Json(static_cast<int>(a.typeCode)));
    j.set("bodyShape", Json(static_cast<int>(a.bodyShape)));
    return j;
}

// ---- FoodSpawn -------------------------------------------------------------
Json toJson(const EntityId id, const FoodSpawn& f, const Vec2 vel)
{
    Json j = Json::makeObject();
    j.set("id", Json(id.value));
    j.set("pos", toJson(f.position));
    j.set("vel", toJson(vel));
    j.set("radius", Json(f.radius));
    j.set("energy", Json(f.energy));
    j.set("initialEnergy", Json(f.initialEnergy));
    j.set("color", toJson(f.color));
    j.set("kind", Json(static_cast<int>(f.kind)));
    j.set("clusterId", Json(static_cast<std::uint64_t>(f.clusterId)));
    return j;
}

// ---- ObstacleSpawn ---------------------------------------------------------
Json toJson(const ObstacleId id, const ObstacleSpawn& o)
{
    Json j = Json::makeObject();
    j.set("id", Json(static_cast<std::uint64_t>(id)));
    j.set("pos", toJson(o.position));
    j.set("radius", Json(o.radius));
    j.set("brushRadius", Json(o.brushRadius));
    j.set("color", toJson(o.color));
    return j;
}

// ---- SpeciesRecord ---------------------------------------------------------
Json toJson(const SpeciesRecord& r)
{
    Json j = Json::makeObject();
    j.set("id", Json(static_cast<std::uint64_t>(r.id)));
    j.set("name", Json(r.name));
    j.set("label", Json(r.label));
    j.set("parameterPrefix", Json(r.parameterPrefix));
    j.set("color", toJson(r.color));
    j.set("initialCount", Json(r.initialCount));
    j.set("minPopulation", Json(r.minPopulation));
    j.set("maxPopulation", Json(r.maxPopulation));
    j.set("showGraph", Json(r.showGraph));
    j.set("enabled", Json(r.enabled));
    j.set("populationMinRescueEnabled", Json(r.populationMinRescueEnabled));
    j.set("defaultGenomeId", Json(r.defaultGenomeId));
    Json aliases = Json::makeArray();
    for (const auto& a : r.legacyAliases) aliases.push(Json(a));
    j.set("legacyAliases", std::move(aliases));
    j.set("typeCode", Json(static_cast<int>(r.typeCode)));
    j.set("bodyShape", Json(static_cast<int>(r.bodyShape)));
    j.set("diet", toJson(r.dietSnapshot));
    return j;
}
SpeciesRecord speciesFrom(const Json& j)
{
    SpeciesRecord r;
    r.id = static_cast<SpeciesId>(j.getInt("id"));
    r.name = j.getString("name");
    r.label = j.getString("label");
    r.parameterPrefix = j.getString("parameterPrefix");
    r.color = colorFrom(j.find("color"));
    r.initialCount = static_cast<int>(j.getInt("initialCount"));
    r.minPopulation = static_cast<int>(j.getInt("minPopulation"));
    r.maxPopulation = static_cast<int>(j.getInt("maxPopulation"));
    r.showGraph = j.getBool("showGraph", true);
    r.enabled = j.getBool("enabled", true);
    r.populationMinRescueEnabled = j.getBool("populationMinRescueEnabled", true);
    r.defaultGenomeId = static_cast<GenomeId>(j.getInt("defaultGenomeId"));
    if (const Json* a = j.find("legacyAliases"))
    {
        for (const auto& e : a->items()) r.legacyAliases.push_back(e.asString());
    }
    r.typeCode = static_cast<AgentTypeCode>(j.getInt("typeCode"));
    r.bodyShape = static_cast<BodyShapeCode>(j.getInt("bodyShape"));
    r.dietSnapshot = dietFrom(j.find("diet"));
    return r;
}

// ---- GenomeRecord ----------------------------------------------------------
Json toJson(const GenomeRecord& r)
{
    Json j = Json::makeObject();
    j.set("id", Json(r.id));
    j.set("parentId", Json(r.parentId));
    j.set("generation", Json(static_cast<std::uint64_t>(r.generation)));
    j.set("bodySize", Json(r.bodySize));
    j.set("bodyShape", Json(static_cast<int>(r.bodyShape)));
    j.set("color", toJson(r.color));
    j.set("mutationRate", Json(r.mutationRate));
    j.set("mutationStrength", Json(r.mutationStrength));
    j.set("reproductionMinAge", Json(r.reproductionMinAge));
    j.set("reproductionCooldown", Json(r.reproductionCooldown));
    j.set("reproductionMode", Json(static_cast<int>(r.reproductionMode)));
    j.set("offspringCount", Json(r.offspringCount));
    j.set("splitEnergy", Json(r.splitEnergy));
    j.set("initialEnergy", Json(r.initialEnergy));
    j.set("energyCap", Json(r.energyCap));
    // Fase 34.2: per-individual locomotion / metabolic-cost / death-energy traits.
    j.set("maxSpeed", Json(r.maxSpeed));
    j.set("maxTurn", Json(r.maxTurn));
    j.set("allowReverse", Json(r.allowReverse));
    j.set("moveCostV0", Json(r.moveCostV0));
    j.set("moveCostVmax", Json(r.moveCostVmax));
    j.set("deathEnergy", Json(r.deathEnergy));
    j.set("speciesId", Json(static_cast<std::uint64_t>(r.speciesId)));
    j.set("typeCode", Json(static_cast<int>(r.typeCode)));
    j.set("brainConfig", toJson(r.brainConfig));
    j.set("speciesPrefix", Json(r.speciesPrefix));
    j.set("diet", toJson(r.diet));
    j.set("vision", toJson(r.vision));
    return j;
}
GenomeRecord genomeFrom(const Json& j)
{
    GenomeRecord r;
    r.id = static_cast<GenomeId>(j.getInt("id"));
    r.parentId = static_cast<GenomeId>(j.getInt("parentId"));
    r.generation = static_cast<std::uint32_t>(j.getInt("generation"));
    r.bodySize = j.getDouble("bodySize", 9.0);
    r.bodyShape = static_cast<BodyShapeCode>(j.getInt("bodyShape"));
    r.color = colorFrom(j.find("color"));
    r.mutationRate = j.getDouble("mutationRate", 0.05);
    r.mutationStrength = j.getDouble("mutationStrength", 0.08);
    r.reproductionMinAge = j.getDouble("reproductionMinAge", 0.0);
    r.reproductionCooldown = j.getDouble("reproductionCooldown", 0.0);
    r.reproductionMode = static_cast<ReproductionMode>(j.getInt("reproductionMode", 0));
    r.offspringCount = std::max(1, static_cast<int>(j.getInt("offspringCount", 1)));
    r.splitEnergy = j.getDouble("splitEnergy", 150.0);
    r.initialEnergy = j.getDouble("initialEnergy", 100.0);
    r.energyCap = j.getDouble("energyCap", 400.0);
    // Fase 34.2: read with the old global defaults so pre-34.2 saves load sanely.
    r.maxSpeed = j.getDouble("maxSpeed", 300.0);
    r.maxTurn = j.getDouble("maxTurn", 3.14159265358979323846);
    r.allowReverse = j.getBool("allowReverse", false);
    r.moveCostV0 = j.getDouble("moveCostV0", 0.5);
    r.moveCostVmax = j.getDouble("moveCostVmax", 8.0);
    r.deathEnergy = j.getDouble("deathEnergy", 50.0);
    r.speciesId = static_cast<SpeciesId>(j.getInt("speciesId"));
    r.typeCode = static_cast<AgentTypeCode>(j.getInt("typeCode"));
    r.brainConfig = brainConfigFrom(j.find("brainConfig"));
    r.speciesPrefix = j.getString("speciesPrefix");
    r.diet = dietFrom(j.find("diet"));
    r.vision = visionFrom(j.find("vision"));
    return r;
}

// ---- parameters ------------------------------------------------------------
Json toJson(const SavedParam& p)
{
    Json j = Json::makeObject();
    j.set("name", Json(p.first));
    std::visit(
        [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, bool>) { j.set("kind", Json("bool")); j.set("value", Json(value)); }
            else if constexpr (std::is_same_v<T, int>) { j.set("kind", Json("int")); j.set("value", Json(value)); }
            else if constexpr (std::is_same_v<T, double>) { j.set("kind", Json("double")); j.set("value", Json(value)); }
            else if constexpr (std::is_same_v<T, std::string>) { j.set("kind", Json("string")); j.set("value", Json(value)); }
            else if constexpr (std::is_same_v<T, config::ColorRgb>)
            {
                j.set("kind", Json("color"));
                Json c = Json::makeArray();
                c.push(Json(value.r)); c.push(Json(value.g)); c.push(Json(value.b));
                j.set("value", std::move(c));
            }
        },
        p.second);
    return j;
}
SavedParam paramFrom(const Json& j)
{
    const std::string name = j.getString("name");
    const std::string kind = j.getString("kind");
    const Json* v = j.find("value");
    if (kind == "bool") return {name, config::ParameterValue{v != nullptr && v->asBool()}};
    if (kind == "int") return {name, config::ParameterValue{static_cast<int>(v != nullptr ? v->asInt() : 0)}};
    if (kind == "double") return {name, config::ParameterValue{v != nullptr ? v->asDouble() : 0.0}};
    if (kind == "color")
    {
        config::ColorRgb c{};
        if (v != nullptr && v->isArray() && v->size() >= 3)
        {
            c.r = static_cast<int>(v->at(0).asInt());
            c.g = static_cast<int>(v->at(1).asInt());
            c.b = static_cast<int>(v->at(2).asInt());
        }
        return {name, config::ParameterValue{c}};
    }
    return {name, config::ParameterValue{v != nullptr ? v->asString() : std::string{}}};
}

std::string nowStamp()
{
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    tmv = *std::localtime(&t);
#endif
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    return buf;
}
} // namespace

bool saveToFile(const std::string& path, const SaveBundle& bundle, std::string& error)
{
    const sim::SimulationSnapshot& s = bundle.snapshot;
    Json root = Json::makeObject();

    Json meta = Json::makeObject();
    meta.set("format", Json("agentbiosim"));
    meta.set("schemaVersion", Json(kSaveSchemaVersion));
    meta.set("createdAt", Json(nowStamp()));
    root.set("meta", std::move(meta));

    Json engine = Json::makeObject();
    engine.set("seed", Json(s.seed));
    engine.set("stepsExecuted", Json(s.stepsExecuted));
    engine.set("timeScale", Json(s.timeScale));
    engine.set("paused", Json(s.paused));
    engine.set("foodEaten", Json(static_cast<std::uint64_t>(s.foodEaten)));
    engine.set("deaths", Json(static_cast<std::uint64_t>(s.deaths)));
    engine.set("births", Json(static_cast<std::uint64_t>(s.births)));
    root.set("engine", std::move(engine));

    Json world = Json::makeObject();
    world.set("shape", Json(static_cast<int>(s.world.shape)));
    world.set("width", Json(s.world.width));
    world.set("height", Json(s.world.height));
    world.set("radius", Json(s.world.radius));
    world.set("center", toJson(s.world.center));
    root.set("world", std::move(world));

    Json agents = Json::makeArray();
    for (std::size_t i = 0; i < s.agents.size() && i < s.agentIds.size(); ++i)
        agents.push(toJson(s.agentIds[i], s.agents[i]));
    root.set("agents", std::move(agents));
    root.set("nextAgentId", Json(s.nextAgentId));

    Json brains = Json::makeArray();
    for (const auto& kv : s.brains)
    {
        Json o = Json::makeObject();
        o.set("agentId", Json(kv.first));
        o.set("brain", toJson(kv.second));
        brains.push(std::move(o));
    }
    root.set("brains", std::move(brains));

    Json foods = Json::makeArray();
    for (std::size_t i = 0; i < s.foods.size() && i < s.foodIds.size(); ++i)
    {
        const Vec2 vel = i < s.foodVelocities.size() ? s.foodVelocities[i] : Vec2{};
        foods.push(toJson(s.foodIds[i], s.foods[i], vel));
    }
    root.set("foods", std::move(foods));
    root.set("nextFoodId", Json(s.nextFoodId));
    root.set("nextClusterId", Json(s.nextClusterId));

    Json obstacles = Json::makeArray();
    for (std::size_t i = 0; i < s.obstacles.size() && i < s.obstacleIds.size(); ++i)
        obstacles.push(toJson(s.obstacleIds[i], s.obstacles[i]));
    root.set("obstacles", std::move(obstacles));
    root.set("nextObstacleId", Json(s.nextObstacleId));

    Json species = Json::makeArray();
    for (const auto& r : s.species) species.push(toJson(r));
    root.set("species", std::move(species));
    root.set("nextSpeciesId", Json(s.nextSpeciesId));

    Json genomes = Json::makeArray();
    for (const auto& r : s.genomes) genomes.push(toJson(r));
    root.set("genomes", std::move(genomes));
    root.set("nextGenomeId", Json(s.nextGenomeId));

    Json params = Json::makeArray();
    for (const auto& p : bundle.params) params.push(toJson(p));
    root.set("params", std::move(params));

    if (bundle.camera.valid)
    {
        Json cam = Json::makeObject();
        cam.set("centerX", Json(bundle.camera.centerX));
        cam.set("centerY", Json(bundle.camera.centerY));
        cam.set("zoom", Json(bundle.camera.zoom));
        root.set("camera", std::move(cam));
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        error = "nao foi possivel abrir o arquivo para escrita: " + path;
        return false;
    }
    // Fase 32.1: NO pretty-print for save files. Indentation added a newline +
    // spaces before every value (millions of them in a dense brain dump),
    // bloating the file and the in-memory string for nothing readable at GB scale.
    const std::string text = root.dump(false);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out)
    {
        error = "falha ao gravar o arquivo: " + path;
        return false;
    }
    return true;
}

LoadResult loadFromFile(const std::string& path)
{
    LoadResult result;
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        result.error = "nao foi possivel abrir o arquivo: " + path;
        return result;
    }
    // Microfase 32.6: read with explicit 64-bit sizes. The old `ss << in.rdbuf()`
    // path truncated files at INT_MAX (2 GB) on MSVC, so a >2 GB save (e.g. the
    // pre-GC overnight bloat) failed to load with "unexpected end at offset
    // 2147483647". seekg/tellg/read all use std::streamoff/std::streamsize (64-bit).
    in.seekg(0, std::ios::end);
    const std::streamoff fileSize = in.tellg();
    in.seekg(0, std::ios::beg);
    std::string text;
    if (fileSize > 0)
    {
        text.resize(static_cast<std::size_t>(fileSize));
        in.read(text.data(), static_cast<std::streamsize>(fileSize));
        text.resize(static_cast<std::size_t>(in.gcount()));
    }

    Json root;
    std::string parseError;
    if (!Json::parse(text, root, parseError) || !root.isObject())
    {
        result.error = "arquivo invalido (JSON): " + parseError;
        return result;
    }

    const Json* meta = root.find("meta");
    if (meta == nullptr || meta->getString("format") != "agentbiosim")
    {
        result.error = "este nao e um arquivo .agentbiosim valido.";
        return result;
    }
    result.schemaVersion = static_cast<int>(meta->getInt("schemaVersion"));
    if (result.schemaVersion > kSaveSchemaVersion)
    {
        result.error = "save mais novo que esta versao do app (schema " +
                       std::to_string(result.schemaVersion) + ").";
        return result;
    }

    sim::SimulationSnapshot& s = result.bundle.snapshot;
    if (const Json* e = root.find("engine"))
    {
        s.seed = static_cast<std::uint64_t>(e->getInt("seed", 1337));
        s.stepsExecuted = static_cast<std::uint64_t>(e->getInt("stepsExecuted"));
        s.timeScale = e->getDouble("timeScale", 1.0);
        s.paused = e->getBool("paused");
        s.foodEaten = static_cast<std::size_t>(e->getInt("foodEaten"));
        s.deaths = static_cast<std::size_t>(e->getInt("deaths"));
        s.births = static_cast<std::size_t>(e->getInt("births"));
    }
    if (const Json* w = root.find("world"))
    {
        s.world.shape = static_cast<WorldShape>(w->getInt("shape"));
        s.world.width = w->getDouble("width", 1000.0);
        s.world.height = w->getDouble("height", 700.0);
        s.world.radius = w->getDouble("radius", 400.0);
        s.world.center = vec2From(w->find("center"));
    }
    if (const Json* a = root.find("agents"))
    {
        for (const auto& o : a->items())
        {
            s.agentIds.push_back(EntityId{static_cast<std::uint64_t>(o.getInt("id"))});
            AgentSpawn ag;
            ag.position = vec2From(o.find("pos"));
            ag.velocity = vec2From(o.find("vel"));
            ag.angle = o.getDouble("angle");
            ag.angularVelocity = o.getDouble("angVel");
            ag.radius = o.getDouble("radius", 9.0);
            ag.energy = o.getDouble("energy", 100.0);
            ag.age = o.getDouble("age");
            ag.reproductionCooldown = o.getDouble("reproCooldown");
            ag.color = colorFrom(o.find("color"));
            ag.speciesId = static_cast<SpeciesId>(o.getInt("speciesId"));
            ag.genomeId = static_cast<GenomeId>(o.getInt("genomeId"));
            ag.typeCode = static_cast<AgentTypeCode>(o.getInt("typeCode"));
            ag.bodyShape = static_cast<BodyShapeCode>(o.getInt("bodyShape"));
            s.agents.push_back(ag);
        }
    }
    s.nextAgentId = static_cast<std::uint64_t>(root.getInt("nextAgentId", 1));
    if (const Json* b = root.find("brains"))
    {
        for (const auto& o : b->items())
        {
            s.brains.emplace_back(static_cast<std::uint64_t>(o.getInt("agentId")),
                                  brainSnapshotFrom(o.find("brain")));
        }
    }
    if (const Json* f = root.find("foods"))
    {
        for (const auto& o : f->items())
        {
            s.foodIds.push_back(EntityId{static_cast<std::uint64_t>(o.getInt("id"))});
            FoodSpawn fd;
            fd.position = vec2From(o.find("pos"));
            fd.radius = o.getDouble("radius", 5.0);
            fd.energy = o.getDouble("energy", 25.0);
            fd.initialEnergy = o.getDouble("initialEnergy", 25.0);
            fd.color = colorFrom(o.find("color"));
            fd.kind = static_cast<FoodKind>(o.getInt("kind"));
            fd.clusterId = static_cast<std::uint32_t>(o.getInt("clusterId"));
            s.foods.push_back(fd);
            s.foodVelocities.push_back(vec2From(o.find("vel")));
        }
    }
    s.nextFoodId = static_cast<std::uint64_t>(root.getInt("nextFoodId", 1));
    s.nextClusterId = static_cast<std::uint32_t>(root.getInt("nextClusterId", 1));
    if (const Json* obs = root.find("obstacles"))
    {
        for (const auto& o : obs->items())
        {
            s.obstacleIds.push_back(static_cast<ObstacleId>(o.getInt("id")));
            ObstacleSpawn ob;
            ob.position = vec2From(o.find("pos"));
            ob.radius = o.getDouble("radius", 20.0);
            ob.brushRadius = o.getDouble("brushRadius", 20.0);
            ob.color = colorFrom(o.find("color"));
            s.obstacles.push_back(ob);
        }
    }
    s.nextObstacleId = static_cast<std::uint32_t>(root.getInt("nextObstacleId", 1));
    if (const Json* sp = root.find("species"))
    {
        for (const auto& o : sp->items()) s.species.push_back(speciesFrom(o));
    }
    s.nextSpeciesId = static_cast<std::uint32_t>(root.getInt("nextSpeciesId", 1));
    if (const Json* gn = root.find("genomes"))
    {
        for (const auto& o : gn->items()) s.genomes.push_back(genomeFrom(o));
    }
    s.nextGenomeId = static_cast<std::uint64_t>(root.getInt("nextGenomeId", 1));

    if (const Json* p = root.find("params"))
    {
        for (const auto& o : p->items()) result.bundle.params.push_back(paramFrom(o));
    }
    if (const Json* cam = root.find("camera"))
    {
        result.bundle.camera.valid = true;
        result.bundle.camera.centerX = cam->getDouble("centerX");
        result.bundle.camera.centerY = cam->getDouble("centerY");
        result.bundle.camera.zoom = cam->getDouble("zoom", 1.0);
    }

    result.ok = true;
    return result;
}

bool saveAgentToFile(const std::string& path, const sim::AgentExport& agent, std::string& error)
{
    Json root = Json::makeObject();
    Json meta = Json::makeObject();
    meta.set("format", Json("agentbiosim_organism"));
    meta.set("schemaVersion", Json(kSaveSchemaVersion));
    meta.set("createdAt", Json(nowStamp()));
    root.set("meta", std::move(meta));
    root.set("genome", toJson(agent.genome));
    root.set("brain", toJson(agent.brain));
    root.set("radius", Json(agent.radius));
    root.set("color", toJson(agent.color));

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        error = "nao foi possivel abrir o arquivo para escrita: " + path;
        return false;
    }
    // Fase 32.1: NO pretty-print for save files. Indentation added a newline +
    // spaces before every value (millions of them in a dense brain dump),
    // bloating the file and the in-memory string for nothing readable at GB scale.
    const std::string text = root.dump(false);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out)
    {
        error = "falha ao gravar o arquivo: " + path;
        return false;
    }
    return true;
}

AgentLoadResult loadAgentFromFile(const std::string& path)
{
    AgentLoadResult result;
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        result.error = "nao foi possivel abrir o arquivo: " + path;
        return result;
    }
    std::ostringstream ss;
    ss << in.rdbuf();

    Json root;
    std::string parseError;
    if (!Json::parse(ss.str(), root, parseError) || !root.isObject())
    {
        result.error = "arquivo invalido (JSON): " + parseError;
        return result;
    }
    const Json* meta = root.find("meta");
    if (meta == nullptr || meta->getString("format") != "agentbiosim_organism")
    {
        result.error = "este nao e um arquivo de organismo (.organism) valido.";
        return result;
    }
    result.agent.genome = genomeFrom(root.find("genome") != nullptr ? *root.find("genome")
                                                                     : Json::makeObject());
    result.agent.brain = brainSnapshotFrom(root.find("brain"));
    result.agent.radius = root.getDouble("radius", 9.0);
    result.agent.color = colorFrom(root.find("color"));
    result.ok = true;
    return result;
}
} // namespace agentbiosim::io
