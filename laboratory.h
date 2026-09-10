#ifndef LABORATORY_H
#define LABORATORY_H

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QPaintEvent>
#include <QList>
#include <QFuture>
#include <QReadWriteLock>
#include <QThread>
#include <Windows.h>

#include <gl/GL.h>

enum PlantPattern
{
    plantPatternOneGroup,
    plantPatternTwoGroups,
    plantPatternRandom
};

class Animal;
class Species;
class CalculationThread;

constexpr int LABORATORY_WIDTH = 360;
constexpr int LABORATORY_HEIGHT = 360;
constexpr int MAX_NUM_PLANTS = 2000;
constexpr int PLANT_INITIAL_ENERGY = 500;
constexpr int PLANT_SPAWN_ENERGY = 1000;

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
    int speed() const { return mSpeed; }
    int numAnimals() const;
    QString statistics() const;
    double cyclesPerSecond();

    PlantPattern plantPattern() const { return mSettings.plantPattern; }

public slots:
    //0 - 100, 0 being fastest
    void setSpeed(int speed) { mSpeed = speed; }
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
    int mSpeed;

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
    bool mStop;
    QMutex mStopMutex;
    QMutex mDataMutex;
    qint64 mNumCycles = 0;
    qint64 mLastNumCycles = 0;
    QElapsedTimer mPerformanceTimer;
    qint64 mCounts = 0;
    qint64 mCountsPerSecond = 0;
//    LARGE_INTEGER mCounts = {{0, 0}};
//    LARGE_INTEGER mCountsPerSecond = {{0, 0}};
};

#endif // LABORATORY_H
