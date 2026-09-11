#include "laboratory.h"
#include "ui_laboratory.h"
#include "animal.h"
#include "species.h"
#include "delay.h"

#include <gl/GLU.h>
#include <gl/GL.h>
//#include <GL/glext.h>
#include "glext.h"

#include <Windows.h>

#include <QSettings>
#include <QPainter>
#include <QRectF>
#include <QPointF>
#include <QSizeF>
#include <QColor>
#include <QDir>
#include <QVector>
#include <QtConcurrent/QtConcurrentMap>
#include <QReadLocker>
#include <QWriteLocker>
#include <QThreadPool>
#include <QMutexLocker>
#include <QPair>

QReadWriteLock positionLock;

Laboratory::Laboratory(QWidget *parent) :
    QOpenGLWidget(parent),
    ui(new Ui::Laboratory)
{
    ui->setupUi(this);

    mSpeed = 0;

    setMinimumWidth(LABORATORY_WIDTH);
    setMinimumHeight(LABORATORY_HEIGHT);

    mSpecies = loadSpecies();

    QSettings settings("ryank", "Evolve", this);
    setPlantPattern(settings.value("Plants.Pattern", plantPatternOneGroup).value<PlantPattern>());

    auto * plants = new Species(Species::typePlant);
    plants->setColor(QColor(Qt::green));
    plants->setMetabolism(1.0);
    plants->setSpawningEnergy(PLANT_SPAWN_ENERGY);
    plants->setMovement(0, 0, MoveStop);
    plants->setMovement(1, 0, MoveStop);
    plants->setMovement(2, 0, MoveStop);
    plants->setMovement(0, 1, MoveStop);
    plants->setMovement(1, 1, MoveStop);
    plants->setMovement(2, 1, MoveStop);
    plants->setMovement(0, 2, MoveStop);
    plants->setMovement(1, 2, MoveStop);
    plants->setMovement(2, 2, MoveStop);
    mSpecies.append(plants);

    mCalculationThread = new CalculationThread(this);

    initActors();

    mAdvanceTimer.setInterval(20);
    connect(&mAdvanceTimer, &QTimer::timeout, this, QOverload<>::of(&Laboratory::update));
}

Laboratory::~Laboratory()
{
    // CalculationThread / QtConcurrent may still be touching Species and Animal
    // objects. Stop and wait before deleting them, or closing the window crashes.
    stop();
    QThreadPool::globalInstance()->waitForDone();
    qDeleteAll(mSpecies);
    mSpecies.clear();
    delete ui;
}

QList<Animal*> & Laboratory::animals()
{
    return mAnimals;
}

QList<Species*> & Laboratory::species()
{
    return mSpecies;
}

int Laboratory::numAnimals() const
{
    return mCachedNumAnimals.loadRelaxed();
}

QString Laboratory::statistics() const
{
    QMutexLocker locker(&mPaintMutex);
    return mCachedStatistics;
}

void Laboratory::captureFrame()
{
    QVector<PaintQuad> quads;
    QString stats;
    int numAnimals = 0;
    for (const Species * species : qAsConst(mSpecies))
    {
        const QColor color = species->color();
        const PaintQuad style{
            static_cast<GLfloat>(color.redF()),
            static_cast<GLfloat>(color.greenF()),
            static_cast<GLfloat>(color.blueF()),
            0,
            0
        };
        const QVector<Animal *> & animals = species->animals();
        if (species->type() == Species::typeAnimal)
        {
            numAnimals += int(animals.size());
        }
        stats += species->statistics();
        for (const Animal * animal : animals)
        {
            if (!animal)
            {
                continue;
            }
            quads.append({style.r, style.g, style.b,
                          static_cast<GLfloat>(animal->pos().x()),
                          static_cast<GLfloat>(animal->pos().y())});
        }
    }

    QMutexLocker locker(&mPaintMutex);
    mPaintSnapshot.swap(quads);
    mCachedStatistics = stats;
    mCachedNumAnimals.storeRelaxed(numAnimals);
}

double Laboratory::cyclesPerSecond()
{
    return mCalculationThread->cyclesPerSecond();
}

void Laboratory::toggleStart()
{
    if ( mCalculationThread->isRunning() )
    {
        stop();
    }
    else
    {
        start();
    }
}

