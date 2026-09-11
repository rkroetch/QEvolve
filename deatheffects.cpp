#include "deatheffects.h"
#include <QMutexLocker>

QMutex DeathEffects::sMutex;
QVector<DeathEvent> DeathEffects::sEvents;

void DeathEffects::notify(const QPointF & pos, const QColor & color)
{
    QMutexLocker locker(&sMutex);
    sEvents.append({pos, color});
}

QVector<DeathEvent> DeathEffects::takeAll()
{
    QMutexLocker locker(&sMutex);
    QVector<DeathEvent> result;
    result.swap(sEvents);
    return result;
}
