#include "simulation/GenomeStore.hpp"

#include <stdexcept>

namespace agentbiosim::simulation
{
GenomeHandle GenomeStore::createGenome(GenomeRecord record)
{
    record.id = nextId_++;
    const std::size_t index = records_.size();
    records_.push_back(std::move(record));
    indexById_.emplace(records_.back().id, index);
    return {records_.back().id};
}

GenomeHandle GenomeStore::cloneFrom(const GenomeId parentId)
{
    const GenomeRecord* parent = find(parentId);
    if (parent == nullptr)
    {
        return {kInvalidGenomeId};
    }
    GenomeRecord child = *parent;
    child.parentId = parent->id;
    child.generation = parent->generation + 1;
    return createGenome(std::move(child));
}

void GenomeStore::restore(std::vector<GenomeRecord> records, const GenomeId nextId)
{
    records_ = std::move(records);
    indexById_.clear();
    for (std::size_t i = 0; i < records_.size(); ++i)
    {
        indexById_[records_[i].id] = i;
    }
    nextId_ = nextId;
}

void GenomeStore::clear()
{
    records_.clear();
    indexById_.clear();
    nextId_ = 1;
}

std::size_t GenomeStore::size() const noexcept
{
    return records_.size();
}

bool GenomeStore::empty() const noexcept
{
    return records_.empty();
}

bool GenomeStore::contains(const GenomeId id) const
{
    return indexById_.find(id) != indexById_.end();
}

const GenomeRecord* GenomeStore::find(const GenomeId id) const
{
    const auto it = indexById_.find(id);
    if (it == indexById_.end())
    {
        return nullptr;
    }
    return &records_[it->second];
}

GenomeRecord* GenomeStore::find(const GenomeId id)
{
    const auto it = indexById_.find(id);
    if (it == indexById_.end())
    {
        return nullptr;
    }
    return &records_[it->second];
}

const GenomeRecord& GenomeStore::get(const GenomeId id) const
{
    const GenomeRecord* record = find(id);
    if (record == nullptr)
    {
        throw std::out_of_range("GenomeStore::get: invalid GenomeId");
    }
    return *record;
}

const std::vector<GenomeRecord>& GenomeStore::records() const noexcept
{
    return records_;
}
} // namespace agentbiosim::simulation
