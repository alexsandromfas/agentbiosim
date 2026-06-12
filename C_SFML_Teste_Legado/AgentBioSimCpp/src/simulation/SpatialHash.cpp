#include "simulation/SpatialHash.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <unordered_set>

namespace agentbiosim::simulation
{
namespace
{
constexpr double kMinCellSize = 1.0;
constexpr double kEpsilon = 1.0e-9;

double widthOf(const SpatialHashConfig& config)
{
    return std::max(1.0, config.maxBounds.x - config.minBounds.x);
}

double heightOf(const SpatialHashConfig& config)
{
    return std::max(1.0, config.maxBounds.y - config.minBounds.y);
}

bool itemIntersectsRadius(const SpatialItem& item, const double x, const double y, const double radius)
{
    const double dx = item.x - x;
    const double dy = item.y - y;
    const double reach = std::max(0.0, radius) + std::max(0.0, item.radius);
    return (dx * dx + dy * dy) <= (reach * reach + kEpsilon);
}

bool itemIntersectsAabb(const SpatialItem& item, const double minX, const double minY, const double maxX, const double maxY)
{
    return item.x + item.radius >= minX &&
           item.x - item.radius <= maxX &&
           item.y + item.radius >= minY &&
           item.y - item.radius <= maxY;
}

std::vector<std::uint64_t> sortedIds(const std::vector<SpatialItem>& items)
{
    std::vector<std::uint64_t> ids;
    ids.reserve(items.size());
    for (const SpatialItem& item : items)
    {
        ids.push_back(item.id.value);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::uint64_t> bruteForceRadiusIds(const std::vector<SpatialItem>& items,
                                               const double x,
                                               const double y,
                                               const double radius)
{
    std::vector<std::uint64_t> ids;
    for (const SpatialItem& item : items)
    {
        if (itemIntersectsRadius(item, x, y, radius))
        {
            ids.push_back(item.id.value);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

void addCheck(SpatialHashValidationSummary& summary,
              const std::string& name,
              const std::vector<std::uint64_t>& actual,
              const std::vector<std::uint64_t>& expected)
{
    ++summary.checks;
    if (actual == expected)
    {
        return;
    }

    summary.passed = false;
    std::ostringstream line;
    line << "FAILED " << name << " expected=[";
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        line << (i == 0U ? "" : ",") << expected[i];
    }
    line << "] actual=[";
    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        line << (i == 0U ? "" : ",") << actual[i];
    }
    line << "]\n";
    summary.details += line.str();
}
} // namespace

SpatialHash::SpatialHash(SpatialHashConfig config)
{
    configure(config);
}

void SpatialHash::configure(SpatialHashConfig config)
{
    config.cellSize = std::max(kMinCellSize, config.cellSize);
    if (config.maxBounds.x <= config.minBounds.x)
    {
        config.maxBounds.x = config.minBounds.x + 1.0;
    }
    if (config.maxBounds.y <= config.minBounds.y)
    {
        config.maxBounds.y = config.minBounds.y + 1.0;
    }

    config_ = config;
    cols_ = std::max<std::size_t>(1U, static_cast<std::size_t>(std::ceil(widthOf(config_) / config_.cellSize)));
    rows_ = std::max<std::size_t>(1U, static_cast<std::size_t>(std::ceil(heightOf(config_) / config_.cellSize)));
    buckets_.assign(cols_ * rows_, {});
    clear();
}

void SpatialHash::clear()
{
    items_.clear();
    seenStamp_.clear();
    for (std::vector<std::size_t>& bucket : buckets_)
    {
        bucket.clear();
    }
    queryStamp_ = 1;
}

void SpatialHash::insert(const EntityId id,
                         const std::size_t storeIndex,
                         const SpatialEntityType entityType,
                         const double x,
                         const double y,
                         const double radius,
                         const int typeCode)
{
    if (!id.isValid() || !overlapsWorldAabb(x, y, radius))
    {
        return;
    }

    const std::size_t index = items_.size();
    items_.push_back({id, storeIndex, entityType, typeCode, x, y, std::max(0.0, radius)});
    seenStamp_.push_back(0U);
    insertItemIntoBuckets(index);
}

void SpatialHash::rebuild(const AgentStore& agents, const FoodStore& foods)
{
    clear();

    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents.aliveAt(i))
        {
            continue;
        }
        const Vec2 position = agents.positionAt(i);
        insert(agents.idAt(i),
               i,
               SpatialEntityType::Agent,
               position.x,
               position.y,
               agents.radiusAt(i),
               static_cast<int>(agents.typeCodeAt(i)));
    }

    for (std::size_t i = 0; i < foods.size(); ++i)
    {
        if (!foods.aliveAt(i))
        {
            continue;
        }
        const Vec2 position = foods.positionAt(i);
        insert(foods.idAt(i),
               i,
               SpatialEntityType::Food,
               position.x,
               position.y,
               foods.radiusAt(i),
               0);
    }
}

std::vector<SpatialItem> SpatialHash::queryRadius(const double x, const double y, const double radius)
{
    std::vector<SpatialItem> out;
    queryRadiusInto(x, y, radius, out);
    return out;
}

void SpatialHash::queryRadiusInto(const double x, const double y, const double radius, std::vector<SpatialItem>& out)
{
    out.clear();
    if (buckets_.empty())
    {
        return;
    }

    const double safeRadius = std::max(0.0, radius);
    const int minCx = clampedCellX(x - safeRadius);
    const int maxCx = clampedCellX(x + safeRadius);
    const int minCy = clampedCellY(y - safeRadius);
    const int maxCy = clampedCellY(y + safeRadius);

    beginQuery();
    for (int cy = minCy; cy <= maxCy; ++cy)
    {
        for (int cx = minCx; cx <= maxCx; ++cx)
        {
            const std::vector<std::size_t>& bucket = buckets_[bucketIndex(cx, cy)];
            for (const std::size_t itemIndex : bucket)
            {
                if (seenStamp_[itemIndex] == queryStamp_)
                {
                    continue;
                }
                seenStamp_[itemIndex] = queryStamp_;
                const SpatialItem& item = items_[itemIndex];
                if (itemIntersectsRadius(item, x, y, safeRadius))
                {
                    out.push_back(item);
                }
            }
        }
    }
}

void SpatialHash::queryRadiusInto(const double x, const double y, const double radius,
                                  std::vector<SpatialItem>& out, QueryScratch& scratch) const
{
    out.clear();
    if (buckets_.empty())
    {
        return;
    }

    const double safeRadius = std::max(0.0, radius);
    const int minCx = clampedCellX(x - safeRadius);
    const int maxCx = clampedCellX(x + safeRadius);
    const int minCy = clampedCellY(y - safeRadius);
    const int maxCy = clampedCellY(y + safeRadius);

    // Per-scratch stamp dedup, mirroring beginQuery() on the caller's storage.
    if (scratch.seenStamp.size() < items_.size())
    {
        scratch.seenStamp.assign(items_.size(), 0U);
        scratch.stamp = 0U;
    }
    ++scratch.stamp;
    if (scratch.stamp == 0U)
    {
        std::fill(scratch.seenStamp.begin(), scratch.seenStamp.end(), 0U);
        scratch.stamp = 1U;
    }

    for (int cy = minCy; cy <= maxCy; ++cy)
    {
        for (int cx = minCx; cx <= maxCx; ++cx)
        {
            const std::vector<std::size_t>& bucket = buckets_[bucketIndex(cx, cy)];
            for (const std::size_t itemIndex : bucket)
            {
                if (scratch.seenStamp[itemIndex] == scratch.stamp)
                {
                    continue;
                }
                scratch.seenStamp[itemIndex] = scratch.stamp;
                const SpatialItem& item = items_[itemIndex];
                if (itemIntersectsRadius(item, x, y, safeRadius))
                {
                    out.push_back(item);
                }
            }
        }
    }
}

std::vector<SpatialItem> SpatialHash::queryAabb(const double minX, const double minY, const double maxX, const double maxY)
{
    std::vector<SpatialItem> out;
    queryAabbInto(minX, minY, maxX, maxY, out);
    return out;
}

void SpatialHash::queryAabbInto(const double minX, const double minY, const double maxX, const double maxY, std::vector<SpatialItem>& out)
{
    out.clear();
    if (buckets_.empty())
    {
        return;
    }

    const double loX = std::min(minX, maxX);
    const double hiX = std::max(minX, maxX);
    const double loY = std::min(minY, maxY);
    const double hiY = std::max(minY, maxY);
    const int minCx = clampedCellX(loX);
    const int maxCx = clampedCellX(hiX);
    const int minCy = clampedCellY(loY);
    const int maxCy = clampedCellY(hiY);

    beginQuery();
    for (int cy = minCy; cy <= maxCy; ++cy)
    {
        for (int cx = minCx; cx <= maxCx; ++cx)
        {
            const std::vector<std::size_t>& bucket = buckets_[bucketIndex(cx, cy)];
            for (const std::size_t itemIndex : bucket)
            {
                if (seenStamp_[itemIndex] == queryStamp_)
                {
                    continue;
                }
                seenStamp_[itemIndex] = queryStamp_;
                const SpatialItem& item = items_[itemIndex];
                if (itemIntersectsAabb(item, loX, loY, hiX, hiY))
                {
                    out.push_back(item);
                }
            }
        }
    }
}

SpatialHashStats SpatialHash::stats() const
{
    SpatialHashStats result;
    result.totalItems = items_.size();
    result.totalCells = buckets_.size();

    for (const std::vector<std::size_t>& bucket : buckets_)
    {
        if (bucket.empty())
        {
            continue;
        }
        ++result.occupiedCells;
        result.totalBucketEntries += bucket.size();
        result.maxBucketSize = std::max(result.maxBucketSize, bucket.size());
    }

    if (result.occupiedCells > 0U)
    {
        result.averageEntriesPerOccupiedCell =
            static_cast<double>(result.totalBucketEntries) / static_cast<double>(result.occupiedCells);
    }
    return result;
}

double SpatialHash::cellSize() const noexcept
{
    return config_.cellSize;
}

Vec2 SpatialHash::minBounds() const noexcept
{
    return config_.minBounds;
}

Vec2 SpatialHash::maxBounds() const noexcept
{
    return config_.maxBounds;
}

std::size_t SpatialHash::cols() const noexcept
{
    return cols_;
}

std::size_t SpatialHash::rows() const noexcept
{
    return rows_;
}

bool SpatialHash::empty() const noexcept
{
    return items_.empty();
}

int SpatialHash::cellX(const double x) const noexcept
{
    return static_cast<int>(std::floor((x - config_.minBounds.x) / config_.cellSize));
}

int SpatialHash::cellY(const double y) const noexcept
{
    return static_cast<int>(std::floor((y - config_.minBounds.y) / config_.cellSize));
}

int SpatialHash::clampedCellX(const double x) const noexcept
{
    return std::clamp(cellX(x), 0, static_cast<int>(cols_) - 1);
}

int SpatialHash::clampedCellY(const double y) const noexcept
{
    return std::clamp(cellY(y), 0, static_cast<int>(rows_) - 1);
}

std::size_t SpatialHash::bucketIndex(const int cx, const int cy) const noexcept
{
    return static_cast<std::size_t>(cy) * cols_ + static_cast<std::size_t>(cx);
}

bool SpatialHash::overlapsWorldAabb(const double x, const double y, const double radius) const noexcept
{
    return x + radius >= config_.minBounds.x &&
           x - radius <= config_.maxBounds.x &&
           y + radius >= config_.minBounds.y &&
           y - radius <= config_.maxBounds.y;
}

void SpatialHash::beginQuery()
{
    ++queryStamp_;
    if (queryStamp_ == 0U)
    {
        std::fill(seenStamp_.begin(), seenStamp_.end(), 0U);
        queryStamp_ = 1U;
    }
}

void SpatialHash::insertItemIntoBuckets(const std::size_t itemIndex)
{
    const SpatialItem& item = items_[itemIndex];
    const int minCx = clampedCellX(item.x - item.radius);
    const int maxCx = clampedCellX(item.x + item.radius);
    const int minCy = clampedCellY(item.y - item.radius);
    const int maxCy = clampedCellY(item.y + item.radius);

    for (int cy = minCy; cy <= maxCy; ++cy)
    {
        for (int cx = minCx; cx <= maxCx; ++cx)
        {
            buckets_[bucketIndex(cx, cy)].push_back(itemIndex);
        }
    }
}

SpatialHashConfig spatialConfigForWorld(const World& world, const double cellSize)
{
    SpatialHashConfig config;
    config.cellSize = cellSize;
    if (world.shape() == WorldShape::Circular)
    {
        const Vec2 center = world.center();
        const double radius = std::max(1.0, world.radius());
        config.minBounds = {center.x - radius, center.y - radius};
        config.maxBounds = {center.x + radius, center.y + radius};
        return config;
    }

    config.minBounds = world.minBounds();
    config.maxBounds = world.maxBounds();
    return config;
}

SpatialHashValidationSummary runSpatialHashValidation()
{
    SpatialHashValidationSummary summary;
    summary.passed = true;

    SpatialHash hash({20.0, {0.0, 0.0}, {100.0, 100.0}});
    std::vector<SpatialItem> baseline;
    std::vector<SpatialItem> actual;

    hash.queryRadiusInto(50.0, 50.0, 5.0, actual);
    addCheck(summary, "empty query", sortedIds(actual), {});

    const auto add = [&](const std::uint64_t id, const double x, const double y, const double r, const SpatialEntityType type) {
        const SpatialItem item{EntityId{id}, 0U, type, type == SpatialEntityType::Food ? 0 : 1, x, y, r};
        baseline.push_back(item);
        hash.insert(item.id, item.storeIndex, item.entityType, item.x, item.y, item.radius, item.typeCode);
    };

    add(1, 10.0, 10.0, 2.0, SpatialEntityType::Agent);
    add(2, 90.0, 90.0, 2.0, SpatialEntityType::Food);
    hash.queryRadiusInto(10.0, 10.0, 3.0, actual);
    addCheck(summary, "one entity inside", sortedIds(actual), bruteForceRadiusIds(baseline, 10.0, 10.0, 3.0));

    hash.queryRadiusInto(50.0, 50.0, 5.0, actual);
    addCheck(summary, "entities outside", sortedIds(actual), bruteForceRadiusIds(baseline, 50.0, 50.0, 5.0));

    add(3, 12.0, 12.0, 2.0, SpatialEntityType::Agent);
    add(4, 14.0, 14.0, 2.0, SpatialEntityType::Food);
    hash.queryRadiusInto(12.0, 12.0, 5.0, actual);
    addCheck(summary, "multiple same cell", sortedIds(actual), bruteForceRadiusIds(baseline, 12.0, 12.0, 5.0));

    add(5, 21.0, 10.0, 2.0, SpatialEntityType::Agent);
    hash.queryRadiusInto(18.0, 10.0, 4.0, actual);
    addCheck(summary, "neighbor cells", sortedIds(actual), bruteForceRadiusIds(baseline, 18.0, 10.0, 4.0));

    add(6, 99.0, 1.0, 2.0, SpatialEntityType::Food);
    hash.queryRadiusInto(100.0, 0.0, 3.0, actual);
    addCheck(summary, "near bounds", sortedIds(actual), bruteForceRadiusIds(baseline, 100.0, 0.0, 3.0));

    hash.clear();
    hash.queryRadiusInto(12.0, 12.0, 100.0, actual);
    addCheck(summary, "clear", sortedIds(actual), {});

    AgentStore agents;
    FoodStore foods;
    AgentSpawn agentSpawn;
    agentSpawn.position = {25.0, 25.0};
    agentSpawn.radius = 3.0;
    const EntityId agentA = agents.createAgent(agentSpawn);
    agentSpawn.position = {60.0, 25.0};
    agentSpawn.radius = 3.0;
    const EntityId agentB = agents.createAgent(agentSpawn);

    FoodSpawn foodSpawn;
    foodSpawn.position = {27.0, 25.0};
    foodSpawn.radius = 2.0;
    const EntityId foodA = foods.createFood(foodSpawn);
    foodSpawn.position = {80.0, 80.0};
    foodSpawn.radius = 2.0;
    const EntityId foodB = foods.createFood(foodSpawn);
    hash.rebuild(agents, foods);

    baseline = {
        {agentA, 0U, SpatialEntityType::Agent, 1, 25.0, 25.0, 3.0},
        {agentB, 1U, SpatialEntityType::Agent, 1, 60.0, 25.0, 3.0},
        {foodA, 0U, SpatialEntityType::Food, 0, 27.0, 25.0, 2.0},
        {foodB, 1U, SpatialEntityType::Food, 0, 80.0, 80.0, 2.0},
    };
    hash.queryRadiusInto(25.0, 25.0, 4.0, actual);
    addCheck(summary, "mixed agents and food rebuild", sortedIds(actual), bruteForceRadiusIds(baseline, 25.0, 25.0, 4.0));

    hash.queryAabbInto(24.0, 24.0, 28.0, 28.0, actual);
    addCheck(summary, "aabb query", sortedIds(actual), std::vector<std::uint64_t>{agentA.value, foodA.value});

    if (summary.passed)
    {
        summary.details = "All SpatialHash validation checks passed.";
    }
    return summary;
}

std::vector<SpatialHashBenchmarkResult> runSpatialHashMicrobenchmark()
{
    constexpr int kQueries = 1000;
    constexpr double kWidth = 1000.0;
    constexpr double kHeight = 700.0;
    constexpr double kCellSize = 36.0;
    constexpr double kQueryRadius = 80.0;

    const int entityCounts[] = {100, 1000, 10000};
    std::vector<SpatialHashBenchmarkResult> results;
    results.reserve(3);

    std::mt19937 rng(20260527U);
    std::uniform_real_distribution<double> xDistribution(0.0, kWidth);
    std::uniform_real_distribution<double> yDistribution(0.0, kHeight);
    std::uniform_real_distribution<double> radiusDistribution(3.0, 10.0);

    for (const int count : entityCounts)
    {
        std::vector<SpatialItem> generated;
        generated.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            generated.push_back({
                EntityId{static_cast<std::uint64_t>(i + 1)},
                static_cast<std::size_t>(i),
                i % 5 == 0 ? SpatialEntityType::Food : SpatialEntityType::Agent,
                i % 5 == 0 ? 0 : 1,
                xDistribution(rng),
                yDistribution(rng),
                radiusDistribution(rng),
            });
        }

        SpatialHash hash({kCellSize, {0.0, 0.0}, {kWidth, kHeight}});
        const auto rebuildStart = std::chrono::high_resolution_clock::now();
        for (const SpatialItem& item : generated)
        {
            hash.insert(item.id, item.storeIndex, item.entityType, item.x, item.y, item.radius, item.typeCode);
        }
        const auto rebuildEnd = std::chrono::high_resolution_clock::now();

        std::vector<SpatialItem> candidates;
        std::size_t totalCandidates = 0;
        const auto queryStart = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < kQueries; ++i)
        {
            hash.queryRadiusInto(xDistribution(rng), yDistribution(rng), kQueryRadius, candidates);
            totalCandidates += candidates.size();
        }
        const auto queryEnd = std::chrono::high_resolution_clock::now();

        const double rebuildMs = std::chrono::duration<double, std::milli>(rebuildEnd - rebuildStart).count();
        const double queryMs = std::chrono::duration<double, std::milli>(queryEnd - queryStart).count();
        results.push_back({
            count,
            kQueries,
            kCellSize,
            rebuildMs,
            queryMs,
            (queryMs * 1000.0) / static_cast<double>(kQueries),
            static_cast<double>(totalCandidates) / static_cast<double>(kQueries),
        });
    }

    return results;
}
} // namespace agentbiosim::simulation
