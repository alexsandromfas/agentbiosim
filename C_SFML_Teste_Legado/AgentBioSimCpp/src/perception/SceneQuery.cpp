#include "perception/SceneQuery.hpp"

#include <cmath>

namespace agentbiosim::perception
{
namespace
{
double colorNormalized(const std::uint8_t channel)
{
    return static_cast<double>(channel) / 255.0;
}

bool matchesTypeFilter(const simulation::SpatialItem& item,
                       const bool seeFood, const bool seeAgents, const bool seePredators, const bool seeAll)
{
    if (seeAll)
    {
        return true;
    }
    if (item.entityType == simulation::SpatialEntityType::Food)
    {
        return seeFood;
    }
    if (item.entityType == simulation::SpatialEntityType::Agent)
    {
        if (seeAgents && item.typeCode == static_cast<int>(simulation::AgentTypeCode::LegacyBacteria))
        {
            return true;
        }
        if (seePredators && item.typeCode == static_cast<int>(simulation::AgentTypeCode::LegacyPredator))
        {
            return true;
        }
        if (seeAgents && item.typeCode == static_cast<int>(simulation::AgentTypeCode::Organism))
        {
            return true;
        }
    }
    return false;
}
} // namespace

namespace
{
// Shared filter: spatial items -> visible candidates (identical in both
// queryVisibleCandidates overloads).
void filterSpatialItems(const std::vector<simulation::SpatialItem>& spatialResults,
                        const bool seeFood, const bool seeAgents, const bool seePredators,
                        const bool seeAll, const std::uint64_t ignoreAgentId,
                        const simulation::AgentStore& agents,
                        const simulation::FoodStore& foods,
                        std::vector<VisibleCandidate>& out)
{
    out.reserve(spatialResults.size());
    for (const auto& item : spatialResults)
    {
        if (item.id.value == ignoreAgentId)
        {
            continue;
        }
        if (!matchesTypeFilter(item, seeFood, seeAgents, seePredators, seeAll))
        {
            continue;
        }

        VisibleCandidate candidate;
        candidate.x = item.x;
        candidate.y = item.y;
        candidate.radius = item.radius;
        candidate.entityType = item.entityType;
        candidate.typeCode = item.typeCode;
        candidate.entityId = item.id.value;

        if (item.entityType == simulation::SpatialEntityType::Food && item.storeIndex < foods.size())
        {
            const simulation::ColorRgb color = foods.colorAt(item.storeIndex);
            candidate.colorR = colorNormalized(color.r);
            candidate.colorG = colorNormalized(color.g);
            candidate.colorB = colorNormalized(color.b);
        }
        else if (item.entityType == simulation::SpatialEntityType::Agent && item.storeIndex < agents.size())
        {
            const simulation::ColorRgb color = agents.colorAt(item.storeIndex);
            candidate.colorR = colorNormalized(color.r);
            candidate.colorG = colorNormalized(color.g);
            candidate.colorB = colorNormalized(color.b);
        }

        out.push_back(candidate);
    }
}
} // namespace

void queryVisibleCandidates(const double searchX, const double searchY, const double searchRadius,
                            const bool seeFood, const bool seeAgents, const bool seePredators, const bool seeAll,
                            const std::uint64_t ignoreAgentId,
                            const simulation::SpatialHash* spatial,
                            const simulation::AgentStore& agents,
                            const simulation::FoodStore& foods,
                            std::vector<VisibleCandidate>& out,
                            SceneQueryScratch& scratch)
{
    out.clear();
    if (spatial != nullptr && !spatial->empty())
    {
        spatial->queryRadiusInto(searchX, searchY, searchRadius, scratch.items, scratch.spatial);
        filterSpatialItems(scratch.items, seeFood, seeAgents, seePredators, seeAll,
                           ignoreAgentId, agents, foods, out);
        return;
    }
    // Brute-force fallback (no spatial hash) is already const + alloc-free.
    queryVisibleCandidates(searchX, searchY, searchRadius, seeFood, seeAgents, seePredators,
                           seeAll, ignoreAgentId, nullptr, agents, foods, out);
}

void queryVisibleCandidates(const double searchX, const double searchY, const double searchRadius,
                            const bool seeFood, const bool seeAgents, const bool seePredators, const bool seeAll,
                            const std::uint64_t ignoreAgentId,
                            simulation::SpatialHash* spatial,
                            const simulation::AgentStore& agents,
                            const simulation::FoodStore& foods,
                            std::vector<VisibleCandidate>& out)
{
    out.clear();

    if (spatial != nullptr && !spatial->empty())
    {
        std::vector<simulation::SpatialItem> spatialResults;
        spatial->queryRadiusInto(searchX, searchY, searchRadius, spatialResults);
        filterSpatialItems(spatialResults, seeFood, seeAgents, seePredators, seeAll,
                           ignoreAgentId, agents, foods, out);
        return;
    }

    if (seeFood || seeAll)
    {
        for (std::size_t i = 0; i < foods.size(); ++i)
        {
            if (!foods.aliveAt(i))
            {
                continue;
            }
            const simulation::Vec2 pos = foods.positionAt(i);
            if (std::hypot(pos.x - searchX, pos.y - searchY) > searchRadius)
            {
                continue;
            }
            const simulation::ColorRgb color = foods.colorAt(i);
            VisibleCandidate candidate;
            candidate.x = pos.x;
            candidate.y = pos.y;
            candidate.radius = foods.radiusAt(i);
            candidate.colorR = colorNormalized(color.r);
            candidate.colorG = colorNormalized(color.g);
            candidate.colorB = colorNormalized(color.b);
            candidate.entityType = simulation::SpatialEntityType::Food;
            candidate.typeCode = 0;
            candidate.entityId = foods.idAt(i).value;
            out.push_back(candidate);
        }
    }

    if (seeAgents || seePredators || seeAll)
    {
        for (std::size_t i = 0; i < agents.size(); ++i)
        {
            if (!agents.aliveAt(i))
            {
                continue;
            }
            const std::uint64_t id = agents.idAt(i).value;
            if (id == ignoreAgentId)
            {
                continue;
            }
            const int tc = static_cast<int>(agents.typeCodeAt(i));
            const bool include = seeAll ||
                (seeAgents && (tc == static_cast<int>(simulation::AgentTypeCode::LegacyBacteria) ||
                               tc == static_cast<int>(simulation::AgentTypeCode::Organism))) ||
                (seePredators && tc == static_cast<int>(simulation::AgentTypeCode::LegacyPredator));
            if (!include)
            {
                continue;
            }
            const simulation::Vec2 pos = agents.positionAt(i);
            if (std::hypot(pos.x - searchX, pos.y - searchY) > searchRadius)
            {
                continue;
            }
            const simulation::ColorRgb color = agents.colorAt(i);
            VisibleCandidate candidate;
            candidate.x = pos.x;
            candidate.y = pos.y;
            candidate.radius = agents.radiusAt(i);
            candidate.colorR = colorNormalized(color.r);
            candidate.colorG = colorNormalized(color.g);
            candidate.colorB = colorNormalized(color.b);
            candidate.entityType = simulation::SpatialEntityType::Agent;
            candidate.typeCode = tc;
            candidate.entityId = id;
            out.push_back(candidate);
        }
    }
}

void appendObstacleCandidates(const double searchX, const double searchY, const double searchRadius,
                                const simulation::ObstacleStore& obstacles,
                                std::vector<VisibleCandidate>& out)
{
    for (std::size_t i = 0; i < obstacles.size(); ++i)
    {
        const auto pos = obstacles.positionAt(i);
        const double r = obstacles.radiusAt(i);
        const double dx = pos.x - searchX;
        const double dy = pos.y - searchY;
        const double reach = searchRadius + r;
        if (dx * dx + dy * dy > reach * reach) continue;
        const simulation::ColorRgb color = obstacles.colorAt(i);
        VisibleCandidate c;
        c.x = pos.x;
        c.y = pos.y;
        c.radius = r;
        c.colorR = static_cast<double>(color.r) / 255.0;
        c.colorG = static_cast<double>(color.g) / 255.0;
        c.colorB = static_cast<double>(color.b) / 255.0;
        c.entityType = simulation::SpatialEntityType::Obstacle;
        c.typeCode = 0;
        c.entityId = obstacles.idAt(i);
        out.push_back(c);
    }
}

bool isOccludedByObstacles(const double ax, const double ay, const double bx, const double by,
                            const simulation::ObstacleStore& obstacles) noexcept
{
    return obstacles.segmentBlocked({ax, ay}, {bx, by});
}
} // namespace agentbiosim::perception
