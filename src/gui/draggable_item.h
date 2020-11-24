#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <QByteArray>
#include <QDebug>
#include <QMimeData>
#include <QPixmap>
#include <QPoint>

class QWindow;
class QObject;
class QMouseEvent;
class MapView;
class Tile;
class Item;
namespace ItemData
{
  struct Container;
}

namespace ItemDrag
{
  struct DraggableItem
  {
    enum class Type
    {
      MapItem,
      ContainerItem
    };
    ~DraggableItem() {}

    virtual Item *item() const = 0;
    virtual void remove() = 0;
    virtual const Type type() const noexcept = 0;
    virtual QPixmap pixmap() const = 0;
    Item copy() const;

    static bool validType(int value);

    static std::unique_ptr<DraggableItem> deserialize(QByteArray &array);

    QByteArray serialize() const;
    virtual QDataStream &serializeInto(QDataStream &dataStream) const = 0;

    // protected:
    // virtual std::unique_ptr<DraggableItem> moveToHeap() = 0;
  protected:
  private:
    template <typename T, typename std::enable_if<std::is_base_of<DraggableItem, T>::value>::type * = nullptr>
    static std::unique_ptr<DraggableItem> moveToHeap(std::optional<T> value);
  };

  struct MapItem : DraggableItem
  {
    MapItem();
    MapItem(MapView *mapView, Tile *tile, Item *item);

    const Type type() const noexcept override
    {
      return Type::MapItem;
    }

    Item *item() const override;
    void remove() override;
    QPixmap pixmap() const override;

    Item moveFromMap();
    static std::optional<MapItem> fromDataStream(QDataStream &dataStream);

    bool operator==(const MapItem &other) const
    {
      return mapView == other.mapView && tile == other.tile && _item == other._item;
    }

    MapView *mapView;
    Tile *tile;
    Item *_item;

  protected:
    QDataStream &serializeInto(QDataStream &dataStream) const override;
  };

  struct ContainerItem : DraggableItem
  {
    Item *item() const override;
    void remove() override;
    QPixmap pixmap() const override;

    static std::optional<ContainerItem> fromDataStream(QDataStream &dataStream);

    const Type type() const noexcept override
    {
      return Type::ContainerItem;
    }

    ItemData::Container *container;
    size_t index;

  protected:
    QDataStream &serializeInto(QDataStream &dataStream) const override;
  };

  /*
  Defines the available MimeData for a drag & drop operation on a map tab.
*/
  class MimeData : public QMimeData
  {
  public:
    MimeData(MimeData &&other) noexcept;
    MimeData &operator=(MimeData &&other) noexcept;

    template <typename T, typename std::enable_if<std::is_base_of<DraggableItem, T>::value>::type * = nullptr>
    static MimeData create(T &&t)
    {
      auto result = MimeData(std::make_unique<T>(std::move(t)));
      return result;
    }

    static const QString mapItemMimeType()
    {
      static const QString mimeType = "vulkan-game-editor-mimetype:map-item";
      return mimeType;
    }

    bool hasFormat(const QString &mimeType) const override;
    QStringList formats() const override;

    QVariant retrieveData(const QString &mimeType, QVariant::Type type) const override;

    std::unique_ptr<DraggableItem> draggableItem;

  private:
    MimeData(std::unique_ptr<DraggableItem> &&draggableItem);
  };

  class DragOperation
  {
  public:
    static constexpr auto MapItemFormat = "vulkan-game-editor-mimetype:map-item";

    DragOperation(DragOperation &&other) noexcept;
    DragOperation &operator=(DragOperation &&other) noexcept;

    template <typename T, typename std::enable_if<std::is_base_of<DraggableItem, T>::value>::type * = nullptr>
    static DragOperation create(T &&t, QWindow *parent)
    {
      return DragOperation(MimeData::create(std::move(t)), parent);
    }

    void setRenderCondition(std::function<bool()> f);

    void start();
    void finish();
    bool isDragging() const;
    bool mouseMoveEvent(QMouseEvent *event);
    bool sendDropEvent(QMouseEvent *event);
    QObject *hoveredObject() const;

    DragOperation::MimeData mimeData;

  private:
    DragOperation(MimeData &&mimeData, QWindow *parent);

    void sendDragEnterEvent(QObject *object, QPoint position);
    void sendDragEnterEvent(QObject *object, QPoint position, QMouseEvent *event);
    void sendDragLeaveEvent(QObject *object, QPoint position, QMouseEvent *event);
    void sendDragMoveEvent(QObject *object, QPoint position, QMouseEvent *event);
    bool sendDragDropEvent(QObject *object, QPoint position, QMouseEvent *event);

    void setHoveredObject(QObject *object);

    void showCursor();
    void hideCursor();

    QWindow *_parent;
    QObject *_hoveredObject;

    QPixmap pixmap;

    std::function<bool()> shouldRender = [] { return true; };
    std::function<void()> onDropRejected = [] {};

    bool renderingCursor;
  };

} // namespace ItemDrag

inline QDebug operator<<(QDebug &dbg, const ItemDrag::MapItem &mapItem)
{
  dbg.nospace() << "MapItem { mapView: " << mapItem.mapView << ", tile: " << mapItem.tile << ", item:" << mapItem._item << "}";
  return dbg.maybeSpace();
}

QDataStream &operator<<(QDataStream &, const ItemDrag::MapItem &);
QDataStream &operator>>(QDataStream &, ItemDrag::MapItem &);

QDataStream &operator<<(QDataStream &, const ItemDrag::ContainerItem &);
QDataStream &operator>>(QDataStream &, ItemDrag::ContainerItem &);

QDataStream &operator<<(QDataStream &, const ItemDrag::DraggableItem &);
QDataStream &operator>>(QDataStream &, ItemDrag::DraggableItem &);

template <typename T, typename std::enable_if<std::is_base_of<ItemDrag::DraggableItem, T>::value>::type *>
static std::unique_ptr<ItemDrag::DraggableItem> ItemDrag::DraggableItem::moveToHeap(std::optional<T> value)
{
  return value ? std::make_unique<T>(value.value()) : std::unique_ptr<DraggableItem>{};
}

// Q_DECLARE_METATYPE(DragOperation::MapItem);
// Q_DECLARE_METATYPE(DragOperation::ContainerItem);