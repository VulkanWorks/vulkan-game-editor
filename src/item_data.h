#pragma once

#include <memory>

#include "item_type.h"

class Item;

struct ItemData
{
    ItemData()
        : _item(nullptr) {}

    ItemData(Item *item)
        : _item(item) {}

    virtual ItemDataType type() const noexcept = 0;
    virtual std::unique_ptr<ItemData> copy() const = 0;

    Item *item() const noexcept
    {
        return _item;
    }

    virtual void setItem(Item *item)
    {
        _item = item;
    }

  protected:
    // The item that the data belongs to
    Item *_item;
};

struct Teleport : public ItemData
{
    Teleport(const Position destination)
        : destination(destination) {}
    ItemDataType type() const noexcept override
    {
        return ItemDataType::Teleport;
    }

    std::unique_ptr<ItemData> copy() const override
    {
        return std::make_unique<Teleport>(destination);
    }

    Position destination;
};

struct HouseDoor : public ItemData
{
    HouseDoor(uint8_t doorId)
        : doorId(doorId) {}

    ItemDataType type() const noexcept override
    {
        return ItemDataType::HouseDoor;
    }

    std::unique_ptr<ItemData> copy() const override
    {
        return std::make_unique<HouseDoor>(doorId);
    }

    uint8_t doorId;
};

struct Depot : public ItemData
{
    Depot(uint16_t depotId)
        : depotId(depotId) {}

    ItemDataType type() const noexcept override
    {
        return ItemDataType::Depot;
    }

    std::unique_ptr<ItemData> copy() const override
    {
        return std::make_unique<Depot>(depotId);
    }

    uint16_t depotId;
};

struct Container : public ItemData
{
  public:
    struct HistoryParent
    {
    };

    struct TileParent
    {
        Position position;
        MapView *mapView;
    };

    using Parent = std::variant<std::monostate, HistoryParent, TileParent, Container *>;

  public:
    Container(uint16_t capacity)
        : _capacity(capacity) {}
    Container(uint16_t capacity, Item *item)
        : ItemData(item), _capacity(capacity) {}
    Container(uint16_t capacity, Item *item, Parent parent)
        : ItemData(item), _capacity(capacity) {}
    Container(uint16_t capacity, const std::vector<Item> &items);
    ~Container();

    Container(Container &&other) noexcept;
    Container &operator=(Container &&other) noexcept;

    ItemDataType type() const noexcept override;
    std::unique_ptr<ItemData> copy() const override;
    void setItem(Item *item) override
    {
        _item = item;
        _parent = std::monostate{};
    }

    const std::vector<Item>::const_iterator findItem(std::function<bool(const Item &)> predicate) const;

    bool insertItemTracked(Item &&item, size_t index);
    Item dropItemTracked(size_t index);

    bool insertItem(Item &&item, size_t index);
    bool addItem(Item &&item);
    bool addItem(int index, Item &&item);
    bool removeItem(Item *item);
    bool removeItem(size_t index);

    Item dropItem(size_t index);

    Item &itemAt(size_t index);

    std::optional<size_t> indexOf(Item *item) const;

    const Item &itemAt(size_t index) const;
    const std::vector<Item> &items() const noexcept;
    size_t size() const noexcept;
    bool isFull() const noexcept;
    bool empty() const noexcept;

    uint16_t capacity() const noexcept;
    uint16_t volume() const noexcept;

    std::vector<Item> _items;

    uint16_t _capacity;

    std::vector<Parent> parents()
    {
        std::vector<Parent> result{_parent};
        while (std::holds_alternative<Container *>(result.back()))
        {
            result.emplace_back(std::get<Container *>(result.back())->_parent);
        }

        return result;
    }

    void setParent(MapView *mapView, Position position)
    {
        _parent = TileParent{position, mapView};
    }

    void setParent(Container *container)
    {
        _parent = container;
    }

  private:
    Parent _parent;
};