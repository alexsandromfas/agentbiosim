#pragma once

#include "simulation/EntityId.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace agentbiosim::ui
{
// Phase 22: SelectionState holds a list of agent entity ids selected by the
// user. EntityId is stable across swap-remove, so the ids stay valid even if
// the underlying AgentStore reorders. Consumers must check `AgentStore::contains`
// before using a selected id to guard against death.
class SelectionState
{
public:
    void clear() noexcept { ids_.clear(); }

    bool add(const simulation::EntityId id)
    {
        if (!id.isValid()) return false;
        if (contains(id)) return false;
        ids_.push_back(id);
        return true;
    }

    bool remove(const simulation::EntityId id)
    {
        const auto it = std::find_if(ids_.begin(), ids_.end(),
            [&](const simulation::EntityId v) { return v.value == id.value; });
        if (it == ids_.end()) return false;
        ids_.erase(it);
        return true;
    }

    [[nodiscard]] bool contains(const simulation::EntityId id) const noexcept
    {
        for (const auto v : ids_) if (v.value == id.value) return true;
        return false;
    }

    [[nodiscard]] std::size_t size() const noexcept { return ids_.size(); }
    [[nodiscard]] bool empty() const noexcept { return ids_.empty(); }
    [[nodiscard]] const std::vector<simulation::EntityId>& ids() const noexcept { return ids_; }

private:
    std::vector<simulation::EntityId> ids_;
};
} // namespace agentbiosim::ui
