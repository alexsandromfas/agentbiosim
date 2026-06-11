#pragma once

#include "config/ParameterRegistry.hpp"
#include "neural/BrainConfig.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/SpeciesStore.hpp"
#include "simulation/World.hpp"
#include "systems/NeuralSystem.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>

namespace agentbiosim::systems
{
struct ReproductionConfig
{
    bool enabled = true;
    double splitEnergy = 150.0;
    double reproductionMinAge = 0.0;
    double reproductionCooldown = 0.0;
    double mutationRate = 0.05;
    double mutationStrength = 0.08;
    double initialEnergy = 100.0;
    double energyCap = 400.0;
    double bodySize = 9.0;
    int maxPopulation = 0;       // 0 means no limit.
    int minPopulation = 0;       // For documentation; rescue is deferred to species phase.
    bool populationMinRescue = true;
    double spawnRadiusOffset = 1.0;  // How far the child spawns from the parent (in radii).
    std::uint64_t seed = 20260528ULL;
    std::string speciesPrefix = "bacteria";
};

struct ReproductionStats
{
    std::size_t agentsConsidered = 0;
    std::size_t agentsEligible = 0;
    std::size_t birthsThisStep = 0;
    std::size_t blockedByPopulation = 0;
    std::size_t blockedByEnergy = 0;
    std::size_t blockedByAge = 0;
    std::size_t blockedByCooldown = 0;
    std::size_t mutationsApplied = 0;
};

class ReproductionSystem
{
public:
    [[nodiscard]] static ReproductionConfig fromRegistry(const config::ParameterRegistry& parameters,
                                                          const std::string& speciesPrefix,
                                                          const neural::BrainConfig& brainConfig);

    // Phase 18: brainSignatureConfig is now passed BY VALUE (Debt 7 resolution).
    // Previously a const reference could dangle when `genomes.cloneFrom()` reallocated
    // the GenomeStore record vector mid-apply if the caller had captured a ref into
    // the same store. Copy is ~200 bytes and removes the latent UB.
    //
    // Microfase 31.1: when `species` is provided, the population CAP is enforced
    // PER LABEL — a child whose species record has maxPopulation > 0 is blocked
    // once that label reaches its max. Without a store (legacy tests) the old
    // global check (total agents vs config.maxPopulation) is kept.
    [[nodiscard]] ReproductionStats apply(simulation::AgentStore& agents,
                                           simulation::GenomeStore& genomes,
                                           NeuralSystem& neuralSystem,
                                           const simulation::World& world,
                                           neural::BrainConfig brainSignatureConfig,
                                           const ReproductionConfig& config,
                                           double dt,
                                           const simulation::SpeciesStore* species = nullptr);

    [[nodiscard]] const ReproductionStats& lastStats() const noexcept;
    [[nodiscard]] std::uint64_t totalBirths() const noexcept;

    void resetCounters() noexcept;
    void reseed(std::uint64_t seed);

private:
    std::mt19937_64 rng_{20260528ULL};
    bool rngInitialized_ = false;
    ReproductionStats lastStats_{};
    std::uint64_t totalBirths_ = 0;
};
} // namespace agentbiosim::systems
