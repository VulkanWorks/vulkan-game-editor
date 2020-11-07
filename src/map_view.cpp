#include "map_view.h"

#include "../vendor/rollbear-visit/visit.hpp"
#include "const.h"

using namespace MapHistory;

std::unordered_set<MapView *> MapView::instances;

MapView::MapView(std::unique_ptr<UIUtils> uiUtils, EditorAction &action)
    : MapView(std::move(uiUtils), action, std::make_shared<Map>())
{
  instances.emplace(this);
}

MapView::MapView(std::unique_ptr<UIUtils> uiUtils, EditorAction &action, std::shared_ptr<Map> map)
    : editorAction(action),
      history(*this),
      _map(map),
      _selection(*this, *map.get()),
      uiUtils(std::move(uiUtils))
{
  instances.emplace(this);
  camera.onViewportChanged<&MapView::cameraViewportChangedEvent>(this);
}

void MapView::cameraViewportChangedEvent()
{
  viewportChange.fire(camera.viewport());
  requestDraw();
}

MapView::~MapView()
{
  instances.erase(this);
}

//>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>Map mutators>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>

void MapView::undo()
{
  history.undoLast();
  requestDraw();
}

void MapView::selectTopItem(const Position pos)
{
  Tile *tile = _map->getTile(pos);
  DEBUG_ASSERT(tile != nullptr, "nullptr tile");
  selectTopItem(*tile);
}

void MapView::selectTopItem(const Tile &tile)
{
  auto selection = Select::topItem(tile);
  if (!selection.has_value())
    return;

  Action action(ActionType::Selection);
  action.addChange(std::move(selection.value()));

  history.commit(std::move(action));
}

void MapView::deselectTopItem(const Tile &tile)
{
  auto deselection = Deselect::topItem(tile);
  if (!deselection.has_value())
    return;

  Action action(ActionType::Selection);
  action.addChange(std::move(deselection.value()));

  history.commit(std::move(action));
}

void MapView::selectTile(const Position &pos)
{
  auto tile = getTile(pos);
  if (tile)
    selectTile(*tile);
}

void MapView::selectTile(const Tile &tile)
{
  auto selection = Select::fullTile(tile);
  if (!selection.has_value())
    return;

  Action action(ActionType::Selection);
  action.addChange(std::move(selection.value()));

  history.commit(std::move(action));
}

void MapView::deselectTile(const Position &pos)
{
  auto tile = getTile(pos);
  if (tile)
    deselectTile(*tile);
}

void MapView::deselectTile(const Tile &tile)
{
  auto deselection = Deselect::fullTile(tile);
  if (!deselection.has_value())
    return;

  Action action(ActionType::Selection);
  action.addChange(std::move(deselection.value()));

  history.commit(std::move(action));
}

void MapView::clearSelection()
{
  if (!_selection.empty())
  {
    history.startGroup(ActionGroupType::Selection);

    history.commit(
        ActionType::Selection,
        SelectMultiple(*this, _selection.allPositions(), false));

    history.endGroup(ActionGroupType::Selection);
  }
}

void MapView::modifyTile(const Position pos, std::function<void(Tile &)> f)
{
  Tile &currentTile = _map->getOrCreateTile(pos);
  Tile newTile = currentTile.deepCopy();
  f(newTile);

  history.commit(ActionType::SetTile, SetTile(std::move(newTile)));
}

void MapView::update(ActionGroupType groupType, std::function<void()> f)
{
  history.startGroup(groupType);
  f();
  history.endGroup(groupType);
}

void MapView::addItem(const Position &pos, uint16_t id)
{
  if (!Items::items.validItemType(id) || pos.x < 0 || pos.y < 0)
    return;

  Tile &currentTile = _map->getOrCreateTile(pos);
  Tile newTile = currentTile.deepCopy();

  newTile.addItem(Item(id));

  Action action(ActionType::SetTile);
  action.addChange(SetTile(std::move(newTile)));

  history.commit(std::move(action));
}