void Laboratory::start()
{
    mCalculationThread->start();
    mAdvanceTimer.start();
}

void Laboratory::stop()
{
    mCalculationThread->stop();
    mCalculationThread->wait();
    mAdvanceTimer.stop();
    update();
}

void Laboratory::reset()
{
    stop();
    initActors();
    update();
}

void Laboratory::setPlantPattern(int pattern)
{
    if (pattern != mSettings.plantPattern)
    {
        mSettings.plantPattern = (PlantPattern)pattern;
        emit plantPatternChanged((PlantPattern)pattern);
        QSettings settings("ryank", "Evolve", this);
        settings.setValue("Plants.Pattern", (PlantPattern)pattern);
    }
}

void Laboratory::initializeGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);               // Black Background
    glDisable(GL_DEPTH_TEST);                            // Disables Depth Testing
    gluOrtho2D(GLdouble(0.0), static_cast<GLdouble>(LABORATORY_WIDTH), static_cast<GLdouble>(LABORATORY_HEIGHT), GLdouble(0.0));
//    glEnable(GL_BLEND);
//    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Laboratory::resizeGL(int w, int h)
{
//    glViewport( 0, 0, (GLint)w, (GLint)h );
    if (h==0)                           // Prevent A Divide By Zero By
    {
            h=1;                        // Making Height Equal One
    }

    glViewport(0, 0, w, h);             // Reset The Current Viewport
    glMatrixMode(GL_PROJECTION);        // Select The Projection Matrix
    glLoadIdentity();                   // Reset The Projection Matrix
    // Calculate The Aspect Ratio Of The Window
//    gluPerspective(90.0f,(GLfloat)w/(GLfloat)h,0.1f,100.0f);

    glOrtho(0, LABORATORY_WIDTH, LABORATORY_HEIGHT, 0, 0, 1);
    glMatrixMode(GL_MODELVIEW);         // Select The Modelview Matrix
    glLoadIdentity();                   // Reset The Modelview Matrix
}

void Laboratory::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.375, 0.375, 0);

    QVector<PaintQuad> quads;
    {
        QMutexLocker locker(&mPaintMutex);
        quads = mPaintSnapshot;
    }

    if (quads.isEmpty())
    {
        return;
    }

    QVector<GLfloat> vertices;
    QVector<GLfloat> colors;
    vertices.resize(quads.size() * 8);
    colors.resize(quads.size() * 12);
    GLfloat * vert = vertices.data();
    GLfloat * col = colors.data();
    for (const PaintQuad & quad : qAsConst(quads))
    {
        const GLfloat x = quad.x;
        const GLfloat y = quad.y;
        vert[0] = x - 1; vert[1] = y + 1;
        vert[2] = x + 1; vert[3] = y + 1;
        vert[4] = x + 1; vert[5] = y - 1;
        vert[6] = x - 1; vert[7] = y - 1;
        vert += 8;
        for (int i = 0; i < 4; ++i)
        {
            col[0] = quad.r;
            col[1] = quad.g;
            col[2] = quad.b;
            col += 3;
        }
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices.constData());
    glColorPointer(3, GL_FLOAT, 0, colors.constData());
    glDrawArrays(GL_QUADS, 0, GLsizei(quads.size() * 4));
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

int Laboratory::heightForWidth(int w) const
{
    return int( ((float)LABORATORY_HEIGHT / (float)LABORATORY_WIDTH ) * (float)w);
}

QColor Laboratory::colorForIndex(int index) const
{
    switch(index)
    {
    case 0:
        return QColor(0,104,132);
    case 1:
        return QColor(0,144,158);
    case 2:
        return QColor(137,219,236);
    case 3:
        return QColor(237,0,38);
    case 4:
        return QColor(250,157,0);
    case 5:
        return QColor(255,208,141);
    case 6:
        return QColor(176,0,81);
    case 7:
        return QColor(246,131,112);
    case 8:
        return QColor(254,171,185);
    case 9:
        return QColor(110,0,108);
    case 10:
        return QColor(145,39,143);
    case 11:
        return QColor(207,151,215);
    case 12:
    default:
        return QColor(Qt::white);
    }
}

