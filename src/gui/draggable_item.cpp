#include "draggable_item.h"

#include <QWindow>

#include "../main.h"
#include "../qt/logging.h"
#include "../tile.h"
#include "../util.h"
#include "qt_util.h"

//>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>ItemDrag::DragOperation>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>

ItemDrag::DragOperation::DragOperation(ItemDrag::MimeData &&mimeData, QWindow *parent)
    : _parent(parent),
      _hoveredObject(QApplication::widgetAt(QCursor::pos())),
      mimeData(std::move(mimeData)),
      pixmap(this->mimeData.draggableItem->pixmap()),
      renderingCursor(false)
{
  if (_hoveredObject)
  {
    auto pos = static_cast<QWidget *>(_hoveredObject)->mapFromGlobal(QCursor::pos());
    sendDragEnterEvent(_hoveredObject, pos);
  }
}

ItemDrag::DragOperation::DragOperation(DragOperation &&other) noexcept
    : _parent(std::move(other._parent)),
      _hoveredObject(std::move(other._hoveredObject)),
      pixmap(std::move(other.pixmap)),
      shouldRender(std::move(other.shouldRender)),
      onDropRejected(std::move(other.onDropRejected)),
      renderingCursor(std::move(other.renderingCursor)),
      mimeData(std::move(other.mimeData))
{
}

ItemDrag::DragOperation &ItemDrag::DragOperation::operator=(DragOperation &&other) noexcept
{
  _parent = std::move(other._parent);
  _hoveredObject = std::move(other._hoveredObject);
  pixmap = std::move(other.pixmap);
  shouldRender = std::move(other.shouldRender);
  onDropRejected = std::move(other.onDropRejected);
  renderingCursor = std::move(other.renderingCursor);
  mimeData = std::move(other.mimeData);

  return *this;
}

void ItemDrag::DragOperation::start()
{
  VME_LOG_D("ItemDrag::DragOperation::start()");
}

void ItemDrag::DragOperation::showCursor()
{
  if (!renderingCursor)
  {
    QApplication::setOverrideCursor(pixmap);
    renderingCursor = true;
  }
}

void ItemDrag::DragOperation::hideCursor()
{
  if (renderingCursor)
  {
    QApplication::restoreOverrideCursor();
    renderingCursor = false;
  }
}

void ItemDrag::DragOperation::finish()
{
  hideCursor();
}

void ItemDrag::DragOperation::setRenderCondition(std::function<bool()> f)
{
  shouldRender = f;
}

bool ItemDrag::DragOperation::mouseMoveEvent(QMouseEvent *event)
{
  if (shouldRender())
  {
    showCursor();
  }
  else
  {
    hideCursor();
  }

  auto widget = QtUtil::qtApp()->widgetAt(QCursor::pos());
  auto object = static_cast<QObject *>(widget);

  auto pos = widget->mapFromGlobal(_parent->mapToGlobal(event->pos()));

  bool changed = object != _hoveredObject;
  if (changed)
  {
    sendDragLeaveEvent(_hoveredObject, pos, event);
    sendDragEnterEvent(object, pos, event);

    setHoveredObject(object);
  }
  else
  {
    sendDragMoveEvent(object, pos, event);
  }

  return changed;
}

bool ItemDrag::DragOperation::sendDropEvent(QMouseEvent *event)
{
  auto widget = QtUtil::qtApp()->widgetAt(QCursor::pos());
  auto pos = widget->mapFromGlobal(_parent->mapToGlobal(event->pos()));

  bool accepted = sendDragDropEvent(widget, pos, event);

  finish();

  return accepted;
}

void ItemDrag::DragOperation::setHoveredObject(QObject *object)
{
  qDebug() << "setHoveredObject: " << _hoveredObject;

  _hoveredObject = object;
}

QObject *ItemDrag::DragOperation::hoveredObject() const
{
  return _hoveredObject;
}

ItemDrag::MimeData::MimeData(MimeData &&other) noexcept
    : draggableItem(std::move(other.draggableItem)) {}

