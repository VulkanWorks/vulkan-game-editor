#include "tileset.h"

#include "brushes/brush.h"
#include "debug.h"
#include "item_palette.h"

Tileset::Tileset(std::string name)
    : _name(name) {}

const std::string &Tileset::name() const noexcept
{
    return _name;
}

size_t Tileset::size() const noexcept
{
    return brushes.size();
}

Brush *Tileset::get(size_t index) const
{
    return brushes.at(index);
}

ItemPalette *Tileset::palette() const noexcept
{
    return _palette;
}

void Tileset::setPalette(ItemPalette *palette) noexcept
{
    _palette = palette;
}

void Tileset::addRawBrush(uint32_t serverId)
{
    Brush *brush = Brush::getOrCreateRawBrush(serverId);
    if (hasBrush(brush))
    {
        auto paletteName = _palette ? _palette->name() : "(No palette)";
        VME_LOG_ERROR("The tileset '" << _name << "' in palette '" << paletteName << "' already contains a brush for serverId " << serverId << ".");
        return;
    }

    brush->setTileset(this);

    brushToIndexMap.try_emplace(brush, brushes.size());
    brushes.emplace_back(brush);
}

int Tileset::indexOf(Brush *brush) const
{
    auto found = brushToIndexMap.find(brush);
    if (found == brushToIndexMap.end())
    {
        return -1;
    }

    return static_cast<int>(found->second);
}

bool Tileset::hasBrush(Brush *brush) const
{
    return brushToIndexMap.find(brush) != brushToIndexMap.end();
}