void MapView::removeItems(const Position position, const std::set<size_t, std::greater<size_t>> &indices)
{
  TileLocation *location = _map->getTileLocation(position);

  DEBUG_ASSERT(location->hasTile(), "The location has no tile.");

  Action action(ActionType::RemoveTile);

  Tile newTile = deepCopyTile(position);

  for (const auto index : indices)
  {
    newTile.removeItem(index);
  }

  action.addChange(SetTile(std::move(newTile)));

  history.commit(std::move(action));
}

void MapView::removeSelectedItems(const Tile &tile)
{
  Action action(ActionType::ModifyTile);

  Tile newTile = tile.deepCopy();

  for (size_t i = 0; i < tile.items().size(); ++i)
  {
    if (tile.items().at(i).selected)
      newTile.removeItem(i);
  }

  Item *ground = newTile.ground();
  if (ground && ground->selected)
  {
    newTile._ground.reset();
  }

  action.addChange(SetTile(std::move(newTile)));

  history.commit(std::move(action));
}

void MapView::removeItems(const Tile &tile, std::function<bool(const Item &)> predicate)
{
  Tile newTile = tile.deepCopy();
  if (newTile.removeItemsIf(predicate) > 0)
  {
    Action action(ActionType::ModifyTile);
    action.addChange(SetTile(std::move(newTile)));

    history.commit(std::move(action));
  }
}

void MapView::insertTile(Tile &&tile)
{
  history.commit(ActionType::SetTile, SetTile(std::move(tile)));
}

void MapView::removeTile(const Position position)
{
  Action action(ActionType::RemoveTile);

  action.addChange(RemoveTile(position));

  history.commit(std::move(action));
}

void MapView::finishMoveSelection()
{
  history.startGroup(ActionGroupType::MoveItems);
  {
    Action action(ActionType::Selection);

    Position deltaPos = _selection.moveDelta.value();
    Position pos = _selection.moveOrigin.value() + _selection.moveDelta.value();
    VME_LOG_D("finishMoveSelection: " << pos);

    auto multiMove = std::make_unique<MultiMove>(deltaPos, _selection.size());
    // VME_LOG_D("Moving " << _selection.size() << " tiles.");

    for (const auto fromPos : _selection)
    {
      const Tile &fromTile = *getTile(fromPos);
      Position toPos = fromPos + deltaPos;
      DEBUG_ASSERT(fromTile.hasSelection(), "The tile at each position of a selection should have a selection.");

      if (fromTile.allSelected())
        multiMove->add(Move::entire(fromPos, toPos));
      else
        multiMove->add(Move::selected(fromTile, toPos));
    }

    action.addChange(std::move(multiMove));

    history.commit(std::move(action));
  }
  history.endGroup(ActionGroupType::MoveItems);
  _selection.endMove();

  VME_LOG_D("Finished move.");
}

void MapView::deleteSelectedItems()
{
  if (_selection.empty())
  {
    return;
  }

  history.startGroup(ActionGroupType::RemoveMapItem);
  for (const auto &pos : _selection)
  {
    const Tile &tile = *getTile(pos);
    if (tile.allSelected())
    {
      removeTile(tile.position());
    }
    else
    {
      removeSelectedItems(tile);
    }
  }

  // TODO: Save the selected item state
  _selection.clear();

  history.endGroup(ActionGroupType::RemoveMapItem);
  requestDraw();
}

void MapView::selectRegion(const Position &from, const Position &to)
{
  std::vector<Position> positions;

  for (auto &location : _map->getRegion(from, to))
  {
    Tile *tile = location.tile();
    if (tile && !tile->isEmpty())
    {
      positions.emplace_back(location.position());
    }
  }

  // Only commit a change if anything was dragged over
  if (!positions.empty())
  {
    history.startGroup(ActionGroupType::Selection);

    Action action(ActionType::Selection);

    action.addChange(SelectMultiple(*this, std::move(positions)));

    history.commit(std::move(action));
    history.endGroup(ActionGroupType::Selection);
  }
}

void MapView::removeItemsInRegion(const Position &from, const Position &to, std::function<bool(const Item &)> predicate)
{
  history.startGroup(ActionGroupType::RemoveMapItem);
  for (auto &tileLocation : this->_map->getRegion(from, to))
  {
    if (tileLocation.hasTile())
      removeItems(*tileLocation.tile(), predicate);
  }

  history.endGroup(ActionGroupType::RemoveMapItem);
}

