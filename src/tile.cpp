#include "tile.h"

#include <numeric>

#include "tile_location.h"
#include "ecs/ecs.h"
#include "ecs/item_animation.h"

Tile::Tile(TileLocation &tileLocation)
    : _position(tileLocation.position()), selectionCount(0), flags(0)
{
}

Tile::Tile(Position position)
    : _position(position), selectionCount(0), flags(0) {}

Tile::Tile(Tile &&other) noexcept
    : _position(other._position),
      _ground(std::move(other._ground)),
      _items(std::move(other._items)),
      selectionCount(other.selectionCount),
      flags(other.flags)
{
}

Tile &Tile::operator=(Tile &&other) noexcept
{
  _items = std::move(other._items);
  _ground = std::move(other._ground);
  _position = std::move(other._position);
  selectionCount = other.selectionCount;
  flags = other.flags;

  return *this;
}

Tile::~Tile()
{
}

void Tile::setLocation(TileLocation &location)
{
  _position = location.position();
}

void Tile::removeItem(size_t index)
{
  deselectItemAtIndex(index);
  _items.erase(_items.begin() + index);
}

Item Tile::dropItem(size_t index)
{
  Item item = std::move(_items.at(index));
  _items.erase(_items.begin() + index);

  if (item.selected)
    selectionCount -= 1;

  return item;
}

void Tile::deselectAll()
{
  if (_ground)
    _ground->selected = false;

  for (Item &item : _items)
  {
    item.selected = false;
  }

  selectionCount = 0;
}

void Tile::moveItems(Tile &other)
{
  // TODO
  ABORT_PROGRAM("Not implemented.");
}

void Tile::moveSelected(Tile &other)
{
  if (_ground && _ground->selected)
  {
    other._items.clear();
    other._ground = dropGround();
  }

  auto it = _items.begin();
  while (it != _items.end())
  {
    Item &item = *it;
    if (item.selected)
    {
      other.addItem(std::move(item));
      it = _items.erase(it);

      --selectionCount;
    }
    else
    {
      ++it;
    }
  }
}

void Tile::addItem(Item &&item)
{
  if (item.isGround())
  {
    replaceGround(std::move(item));
    return;
  }

  std::vector<Item>::iterator it;

  bool replace = false;
  ItemType &itemType = *item.itemType;
  if (itemType.alwaysOnTop)
  {
    for (it = _items.begin(); it != _items.end(); ++it)
    {
      ItemType &currentType = *it->itemType;

      if (currentType.alwaysOnTop)
      {
        if (itemType.isGroundBorder())
        {
          if (!currentType.isGroundBorder())
          {
            break;
          }
        }
        else // New item is not a border
        {
          if (currentType.alwaysOnTop)
          {
            if (!currentType.isGroundBorder())
            {
              // Replace the current item at cursor with the new item
              replace = true;
              break;
            }
          }
        }
        // if (item.getTopOrder() < cursor->getTopOrder())
        // {
        //   break;
        // }
      }
      else
      {
        break;
      }
    }
  }
  else
  {
    it = _items.end();
  }

  if (replace)
  {
    replaceItem(it - _items.begin(), std::move(item));
  }
  else
  {
    if (item.selected)
      ++selectionCount;

    _items.emplace(it, std::move(item));
  }
}

void Tile::replaceGround(Item &&ground)
{
  bool s1 = _ground && _ground->selected;
  bool s2 = ground.selected;
  _ground = std::make_unique<Item>(std::move(ground));

  if (s1 && !s2)
    --selectionCount;
  else if (!s1 && s2)
    ++selectionCount;
}

void Tile::replaceItem(uint16_t index, Item &&item)
{
  bool s1 = _items.at(index).selected;
  bool s2 = item.selected;
  _items.at(index) = std::move(item);

  if (s1 && !s2)
    --selectionCount;
  else if (!s1 && s2)
    ++selectionCount;
}

