#pragma once

#include <cstddef>
#include <string>

namespace agentbiosim::systems
{
// Phase 32 (optimization): zero-functional-regression tooling.
//
// `runPhase32StateChecksum()` runs fixed scenarios (seeded, N steps, multiple
// neural types and vision modes) and returns a high-precision digest of the
// final simulation state (counts, positions, energies, ages, brain weights).
// Captured BEFORE optimizing and compared after: identical strings prove the
// optimizations changed nothing behavioral (same seed -> same state).
[[nodiscard]] std::string runPhase32StateChecksum();

// Selftest: determinism (two same-seed runs -> identical digests, including
// with multithreaded systems), metrics series equality, and digest stability
// across the scenarios used for the before/after proof.
struct Phase32ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

[[nodiscard]] Phase32ValidationSummary runPhase32Validation();
} // namespace agentbiosim::systems
