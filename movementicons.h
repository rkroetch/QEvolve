#ifndef MOVEMENTICONS_H
#define MOVEMENTICONS_H

#include <QPixmap>
#include <QString>
#include "common.h"

QPixmap movementIcon(MovementDirections direction);
QString movementToolTip(MovementDirections direction);

#endif // MOVEMENTICONS_H
