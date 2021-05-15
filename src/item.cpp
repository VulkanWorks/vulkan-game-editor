#include "item.h"

#include "graphics/appearances.h"
#include "item_data.h"
#include "items.h"
#include "sprite_info.h"
#include "util.h"

Item::Item(ItemTypeId itemTypeId)
    : itemType(Items::items.getItemTypeByServerId(itemTypeId)),
      _guid(Items::items.createItemGid())
{
    if (itemType->hasAnimation())
    {
        _animation = std::make_shared<ItemAnimation>(itemType->getSpriteInfo().animation());
    }
}

Item::Item(const Item &other)
    : itemType(other.itemType), _animation(other._animation), _guid(other._guid)
{
    Items::items.guidRefCreated(_guid);
}

Item::Item(Item &&other) noexcept
    : itemType(other.itemType),
      selected(other.selected),
      _attributes(std::move(other._attributes)),
      _animation(std::move(other._animation)),
      _itemData(std::move(other._itemData)),
      _subtype(other._subtype),
      _guid(other._guid)
{
    Items::items.guidRefCreated(_guid);

    if (_itemData)
    {
        _itemData->setItem(this);
    }
}

Item &Item::operator=(Item &&other) noexcept
{
    itemType = other.itemType;
    _attributes = std::move(other._attributes);
    _subtype = other._subtype;
    _itemData = std::move(other._itemData);
    selected = other.selected;
    _animation = std::move(other._animation);
    _guid = other._guid;

    Items::items.guidRefCreated(_guid);

    if (_itemData)
    {
        _itemData->setItem(this);
    }

    return *this;
}

Item::~Item()
{
    Items::items.guidRefDestroyed(_guid);
}

uint32_t Item::guid() const noexcept
{
    return _guid;
}

Item Item::deepCopy() const
{
    Item item(*this);

    if (_attributes)
    {
        auto attributeCopy = *_attributes.get();
        item._attributes = std::make_unique<std::unordered_map<ItemAttribute_t, ItemAttribute>>(std::move(attributeCopy));
    }

    item._subtype = this->_subtype;
    if (_itemData)
    {
        item._itemData = _itemData->copy();
        item._itemData->setItem(nullptr);
    }
    item.selected = this->selected;

    return item;
}

uint32_t Item::getSpriteId(const Position &pos) const
{
    uint32_t offset = getPatternIndex(pos);
    const SpriteInfo &spriteInfo = itemType->getSpriteInfo(0);
    if (spriteInfo.hasAnimation() && _animation)
    {
        offset += _animation->state.phaseIndex * spriteInfo.patternSize;
    }

    return spriteInfo.spriteIds.at(offset);
}

const TextureInfo Item::getTextureInfo(const Position &pos, TextureInfo::CoordinateType coordinateType) const
{
    // TODO Add more pattern checks like hanging item types
    uint32_t spriteId = getSpriteId(pos);

    return itemType->getTextureInfo(spriteId, coordinateType);
}

const uint32_t Item::getPatternIndex(const Position &pos) const
{
    const SpriteInfo &spriteInfo = itemType->getSpriteInfo();
    if (!itemType->isStackable())
        return itemType->getPatternIndex(pos);

    // For stackable items

    // Amount of sprites for the different counts
    uint8_t stackSpriteCount = spriteInfo.patternSize;
    if (stackSpriteCount == 1)
        return 0;

    int itemCount = count();
    if (itemCount <= 5)
        return itemCount - 1;
    else if (itemCount < 10)
        return to_underlying(StackSizeOffset::Five);
    else if (itemCount < 25)
        return to_underlying(StackSizeOffset::Ten);
    else if (itemCount < 50)
        return to_underlying(StackSizeOffset::TwentyFive);
    else
        return to_underlying(StackSizeOffset::Fifty);
}

void Item::setCount(uint8_t count) noexcept
{
    bool changed = count != _subtype;
    _subtype = count;

    if (changed)
    {
        Items::items.itemPropertyChanged(this, ItemChangeType::Count);
    }
}

bool Item::isGround() const noexcept
{
    return itemType->isGround();
}

bool Item::isBorder() const noexcept
{
    return itemType->hasFlag(AppearanceFlag::Border);
}

bool Item::isBottom() const noexcept
{
    return itemType->hasFlag(AppearanceFlag::Bottom);
}

bool Item::isTop() const noexcept
{
    return itemType->hasFlag(AppearanceFlag::Top);
}

void Item::animate() const
{
    if (_animation)
    {
        _animation->update();
    }
}

void Item::setAttribute(ItemAttribute &&attribute)
{
    if (!_attributes)
    {
        _attributes = std::make_unique<std::unordered_map<ItemAttribute_t, ItemAttribute>>();
    }

    _attributes->emplace(attribute.type(), std::move(attribute));
}

void Item::setActionId(uint16_t id)
{
    auto &attr = getOrCreateAttribute(ItemAttribute_t::ActionId);
    const auto oldActionId = attr.get<int>();
    if (oldActionId == id)
        return;

    attr.setInt(id);
    Items::items.itemPropertyChanged(this, ItemChangeType::ActionId);
}

void Item::setUniqueId(uint16_t id)
{
    auto &attr = getOrCreateAttribute(ItemAttribute_t::UniqueId);
    const auto oldUniqueId = attr.get<int>();
    if (oldUniqueId == id)
        return;

    attr.setInt(id);
    Items::items.itemPropertyChanged(this, ItemChangeType::UniqueId);
}

void Item::setText(const std::string &text)
{
    getOrCreateAttribute(ItemAttribute_t::Text).setString(text);
}

void Item::setText(std::string &&text)
{
    getOrCreateAttribute(ItemAttribute_t::Text).setString(std::move(text));
}

void Item::setDescription(const std::string &description)
{
    getOrCreateAttribute(ItemAttribute_t::Description).setString(description);
}

void Item::setDescription(std::string &&description)
{
    getOrCreateAttribute(ItemAttribute_t::Description).setString(std::move(description));
}

ItemAttribute &Item::getOrCreateAttribute(const ItemAttribute_t attributeType)
{
    if (!_attributes)
    {
        _attributes = std::make_unique<std::unordered_map<ItemAttribute_t, ItemAttribute>>();
    }

    _attributes->try_emplace(attributeType, attributeType);
    return _attributes->at(attributeType);
}

void Item::setItemData(Container &&container)
{
    _itemData = std::make_unique<Container>(std::move(container));
}

Container *Item::getOrCreateContainer()
{
    DEBUG_ASSERT(isContainer(), "Must be container.");
    if (itemDataType() != ItemDataType::Container)
    {
        _itemData = std::make_unique<Container>(itemType->volume, this);
    }

    auto container = getDataAs<Container>();
    container->setItem(this);

    return container;
}

ItemAnimation *Item::animation() const noexcept
{
    return _animation.get();
}

ItemData *Item::data() const
{
    return _itemData.get();
}