void Tile::setGround(std::unique_ptr<Item> ground)
{
  DEBUG_ASSERT(ground->isGround(), "Tried to add a ground that is not a ground item.");

  if (_ground)
    removeGround();

  if (ground->selected)
    ++selectionCount;

  _ground = std::move(ground);
}

void Tile::removeGround()
{
  if (_ground->selected)
  {
    --selectionCount;
  }
  _ground.reset();
}

void Tile::setItemSelected(size_t itemIndex, bool selected)
{
  if (selected)
    selectItemAtIndex(itemIndex);
  else
    deselectItemAtIndex(itemIndex);
}

void Tile::selectItemAtIndex(size_t index)
{
  if (!_items.at(index).selected)
  {
    _items.at(index).selected = true;
    ++selectionCount;
  }
}

void Tile::deselectItemAtIndex(size_t index)
{
  if (_items.at(index).selected)
  {
    _items.at(index).selected = false;
    --selectionCount;
  }
}

void Tile::selectAll()
{
  size_t count = 0;
  if (_ground)
  {
    ++count;
    _ground->selected = true;
  }

  count += _items.size();
  for (Item &item : _items)
  {
    item.selected = true;
  }

  selectionCount = count;
}

void Tile::setGroundSelected(bool selected)
{
  if (selected)
    selectGround();
  else
    deselectGround();
}

void Tile::selectGround()
{
  if (_ground && !_ground->selected)
  {
    ++selectionCount;
    _ground->selected = true;
  }
}
void Tile::deselectGround()
{
  if (_ground && _ground->selected)
  {
    --selectionCount;
    _ground->selected = false;
  }
}

std::unique_ptr<Item> Tile::dropGround()
{
  if (_ground)
  {
    std::unique_ptr<Item> ground = std::move(_ground);
    _ground.reset();

    return ground;
  }
  else
  {
    return {};
  }
}

void Tile::selectTopItem()
{
  if (_items.empty())
  {
    selectGround();
  }
  else
  {
    selectItemAtIndex(_items.size() - 1);
  }
}

void Tile::deselectTopItem()
{
  if (_items.empty())
  {
    deselectGround();
  }
  else
  {
    deselectItemAtIndex(_items.size() - 1);
  }
}

Item *Tile::ground() const
{
  return _ground.get();
}

bool Tile::hasTopItem() const
{
  return !isEmpty();
}

Item *Tile::getTopItem() const
{
  if (_items.size() > 0)
  {
    return const_cast<Item *>(&_items.back());
  }
  if (_ground)
  {
    return _ground.get();
  }

  return nullptr;
}

bool Tile::topItemSelected() const
{
  if (!hasTopItem())
    return false;

  const Item *topItem = getTopItem();
  return allSelected() || topItem->selected;
}

size_t Tile::getEntityCount()
{
  size_t result = _items.size();
  if (_ground)
    ++result;

  return result;
}

int Tile::getTopElevation() const
{
  return std::accumulate(
      _items.begin(),
      _items.end(),
      0,
      [](int elevation, const Item &next) { return elevation + next.itemType->getElevation(); });
}

Tile Tile::deepCopy() const
{
  Tile tile(_position);
  for (const auto &item : _items)
  {
    tile.addItem(item.deepCopy());
  }
  tile.flags = this->flags;
  if (_ground)
  {
    tile._ground = std::make_unique<Item>(_ground->deepCopy());
  }

  tile.selectionCount = this->selectionCount;

  return tile;
}

bool Tile::isEmpty() const
{
  return !_ground && _items.empty();
}

bool Tile::allSelected() const
{
  size_t size = _items.size();
  if (_ground)
    ++size;

  return selectionCount == size;
}

bool Tile::hasSelection() const
{
  return selectionCount != 0;
}

void Tile::initEntities()
{
  if (_ground)
    _ground->registerEntity();

  for (auto &item : _items)
    item.registerEntity();
}

void Tile::destroyEntities()
{
  if (_ground)
    _ground->destroyEntity();

  for (auto &item : _items)
    item.destroyEntity();
}