void MapView::fillRegion(const Position &from, const Position &to, uint16_t serverId)
{
  history.startGroup(ActionGroupType::AddMapItem);

  for (const auto &pos : MapArea(from, to))
  {
    addItem(pos, serverId);
  }

  history.endGroup(ActionGroupType::AddMapItem);
}

//>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>End of Map mutators>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>

void MapView::setViewportSize(int width, int height)
{
  camera.setSize(width, height);
}

void MapView::setDragStart(WorldPosition position)
{
  if (dragRegion.has_value())
  {
    dragRegion.value().setFrom(position);
  }
  else
  {
    dragRegion = Region2D(position, position);
  }
}

//>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>
//>>>>>>Accessors>>>>>>
//>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>

util::Rectangle<int> MapView::getGameBoundingRect() const
{
  Position position = camera.position();

  const Camera::Viewport &viewport = camera.viewport();
  auto [width, height] = ScreenPosition(viewport.width, viewport.height).mapPos(*this);
  util::Rectangle<int> rect;
  rect.x1 = position.x;
  rect.y1 = position.y;
  // Add one to not miss large sprites (64 in width or height) when zoomed in
  rect.x2 = position.x + width + 10;
  rect.y2 = position.y + height + 10;

  return rect;
}

std::optional<std::pair<WorldPosition, WorldPosition>> MapView::getDragPoints() const
{
  if (!dragRegion.has_value())
    return {};

  std::pair<WorldPosition, WorldPosition> result;
  result.first = dragRegion.value().from();
  result.second = dragRegion.value().to();
  return result;
}

MapRegion MapView::mapRegion() const
{
  Position from(camera.position());
  from.z = from.z <= GROUND_FLOOR ? GROUND_FLOOR : MAP_LAYERS - 1;

  const Camera::Viewport &viewport = camera.viewport();
  auto [width, height] = ScreenPosition(viewport.width, viewport.height).mapPos(*this);

  Position to(from.x + width, from.y + height, camera.z());

  return _map->getRegion(from, to);
}

const Tile *MapView::getTile(const Position pos) const
{
  return _map->getTile(pos);
}

Tile &MapView::getOrCreateTile(const Position pos)
{
  return _map->getOrCreateTile(pos);
}

bool MapView::draggingWithSubtract() const
{
  if (!isDragging())
    return false;
  auto action = editorAction.as<MouseAction::RawItem>();
  auto modifiers = uiUtils->modifiers();

  return action && (modifiers & VME::ModifierKeys::Shift) && (modifiers & VME::ModifierKeys::Ctrl);
}

bool MapView::hasSelection() const
{
  return !_selection.empty();
}

bool MapView::hasSelectionMoveOrigin() const
{
  return _selection.moveOrigin.has_value();
}

bool MapView::singleTileSelected() const
{
  return _selection.size() == 1;
}

bool MapView::isEmpty(const Position position) const
{
  return _map->isTileEmpty(position);
}

bool MapView::isDragging() const
{
  return dragRegion.has_value();
}

bool MapView::inDragRegion(Position pos) const
{
  if (!dragRegion)
    return false;

  WorldPosition topLeft = pos.worldPos();
  WorldPosition bottomRight(topLeft.x + MapTileSize, topLeft.y + MapTileSize);

  return dragRegion.value().collides(topLeft, bottomRight);
}

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>End of Accessors>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>

void MapView::setDragEnd(WorldPosition position)
{
  DEBUG_ASSERT(dragRegion.has_value(), "There is no current dragging operation.");

  dragRegion.value().setTo(position);
}

void MapView::endDragging(VME::ModifierKeys modifiers)
{
  auto [fromWorldPos, toWorldPos] = dragRegion.value();

  Position from = fromWorldPos.toPos(*this);
  Position to = toWorldPos.toPos(*this);

  std::visit(
      util::overloaded{
          [this, from, to](MouseAction::Select &select) {
            if (select.area)
            {
              selectRegion(from, to);

              // This prevents having the mouse release trigger a deselect of the tile being hovered
              _selection.blockDeselect = true;

              select.area = false;
            }
          },

          [this, modifiers, from, to](MouseAction::RawItem &action) {
            if (action.area)
            {
              if (modifiers & VME::ModifierKeys::Ctrl)
              {
                const auto predicate = [action](const Item &item) {
                  return item.serverId() == action.serverId;
                };
                removeItemsInRegion(from, to, predicate);
              }
              else
              {
                fillRegion(from, to, action.serverId);
              }

              action.area = false;
            }
          },

          [](const auto &arg) {}},
      editorAction.action());

  dragRegion.reset();
  requestDraw();
}