ItemDrag::MimeData &ItemDrag::MimeData::operator=(MimeData &&other) noexcept
{
  draggableItem = std::move(other.draggableItem);
  return *this;
}

QVariant ItemDrag::MimeData::retrieveData(const QString &mimeType, QVariant::Type type) const
{
  QVariant data;
  if (mimeType != ItemDrag::DragOperation::MapItemFormat)
  {
    VME_LOG_D("ItemDrag::DragOperation does not accept mimeType: " << mimeType);
    return data;
  }

  /*
      The data must be QByteArray. Otherwise QML can't read it.
      It seems like QML does not perform an implicit conversion from QVariant(X)
      to QVariant(QByteArray)).
    */
  data.setValue(draggableItem->serialize());

  return data;
}

void ItemDrag::DragOperation::sendDragEnterEvent(QObject *object, QPoint position)
{
  if (!object)
    return;

  QDragEnterEvent dragEnterEvent(position, Qt::DropAction::MoveAction, &mimeData, QApplication::mouseButtons(), QApplication::keyboardModifiers());
  QApplication::sendEvent(object, &dragEnterEvent);
}

void ItemDrag::DragOperation::sendDragEnterEvent(QObject *object, QPoint position, QMouseEvent *event)
{
  if (!object)
    return;

  QDragEnterEvent dragEnterEvent(position, Qt::DropAction::MoveAction, &mimeData, event->buttons(), event->modifiers());
  QApplication::sendEvent(object, &dragEnterEvent);
}

void ItemDrag::DragOperation::sendDragLeaveEvent(QObject *object, QPoint position, QMouseEvent *event)
{
  if (!object)
    return;

  QDragLeaveEvent dragLeaveEvent;
  QApplication::sendEvent(object, &dragLeaveEvent);
}

void ItemDrag::DragOperation::sendDragMoveEvent(QObject *object, QPoint position, QMouseEvent *event)
{
  if (!object)
    return;

  QDragMoveEvent dragMoveEvent(position, Qt::DropAction::MoveAction, &mimeData, event->buttons(), event->modifiers());
  QApplication::sendEvent(object, &dragMoveEvent);
}

bool ItemDrag::DragOperation::sendDragDropEvent(QObject *object, QPoint position, QMouseEvent *event)
{
  if (!object)
    return false;

  QDropEvent dropEvent(position, Qt::DropAction::MoveAction, &mimeData, event->buttons(), event->modifiers());
  // QDropEvent dropEvent(position, Qt::DropAction::MoveAction, nullptr, event->buttons(), event->modifiers());
  bool accepted = QApplication::sendEvent(object, &dropEvent);
  return accepted;
}

ItemDrag::MapItem::MapItem() : MapItem(nullptr, nullptr, nullptr) {}

ItemDrag::MapItem::MapItem(MapView *mapView, Tile *tile, Item *item)
    : mapView(mapView), tile(tile), _item(item)
{
}

void ItemDrag::MapItem::remove()
{
  mapView->removeItem(*tile, _item);
}

Item ItemDrag::MapItem::moveFromMap()
{
  return mapView->dropItem(tile, _item);
}

std::optional<ItemDrag::MapItem> ItemDrag::MapItem::fromDataStream(QDataStream &dataStream)
{
  MapItem mapItem;

  dataStream >> mapItem;

  if (mapItem.mapView == nullptr || mapItem.tile == nullptr || mapItem._item == nullptr)
    return std::nullopt;

  return mapItem;
}

Item *ItemDrag::MapItem::item() const
{
  return _item;
}

QPixmap ItemDrag::MapItem::pixmap() const
{
  return QtUtil::itemPixmap(tile->position(), *_item);
}

QDataStream &ItemDrag::MapItem::serializeInto(QDataStream &dataStream) const
{
  return dataStream << (*this);
}

