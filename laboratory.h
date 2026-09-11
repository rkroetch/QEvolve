#ifndef LABORATORY_H
#define LABORATORY_H

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QPaintEvent>
#include <QList>
#include <QFuture>
#include <QReadWriteLock>
#include <QMutex>
#include <QThread>
#include <QVector>
#include <QAtomicInteger>
#include <QColor>
#include <atomic>
#include <Windows.h>

#include <gl/GL.h>
#include "common.h"

enum PlantPattern
{
    plantPatternOneGroup,
    plantPatternTwoGroups,
    plantPatternRandom
};

class Animal;
class Species;
class CalculationThread;

struct PaintQuad
{
    GLfloat r = 0;
    GLfloat g = 0;
    GLfloat b = 0;
    GLfloat x = 0;
    GLfloat y = 0;
};

// A death-effect animation in flight, keyed off mEffectsTimer so it can be
// aged/faded purely from elapsed time in paintGL().
struct DeathEffectAnim
{
    QPointF pos;
    QColor color;
    qint64 spawnMs = 0;
};

namespace Ui {
    class Laboratory;
}

class Laboratory : public QOpenGLWidget
{
    Q_OBJECT
public:
    struct Settings
    {
        PlantPattern plantPattern = plantPatternOneGroup;
    };

public:
    explicit Laboratory(QWidget *parent = nullptr);
    ~Laboratory();

    QList<Animal*> & animals();
    QList<Species*> & species();
    int speed() const { return mSpeed.loadRelaxed(); }
    int numAnimals() const;
    QString statistics() const;
    double cyclesPerSecond();
    void captureFrame();

    PlantPattern plantPattern() const { return mSettings.plantPattern; }

public slots:
    //0 being fastest
    void setSpeed(int speed) { mSpeed.storeRelaxed(speed); }
    void toggleStart();
    void start();
    void stop();
    void reset();
    void setPlantPattern(int pattern);

signals:
    void plantPatternChanged(PlantPattern pattern);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    int heightForWidth(int w) const override;

private:
    QList<Species *> loadSpecies();
    void initActors();
    QColor colorForIndex(int index) const;

private:
    Ui::Laboratory *ui;
    QList<Species *> mSpecies;
    Settings mSettings;
    QTimer mAdvanceTimer;
    QList<Animal*> mAnimals;
    QReadWriteLock mPositionLock;
    CalculationThread * mCalculationThread;
    QAtomicInteger<int> mSpeed = 0;
    mutable QMutex mPaintMutex;
    QVector<PaintQuad> mPaintSnapshot;
    QElapsedTimer mEffectsTimer;
    QVector<DeathEffectAnim> mDeathEffects;
    QAtomicInteger<int> mCachedNumAnimals = 0;
    QString mCachedStatistics;

    GLfloat mVertices[8 * 10000]{};
};

class CalculationThread : public QThread
{
public:
    explicit CalculationThread(Laboratory * laboratory, QObject * parent = nullptr);

    double cyclesPerSecond();
    void stop();
    void run() override;

private:
    Laboratory * mLaboratory;
    std::atomic<bool> mStop{false};
    QMutex mDataMutex;
    QVector<Animal *> mCalcAnimals;
    qint64 mNumCycles = 0;
    qint64 mLastNumCycles = 0;
    QElapsedTimer mPerformanceTimer;
    qint64 mCounts = 0;
    qint64 mCountsPerSecond = 0;
//    LARGE_INTEGER mCounts = {{0, 0}};
//    LARGE_INTEGER mCountsPerSecond = {{0, 0}};
};

#endif // LABORATORY_H