//>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>
//>>>>>>Events>>>>>>
//>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>

void MapView::mousePressEvent(VME::MouseEvent event)
{
  VME_LOG_D("MapView::mousePressEvent");
  if (event.buttons() & VME::MouseButtons::LeftButton)
  {
    Position pos = event.pos().toPos(*this);

    std::visit(
        util::overloaded{
            [this, pos, event](const MouseAction::Select action) {
              if (event.modifiers() & VME::ModifierKeys::Shift)
              {
                MouseAction::Select newAction = action;
                newAction.area = true;
                editorAction.set(newAction);
              }
              else
              {
                const Item *topItem = _map->getTopItem(pos);
                if (!topItem)
                {
                  clearSelection();
                  return;
                }

                if (!topItem->selected)
                {
                  clearSelection();
                  update(ActionGroupType::Selection, [this, pos] {
                    selectTopItem(pos);
                  });
                }

                _selection.startMove(pos);
                VME_LOG_D("Start move: " << pos);
                // VME_LOG_D("moveDelta: " << _selection.moveDelta());
              }
            },

            [this, pos, event](MouseAction::RawItem &action) {
              clearSelection();

              if (event.modifiers() & VME::ModifierKeys::Shift)
              {
                action.area = true;
              }
              else
              {
                bool remove = event.modifiers() & VME::ModifierKeys::Ctrl;

                if (remove)
                {
                  const Tile *tile = getTile(pos);
                  if (tile)
                  {
                    history.startGroup(ActionGroupType::RemoveMapItem);
                    removeItems(*tile, [action](const Item &item) { return item.serverId() == action.serverId; });
                    history.endGroup(ActionGroupType::RemoveMapItem);
                  }
                }
                else
                {
                  history.startGroup(ActionGroupType::AddMapItem);
                  addItem(pos, action.serverId);
                  history.endGroup(ActionGroupType::AddMapItem);
                }
              }
            },

            [this, event](MouseAction::Pan &pan) {
              pan.mouseOrigin = event.pos();
              pan.cameraOrigin = camera.worldPosition();
            },

            [](const auto &arg) {}},
        editorAction.action());

    setDragStart(mouseWorldPos());
  }

  requestDraw();
}