QPixmap ItemDrag::ContainerItem::pixmap() const
{
  return QtUtil::itemPixmap(Position(0, 0, 7), container->itemAt(index));
}

void ItemDrag::ContainerItem::remove()
{
  container->removeItem(item());
}

Item *ItemDrag::ContainerItem::item() const
{
  return &container->itemAt(index);
}

QDataStream &ItemDrag::ContainerItem::serializeInto(QDataStream &dataStream) const
{
  return dataStream << (*this);
}

std::optional<ItemDrag::ContainerItem> ItemDrag::ContainerItem::fromDataStream(QDataStream &dataStream)
{
  ContainerItem containerItem;
  dataStream >> containerItem;

  if (containerItem.container == nullptr || containerItem.index >= containerItem.container->size())
    return std::nullopt;

  return containerItem;
}

ItemDrag::MimeData::MimeData(std::unique_ptr<DraggableItem> &&draggableItem)
    : QMimeData(), draggableItem(std::move(draggableItem))
{
  VME_LOG_D("ItemDrag::MimeData::MimeData");
}

// QDataStream &ItemDrag::DragOperation::MimeData::operator<<(QDataStream &out, const ItemDrag::DragOperation::MimeData::RawData &rawData)
// {
//   out << rawData.tile;
//   out << rawData.item;
//   return out;
// }

// QDataStream &ItemDrag::DragOperation::MimeData::operator>>(QDataStream &in, ItemDrag::DragOperation::MimeData::RawData &rawData)
// {
// //in >> rawData.tile;
// //in >> rawData.item;
//   return in;
// }

bool ItemDrag::MimeData::hasFormat(const QString &mimeType) const
{
  return mimeType == mapItemMimeType();
}

QStringList ItemDrag::MimeData::formats() const
{
  return QStringList() << mapItemMimeType();
}

QByteArray ItemDrag::DraggableItem::serialize() const
{
  QByteArray byteArray;
  QDataStream dataStream(&byteArray, QIODevice::WriteOnly);

  int metaType = to_underlying(type());

  dataStream << metaType;
  serializeInto(dataStream);

  return byteArray;
}

std::unique_ptr<ItemDrag::DraggableItem> ItemDrag::DraggableItem::deserialize(QByteArray &array)
{
  QDataStream dataStream(&array, QIODevice::ReadOnly);
  int metaType;
  dataStream >> metaType;

  switch (static_cast<DraggableItem::Type>(metaType))
  {
  case DraggableItem::Type::MapItem:
    return moveToHeap(MapItem::fromDataStream(dataStream));
  case DraggableItem::Type::ContainerItem:
    return moveToHeap(ContainerItem::fromDataStream(dataStream));
  default:
    return std::unique_ptr<DraggableItem>{};
  }
}

Item ItemDrag::DraggableItem::copy() const
{
  return item()->deepCopy();
}

QDataStream &operator<<(QDataStream &dataStream, const ItemDrag::MapItem &mapItem)
{
  dataStream << util::pointerAddress(mapItem.mapView);
  dataStream << util::pointerAddress(mapItem.tile);
  dataStream << util::pointerAddress(mapItem.item());

  return dataStream;
}

QDataStream &operator>>(QDataStream &dataStream, ItemDrag::MapItem &mapItem)
{
  mapItem.mapView = QtUtil::readPointer<MapView *>(dataStream);
  mapItem.tile = QtUtil::readPointer<Tile *>(dataStream);
  mapItem._item = QtUtil::readPointer<Item *>(dataStream);

  return dataStream;
}

QDataStream &operator<<(QDataStream &dataStream, const ItemDrag::ContainerItem &containerItem)
{
  dataStream << util::pointerAddress(containerItem.container);
  dataStream << containerItem.index;

  return dataStream;
}

QDataStream &operator>>(QDataStream &dataStream, ItemDrag::ContainerItem &containerItem)
{
  containerItem.container = QtUtil::readPointer<ItemData::Container *>(dataStream);
  dataStream >> containerItem.index;

  return dataStream;
}