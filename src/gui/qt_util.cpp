#include "qt_util.h"

#include <optional>
#include <memory>
#include <algorithm>

#include <QApplication>
#include <QWheelEvent>
#include <QSize>
#include <QLabel>
#include <QPainter>

#include "../items.h"
#include "../map_view.h"

#include "../logger.h"
#include "../qt/logging.h"

namespace
{
  std::unique_ptr<QPixmap> blackSquare;

  QPixmap blackSquarePixmap()
  {
    if (!blackSquare)
    {
      QImage image(32, 32, QImage::Format::Format_ARGB32);
      image.fill(QColor(0, 0, 0, 255));

      QPixmap pixmap = QPixmap::fromImage(image);

      blackSquare = std::make_unique<QPixmap>(std::move(pixmap));
    }

    return *blackSquare;
  }

} // namespace

QPixmap QtUtil::itemPixmap(uint16_t serverId)
{
  if (!Items::items.validItemType(serverId))
  {
    return blackSquarePixmap();
  }

  ItemType *t = Items::items.getItemType(serverId);
  auto &info = t->getTextureInfoUnNormalized();

  TextureAtlas *atlas = info.atlas;

  const uint8_t *pixelData = atlas->getOrCreateTexture().pixels().data();

  QRect textureRegion(info.window.x0, info.window.y0, info.window.x1, info.window.y1);

  QImage sprite = QImage(pixelData, 12 * 32, 12 * 32, 384 * 4, QImage::Format::Format_ARGB32)
                      .copy(textureRegion)
                      .mirrored();

  if (atlas->spriteWidth == 32 && atlas->spriteHeight == 32)
  {
    return QPixmap::fromImage(sprite);
  }
  else
  {
    return QPixmap::fromImage(sprite.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }

  return QPixmap::fromImage(sprite);
}

MainApplication *QtUtil::qtApp()
{
  return (MainApplication *)(QApplication::instance());
}

std::optional<int> QtUtil::ScrollState::scroll(QWheelEvent *event)
{
  // The relative amount that the wheel was rotated, in eighths of a degree.
  const int deltaY = event->angleDelta().y();
  amountBuffer += deltaY;

  if (std::abs(amountBuffer) < minRotationDelta)
    return {};

  int result = amountBuffer / QtMinimumWheelDelta;
  amountBuffer = amountBuffer % QtMinimumWheelDelta;

  return result;
}

MapView *QtUtil::associatedMapView(QWidget *widget)
{
  QVariant prop = widget->property(QtUtil::PropertyName::MapView);
  if (prop.isNull())
    return nullptr;

  MapView *mapView = static_cast<MapView *>(prop.value<void *>());
#ifdef __DEBUG__VME
  bool isInstance = MapView::isInstance(mapView);

  std::ostringstream msg;
  msg << "The property QtUtil::PropertyName::MapView for widget " << widget << " must contain a MapView pointer.";
  DEBUG_ASSERT(isInstance, msg.str());

  return mapView;
#else
  return MapView::isInstance(mapView) ? mapView : nullptr;
#endif
}