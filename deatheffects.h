#ifndef DEATHEFFECTS_H
#define DEATHEFFECTS_H

#include <QPointF>
#include <QColor>
#include <QVector>
#include <QMutex>

// Thread-safe drop box for animal-death UI events. Animal::killSelf() runs
// from CalculationThread's worker pool (one worker per species, potentially
// in parallel), so this only needs to collect positions/colors safely.
// Laboratory drains it once per cycle from captureFrame() and turns each
// entry into a timed animation for paintGL() to draw.
struct DeathEvent
{
    QPointF pos;
    QColor color;
};

class DeathEffects
{
public:
    static void notify(const QPointF & pos, const QColor & color);
    static QVector<DeathEvent> takeAll();

private:
    static QMutex sMutex;
    static QVector<DeathEvent> sEvents;
};

#endif // DEATHEFFECTS_H
