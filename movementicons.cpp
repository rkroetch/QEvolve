#include "movementicons.h"

#include <QPainter>

namespace {

// The bundled action icons (resources/actions/*.png) are modern flat-shaded
// blue/red arrows that clash with the DB16 pixel-art theme applied
// everywhere else (see resources/theme.qss) - they're minor/secondary UI
// elements (direction hints on buttons/labels), so rather than hand-author
// new sprite art (no image-generation tool is available this session), they
// get tinted to a single DB16 ink color here. Cheap: a SourceIn composite
// over the existing pixmap's own alpha shape, so the arrow silhouettes are
// unchanged and no new artwork is needed.
QPixmap tinted(const QPixmap & source, const QColor & color)
{
    if (source.isNull())
    {
        return source;
    }
    QPixmap result(source.size());
    result.setDevicePixelRatio(source.devicePixelRatio());
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.drawPixmap(0, 0, source);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(result.rect(), color);
    painter.end();
    return result;
}

QPixmap rawMovementIcon(MovementDirections direction)
{
    switch (direction)
    {
    case MoveUp:
        return QPixmap(":actions/resources/actions/arrow_up.png");
    case MoveUpRight:
        return QPixmap(":actions/resources/actions/arrow_right_up.png");
    case MoveRight:
        return QPixmap(":actions/resources/actions/arrow_right.png");
    case MoveDownRight:
        return QPixmap(":actions/resources/actions/arrow_right_down.png");
    case MoveDown:
        return QPixmap(":actions/resources/actions/arrow_down.png");
    case MoveDownLeft:
        return QPixmap(":actions/resources/actions/arrow_left_down.png");
    case MoveLeft:
        return QPixmap(":actions/resources/actions/arrow_left.png");
    case MoveUpLeft:
        return QPixmap(":actions/resources/actions/arrow_left_up.png");
    case MoveRandom:
        return QPixmap(":actions/resources/actions/arrow_refresh_small.png");
    case MoveStop:
        return QPixmap(":actions/resources/actions/stop.png");
    case MoveGo:
        return QPixmap(":actions/resources/actions/arrow_out.png");
    case MoveTurnAround:
        return QPixmap(":actions/resources/actions/arrow_undo.png");
    case MoveTurnRight:
        return QPixmap(":actions/resources/actions/arrow_turn_right.png");
    case MoveTurnLeft:
        return QPixmap(":actions/resources/actions/arrow_turn_left.png");
    case MoveMerge:
        return QPixmap(":actions/resources/actions/arrow_merge.png");
    case MoveSplit:
        return QPixmap(":actions/resources/actions/arrow_branch.png");
    default:
        return QPixmap();
    }
}

} // namespace

QPixmap movementIcon(MovementDirections direction)
{
    // DB16 "light gray" (#d2d2d2) - the same tone resources/theme.qss uses
    // for body text, so these icons read as ink marks alongside the labels
    // they sit next to rather than as separately-colored artwork.
    static const QColor kIconTint(0xd2, 0xd2, 0xd2);
    return tinted(rawMovementIcon(direction), kIconTint);
}

QString movementToolTip(MovementDirections direction)
{
    switch (direction)
    {
    case MoveMerge:
        return QString("Merge");
    case MoveSplit:
        return QString("Split");
    case MoveUp:
        return QString("Up");
    case MoveUpRight:
        return QString("Up Right");
    case MoveRight:
        return QString("Right");
    case MoveDownRight:
        return QString("Down Right");
    case MoveDown:
        return QString("Down");
    case MoveDownLeft:
        return QString("Down Left");
    case MoveLeft:
        return QString("Left");
    case MoveUpLeft:
        return QString("Up Left");
    case MoveRandom:
        return QString("Random");
    case MoveStop:
        return QString("Stop");
    case MoveGo:
        return QString("Go");
    case MoveTurnAround:
        return QString("Turn Around");
    case MoveTurnRight:
        return QString("Turn Right");
    case MoveTurnLeft:
        return QString("Turn Left");
    default:
        return QString("");
    }
}
