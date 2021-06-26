#include "item_palette_model.h"

#include <QPainter>

#include "../../debug.h"
#include "../../items.h"
#include "../../tileset.h"
#include "../gui_thing_image.h"
#include "../qt_util.h"

#include "../../brushes/creature_brush.h"
#include "../../brushes/doodad_brush.h"
#include "../../brushes/ground_brush.h"
#include "../../brushes/raw_brush.h"

using TilesetModel = ItemPaletteUI::TilesetModel;
// using ModelItem = ItemPaletteUI::ModelItem;
using ItemDelegate = ItemPaletteUI::ItemDelegate;
using HighlightAnimation = ItemPaletteUI::HighlightAnimation;

TilesetModel::TilesetModel(QObject *parent)
    : QAbstractListModel(parent), highlightAnimation(this)
{
}

void TilesetModel::setTileset(Tileset *tileset)
{
    beginResetModel();
    _tileset = tileset;
    endResetModel();
}

void TilesetModel::highlightIndex(const QModelIndex &modelIndex)
{
    highlightAnimation.runOnIndex(modelIndex);
}

int TilesetModel::rowCount(const QModelIndex &parent) const
{
    return _tileset ? static_cast<int>(_tileset->size()) : 0;
}

Brush *TilesetModel::brushAtIndex(size_t index) const
{
    return _tileset ? _tileset->get(index) : nullptr;
}

Tileset *TilesetModel::tileset() const noexcept
{
    return _tileset;
}

void TilesetModel::clear()
{
    setTileset(nullptr);
}

QVariant TilesetModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= _tileset->size())
        return QVariant();

    if (role == BrushRole)
    {
        Brush *brush = _tileset->get(index.row());
        return QVariant::fromValue(brush);
    }
    else if (role == TilesetModel::HighlightRole)
    {
        if (QPersistentModelIndex(index) == highlightAnimation.index)
        {
            return highlightAnimation.currentValue();
        }
    }

    return QVariant();
}

ItemDelegate::ItemDelegate(QObject *parent)
    : QAbstractItemDelegate(parent), color("#2196F3")
{
    highlightBorderPen.setWidth(4);
    setHighlightPenOpacity(1.0f);
}

void ItemDelegate::setHighlightPenOpacity(float opacity) const
{
    color.setAlphaF(opacity);
    highlightBorderPen.setColor(color);
}

void ItemDelegate::paintTextureArea(QPainter *painter, const QPoint topLeft, const QtTextureArea &textureArea) const
{
    QImage image = textureArea.image->copy(textureArea.rect).mirrored();
    if (textureArea.rect.width() > 32 || textureArea.rect.height() > 32)
    {
        image = image.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    painter->drawImage(topLeft, image);
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // TODO Add option for rendering as a list with names
    // painter->drawText(40, option.rect.y() + 18, QString::number(data.itemType->id));
    // painter->drawPixmap(4, option.rect.y(), data.pixmap);
    // qDebug() << "Delegate::paint: " << option.rect;

    QPoint topLeft = option.rect.topLeft();

    Brush *b = qvariant_cast<Brush *>(index.data(TilesetModel::BrushRole));

    switch (b->type())
    {
        case BrushType::Raw:
        {
            auto brush = static_cast<RawBrush *>(b);
            QtTextureArea textureArea = GUIThingImage::getItemTypeTexture(*brush->itemType());
            paintTextureArea(painter, topLeft, textureArea);
            break;
        }
        case BrushType::Ground:
        {
            auto brush = static_cast<GroundBrush *>(b);
            QtTextureArea textureArea = GUIThingImage::getItemTypeTexture(brush->iconServerId());
            paintTextureArea(painter, topLeft, textureArea);
            break;
        }
        case BrushType::Doodad:
        {
            auto brush = static_cast<DoodadBrush *>(b);
            QtTextureArea textureArea = GUIThingImage::getItemTypeTexture(brush->iconServerId());
            paintTextureArea(painter, topLeft, textureArea);
            break;
        }
        case BrushType::Creature:
        {
            auto brush = static_cast<CreatureBrush *>(b);
            // std::vector<QtTextureArea> textureArea = GUIThingImage::getCreatureTypeTextures(*brush->creatureType, Direction::South);
            QtTextureArea textureArea = GUIThingImage::getCreatureTypeTextures(*brush->creatureType, Direction::South);
            paintTextureArea(painter, topLeft, textureArea);

            // for (const auto &area : textureArea)
            // {
            //     paintTextureArea(painter, topLeft, area);
            // }
            break;
        }
        default:
            break;
    }

    bool ok;
    int highlightOpacity = index.data(TilesetModel::HighlightRole).toInt(&ok);
    if (ok && highlightOpacity > 0)
    {
        painter->save();
        setHighlightPenOpacity(highlightOpacity / 100.0f);
        painter->setPen(highlightBorderPen);

        painter->drawRect(QRect(option.rect.x(), option.rect.y(), 32, 32));
        painter->restore();
    }
}

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return QSize(32, 32);
}

HighlightAnimation::HighlightAnimation(TilesetModel *model, QObject *parent)
    : model(model)
{
    setKeyValueAt(0, 0);
    setKeyValueAt(0.25f, 100);
    setKeyValueAt(0.5f, 0);
    setKeyValueAt(0.75f, 100);
    setKeyValueAt(1.0f, 0);

    // setStartValue(100);
    // setEndValue(0);

    setDuration(2000);
    connect(this, &HighlightAnimation::valueChanged, [=](const QVariant &value) {
        model->dataChanged(index, index, QList{TilesetModel::HighlightRole});
    });
}

void HighlightAnimation::runOnIndex(const QModelIndex &modelIndex)
{
    if (state() == State::Running)
    {
        stop();
    }
    index = QPersistentModelIndex(modelIndex);

    start();
}
