#include "simulation/SpeciesStore.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace agentbiosim::simulation
{
std::string SpeciesStore::normalize(std::string value)
{
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](const unsigned char ch) {
        return !std::isspace(ch);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](const unsigned char ch) {
        return !std::isspace(ch);
    }).base(), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        const char lower = static_cast<char>(std::tolower(ch));
        return lower == '-' || lower == ' ' ? '_' : lower;
    });
    return value;
}

SpeciesId SpeciesStore::registerSpecies(SpeciesRecord record)
{
    record.id = nextId_++;
    const std::size_t index = records_.size();
    records_.push_back(std::move(record));
    SpeciesRecord& stored = records_.back();
    indexById_.emplace(stored.id, index);

    if (!stored.name.empty())
    {
        aliasIndex_[normalize(stored.name)] = stored.id;
    }
    if (!stored.label.empty())
    {
        aliasIndex_[normalize(stored.label)] = stored.id;
    }
    if (!stored.parameterPrefix.empty())
    {
        aliasIndex_[normalize(stored.parameterPrefix)] = stored.id;
    }
    for (const auto& alias : stored.legacyAliases)
    {
        aliasIndex_[normalize(alias)] = stored.id;
    }
    return stored.id;
}

void SpeciesStore::clear()
{
    records_.clear();
    indexById_.clear();
    aliasIndex_.clear();
    nextId_ = 1;
}

std::size_t SpeciesStore::size() const noexcept
{
    return records_.size();
}

bool SpeciesStore::empty() const noexcept
{
    return records_.empty();
}

const SpeciesRecord* SpeciesStore::find(const SpeciesId id) const
{
    const auto it = indexById_.find(id);
    if (it == indexById_.end()) return nullptr;
    return &records_[it->second];
}

SpeciesRecord* SpeciesStore::find(const SpeciesId id)
{
    const auto it = indexById_.find(id);
    if (it == indexById_.end()) return nullptr;
    return &records_[it->second];
}

const SpeciesRecord& SpeciesStore::get(const SpeciesId id) const
{
    const SpeciesRecord* record = find(id);
    if (record == nullptr)
    {
        throw std::out_of_range("SpeciesStore::get: invalid SpeciesId");
    }
    return *record;
}

bool SpeciesStore::contains(const SpeciesId id) const
{
    return indexById_.find(id) != indexById_.end();
}

SpeciesId SpeciesStore::resolveAlias(const std::string& alias) const
{
    const auto key = normalize(alias);
    const auto it = aliasIndex_.find(key);
    return it == aliasIndex_.end() ? kInvalidSpeciesId : it->second;
}

const SpeciesRecord* SpeciesStore::findByName(const std::string& name) const
{
    return find(idByName(name));
}

SpeciesId SpeciesStore::idByName(const std::string& name) const
{
    const auto key = normalize(name);
    for (const auto& r : records_)
    {
        if (normalize(r.name) == key) return r.id;
    }
    return kInvalidSpeciesId;
}

bool SpeciesStore::setDefaultGenome(const SpeciesId id, const GenomeId genome)
{
    if (auto* r = find(id))
    {
        r->defaultGenomeId = genome;
        return true;
    }
    return false;
}

bool SpeciesStore::setColor(const SpeciesId id, const ColorRgb color)
{
    if (auto* r = find(id))
    {
        r->color = color;
        return true;
    }
    return false;
}

bool SpeciesStore::setEnabled(const SpeciesId id, const bool enabled)
{
    if (auto* r = find(id))
    {
        r->enabled = enabled;
        return true;
    }
    return false;
}

bool SpeciesStore::addAlias(const SpeciesId id, const std::string& alias)
{
    if (auto* r = find(id))
    {
        r->legacyAliases.push_back(alias);
        aliasIndex_[normalize(alias)] = id;
        return true;
    }
    return false;
}

const std::vector<SpeciesRecord>& SpeciesStore::records() const noexcept
{
    return records_;
}
} // namespace agentbiosim::simulation