void MapView::mouseMoveEvent(VME::MouseEvent event)
{
  Position pos = event.pos().toPos(*this);

  bool needsDraw = pos != _drawRequestMousePos;
  _drawRequestMousePos = pos;

  if (!isDragging())
  {
    if (needsDraw)
      requestDraw();
    return;
  }

  auto dragPositions = getDragPoints().value();

  if (event.buttons() & VME::MouseButtons::LeftButton)
  {
    rollbear::visit(
        util::overloaded{
            [this, &pos](const MouseAction::Select) {
              if (hasSelectionMoveOrigin())
              {
                _selection.updateMoveDelta(pos);
              }
              // auto mousePos = mouseGamePos();
              // auto delta = _selection.moveOrigin.value() - mousePos;
              // auto correctionPos = _selection.topLeft().value() - delta;

              // _selection.outOfBoundCorrection.x = -std::min(correctionPos.x, 0);
              // _selection.outOfBoundCorrection.y = -std::min(correctionPos.y, 0);
            },

            [this, pos, event, dragPositions](const MouseAction::RawItem &action) {
              const auto [from, to] = dragPositions;
              if (action.area)
              {
                return;
              }
              if (event.pos().worldPos(*this) == to)
              {
                return;
              }

              if (event.modifiers() & VME::ModifierKeys::Ctrl)
              {
                const Tile *tile = getTile(pos);
                if (tile)
                {
                  history.startGroup(ActionGroupType::RemoveMapItem);

                  removeItems(*tile, [action](const Item &item) { return item.serverId() == action.serverId; });
                  history.endGroup(ActionGroupType::RemoveMapItem);
                }
              }
              else
              {
                history.startGroup(ActionGroupType::AddMapItem);
                for (const auto position : Position::bresenHams(to.toPos(floor()), pos))
                  addItem(position, action.serverId);
                history.endGroup(ActionGroupType::AddMapItem);
              }
            },

            [this, event](MouseAction::Pan &action) {
              if (action.active())
              {
                ScreenPosition delta = event.pos() - action.mouseOrigin.value();
                auto newX = static_cast<WorldPosition::value_type>(std::round(delta.x / camera.zoomFactor()));
                auto newY = static_cast<WorldPosition::value_type>(std::round(delta.y / camera.zoomFactor()));

                auto newPosition = action.cameraOrigin.value() + WorldPosition(-newX, -newY);

                if (newPosition.x < 0)
                {
                  action.cameraOrigin.value().x -= static_cast<int>(std::round((newPosition.x)));
                  newPosition.x = 0;
                }
                if (newPosition.y < 0)
                {
                  action.cameraOrigin.value().y -= static_cast<int>(std::round((newPosition.y)));
                  newPosition.y = 0;
                }

                camera.setWorldPosition(newPosition);
              }
            },

            [](const auto &arg) {}},
        editorAction.action());
  }

  setDragEnd(event.pos().worldPos(*this));

  requestDraw();
}

void MapView::mouseReleaseEvent(VME::MouseEvent event)
{
  Position pos = event.pos().toPos(*this);
  VME_LOG_D("MapView::mouseReleaseEvent: " << pos);

  if (!(event.buttons() & VME::MouseButtons::LeftButton))
  {
    std::visit(
        util::overloaded{
            [](MouseAction::Pan pan) {
              pan.stop();
            },
            [](const auto &arg) {}},
        editorAction.action());

    if (dragRegion.has_value())
    {
      endDragging(event.modifiers());
    }

    if (_selection.moveOrigin.has_value())
    {
      if (_selection.moveDelta.has_value() && _selection.moveDelta.value() != PositionConstants::Zero)
      {
        waitForDraw([this] { finishMoveSelection(); });
      }
      else
      {
        _selection.endMove();
      }
    }
  }

  requestDraw();
}

void MapView::waitForDraw(std::function<void()> f)
{
  uiUtils->waitForDraw(f);
}

void MapView::escapeEvent()
{
  std::visit(
      util::overloaded{
          [this](MouseAction::Select &) {
            clearSelection();
            requestDraw();
          },

          [this](const auto &arg) {
            editorAction.reset();
            requestDraw();
          }},
      editorAction.action());
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>Camera related>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/

void MapView::setX(WorldPosition::value_type x)
{
  camera.setX(x);
}

void MapView::setY(WorldPosition::value_type y)
{
  camera.setY(y);
}

void MapView::zoom(int delta)
{
  // TODO Handle the value of the zoom delta, not just its sign
  switch (util::sgn(delta))
  {
  case -1:
    zoomOut();
    break;
  case 1:
    zoomIn();
    break;
  default:
    break;
  }

  requestDraw();
}

void MapView::zoomOut()
{
  camera.zoomOut(mousePos());
}

void MapView::zoomIn()
{
  camera.zoomIn(mousePos());
}

void MapView::resetZoom()
{
  camera.resetZoom(mousePos());
}

float MapView::getZoomFactor() const noexcept
{
  return camera.zoomFactor();
}

void MapView::translateCamera(WorldPosition delta)
{
  camera.translate(delta);
}

void MapView::translateX(WorldPosition::value_type x)
{
  camera.setX(camera.x() + x);
}

void MapView::translateY(WorldPosition::value_type y)
{
  camera.setY(camera.y() + y);
}

void MapView::translateZ(int z)
{
  camera.translateZ(z);
}

void MapView::requestDraw()
{
  drawRequest.fire();
}

void MapView::setUnderMouse(bool underMouse)
{
  bool prev = _underMouse;

  _underMouse = underMouse;

  if (prev != underMouse)
  {
    requestDraw();
  }
}