QList<Species*> Laboratory::loadSpecies()
{
    QList<Species*> speciesList;

    QDir dir("species");
    dir.setFilter(QDir::Files | QDir::Readable | QDir::NoSymLinks);
    qWarning() << "Looking for species in" << dir.canonicalPath();

    QStringList fileExtensions;
    fileExtensions.append("*.SPC");
    dir.setNameFilters(fileExtensions);

    foreach ( const QFileInfo & fileInfo, dir.entryInfoList() )
    {
        auto * species = new Species(Species::typeAnimal);

        if ( Species::load(fileInfo.filePath(), *species) )
        {
            qDebug() << "Loaded species:" << fileInfo.fileName();
            species->setColor(colorForIndex(speciesList.size()));
            speciesList.append(species);
        }
        else
        {
            qWarning() << "Unable to load species:" << fileInfo.fileName();
            delete species;
        }
    }

    return speciesList;
}

void Laboratory::initActors()
{
    positionLock.lockForWrite();
    for (auto species : qAsConst(mSpecies))
    {
        qWarning() << "Resetting species" << species->name();
        species->clear();
        if ( species->type() == Species::typeAnimal )
        {
            species->initialize(1000, 10000, 900);
        }
        else if ( species->type() == Species::typePlant )
        {
            species->initialize(10, MAX_NUM_PLANTS, PLANT_INITIAL_ENERGY);
        }
    }
    positionLock.unlock();
    captureFrame();
}

void calculateRange(const QPair<Animal * const *, int> & range)
{
    Animal * const * ptrs = range.first;
    const int count = range.second;
    for (int i = 0; i < count; ++i)
    {
        ptrs[i]->calculateMovement();
    }
}

void executeSpecies(Species * & species)
{
    const QVector<Animal*> snapshot = species->animals();
    for (Animal * animal : snapshot)
    {
        animal->executeMovement();
    }
}

CalculationThread::CalculationThread(Laboratory * laboratory, QObject * parent) : QThread(parent),
    mLaboratory(laboratory)
{
}

double CalculationThread::cyclesPerSecond()
{
    QMutexLocker locker(&mDataMutex);

    qint64 numCycles = mNumCycles - mLastNumCycles;
    mLastNumCycles = mNumCycles;

    const qint64 elapsedMs = mPerformanceTimer.restart();
    if (elapsedMs <= 0)
    {
        return 0.0;
    }
    return double(numCycles) / (elapsedMs / 1000.0);
}

void CalculationThread::stop()
{
    mStop.store(true, std::memory_order_relaxed);
}

void CalculationThread::run()
{
    mPerformanceTimer.start();
    mStop.store(false, std::memory_order_relaxed);

    forever
    {
        if ( mStop.load(std::memory_order_relaxed) )
        {
            qWarning() << "Stopping";
            return;
        }

        {
            QReadLocker locker(&positionLock);
            mCalcAnimals.clear();
            for (Species * species : mLaboratory->species())
            {
                if (species->type() != Species::typeAnimal)
                {
                    continue;
                }
                mCalcAnimals += species->animals();
                species->advanceCombatCycle();
            }
            if (Species * plants = Species::plantSpecies())
            {
                plants->advanceCombatCycle();
            }
        }

        if (!mCalcAnimals.isEmpty())
        {
            const int n = int(mCalcAnimals.size());
            const int threadCount = qMax(1, qMin(n, QThread::idealThreadCount()));
            QVector<QPair<Animal * const *, int>> ranges;
            ranges.reserve(threadCount);
            for (int t = 0; t < threadCount; ++t)
            {
                const int begin = t * n / threadCount;
                const int end = (t + 1) * n / threadCount;
                if (begin < end)
                {
                    ranges.append(qMakePair(mCalcAnimals.constData() + begin, end - begin));
                }
            }
            QtConcurrent::blockingMap(ranges, calculateRange);
        }

        if ( mStop.load(std::memory_order_relaxed) )
        {
            return;
        }

        positionLock.lockForWrite();
        QtConcurrent::blockingMap(mLaboratory->species(), executeSpecies);
        for (Species * species : mLaboratory->species())
        {
            species->respawn(1, 500);
        }
        mLaboratory->captureFrame();
        positionLock.unlock();

        mDataMutex.lock();
        mNumCycles++;
        mDataMutex.unlock();

        const int speed = mLaboratory->speed();
        if ( speed != 0 )
        {
            usleep(speed);
        }
    }
}


