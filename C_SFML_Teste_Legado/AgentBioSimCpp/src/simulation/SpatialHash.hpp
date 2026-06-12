#pragma once

#include "simulation/AgentStore.hpp"
#include "simulation/EntityId.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/World.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace agentbiosim::simulation
{
enum class SpatialEntityType : std::uint8_t
{
    Agent = 0,
    Food = 1,
    Obstacle = 2
};

struct SpatialHashConfig
{
    double cellSize = 36.0;
    Vec2 minBounds{0.0, 0.0};
    Vec2 maxBounds{1000.0, 700.0};
};

struct SpatialItem
{
    EntityId id{};
    std::size_t storeIndex = 0;
    SpatialEntityType entityType = SpatialEntityType::Agent;
    int typeCode = 0;
    double x = 0.0;
    double y = 0.0;
    double radius = 0.0;
};

struct SpatialHashStats
{
    std::size_t totalItems = 0;
    std::size_t totalBucketEntries = 0;
    std::size_t occupiedCells = 0;
    std::size_t totalCells = 0;
    std::size_t maxBucketSize = 0;
    double averageEntriesPerOccupiedCell = 0.0;
};

class SpatialHash
{
public:
    SpatialHash() = default;
    explicit SpatialHash(SpatialHashConfig config);

    void configure(SpatialHashConfig config);
    void clear();

    void insert(EntityId id,
                std::size_t storeIndex,
                SpatialEntityType entityType,
                double x,
                double y,
                double radius,
                int typeCode = 0);

    void rebuild(const AgentStore& agents, const FoodStore& foods);

    [[nodiscard]] std::vector<SpatialItem> queryRadius(double x, double y, double radius);
    void queryRadiusInto(double x, double y, double radius, std::vector<SpatialItem>& out);

    // Phase 32: thread-safe query. The member variants above mutate the shared
    // dedup stamps (seenStamp_/queryStamp_), so concurrent callers must use this
    // overload with their OWN scratch (one per thread). Read-only on the hash;
    // results identical to the member variant.
    struct QueryScratch
    {
        std::vector<std::uint32_t> seenStamp;
        std::uint32_t stamp = 0;
    };
    void queryRadiusInto(double x, double y, double radius, std::vector<SpatialItem>& out,
                         QueryScratch& scratch) const;

    [[nodiscard]] std::vector<SpatialItem> queryAabb(double minX, double minY, double maxX, double maxY);
    void queryAabbInto(double minX, double minY, double maxX, double maxY, std::vector<SpatialItem>& out);

    [[nodiscard]] SpatialHashStats stats() const;
    [[nodiscard]] double cellSize() const noexcept;
    [[nodiscard]] Vec2 minBounds() const noexcept;
    [[nodiscard]] Vec2 maxBounds() const noexcept;
    [[nodiscard]] std::size_t cols() const noexcept;
    [[nodiscard]] std::size_t rows() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    [[nodiscard]] int cellX(double x) const noexcept;
    [[nodiscard]] int cellY(double y) const noexcept;
    [[nodiscard]] int clampedCellX(double x) const noexcept;
    [[nodiscard]] int clampedCellY(double y) const noexcept;
    [[nodiscard]] std::size_t bucketIndex(int cx, int cy) const noexcept;
    [[nodiscard]] bool overlapsWorldAabb(double x, double y, double radius) const noexcept;
    void beginQuery();
    void insertItemIntoBuckets(std::size_t itemIndex);

    SpatialHashConfig config_{};
    std::size_t cols_ = 1;
    std::size_t rows_ = 1;
    std::vector<SpatialItem> items_;
    std::vector<std::vector<std::size_t>> buckets_;
    std::vector<std::uint32_t> seenStamp_;
    std::uint32_t queryStamp_ = 1;
};

struct SpatialHashValidationSummary
{
    bool passed = false;
    int checks = 0;
    std::string details;
};

struct SpatialHashBenchmarkResult
{
    int entities = 0;
    int queries = 0;
    double cellSize = 0.0;
    double rebuildMilliseconds = 0.0;
    double queryMilliseconds = 0.0;
    double averageQueryMicroseconds = 0.0;
    double averageCandidates = 0.0;
};

[[nodiscard]] SpatialHashConfig spatialConfigForWorld(const World& world, double cellSize);
[[nodiscard]] SpatialHashValidationSummary runSpatialHashValidation();
[[nodiscard]] std::vector<SpatialHashBenchmarkResult> runSpatialHashMicrobenchmark();
} // namespace agentbiosim::simulation
