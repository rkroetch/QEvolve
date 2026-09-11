#include "movementicons.h"

QPixmap movementIcon(MovementDirections direction)
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
