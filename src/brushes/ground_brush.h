#pragma once

#include <unordered_set>

#include "brush.h"

struct Position;
class MapView;

/**
 * Ground Brush
 * 
 * Possible optimization:
 * nextServerId() performs linear search. It is the fastest approach for small lists (< 30 items at least).
 * However, if in the future it is common to use larger ground brushes (40-60+ items) then a possible optimization to try
 * is Walker's Alias Method:
 * https://gamedev.stackexchange.com/a/162996
 * C++ Implementation: https://gist.github.com/Liam0205/0b5786e9bfc73e75eb8180b5400cd1f8
 */

class GroundBrush final : public Brush
{
  public:
    GroundBrush(const std::string &name, std::vector<WeightedItemId> &&weightedIds);
    GroundBrush(uint32_t id, const std::string &name, std::vector<WeightedItemId> &&weightedIds);
    GroundBrush(uint32_t id, const std::string &name, std::vector<WeightedItemId> &&weightedIds, uint32_t iconServerId);

    void apply(MapView &mapView, const Position &position) override;
    uint32_t iconServerId() const override;
    bool erasesItem(uint32_t serverId) const override;
    BrushType type() const override;
    std::vector<ItemPreviewInfo> previewInfo() const override;

    uint32_t nextServerId();

    uint32_t brushId() const noexcept;

  private:
    void initialize();
    uint32_t sampleServerId();

    std::unordered_set<uint32_t> serverIds;
    std::vector<WeightedItemId> _weightedIds;

    uint32_t id;
    uint32_t _iconServerId;

    uint32_t totalWeight = 0;
    uint32_t _nextId;
};