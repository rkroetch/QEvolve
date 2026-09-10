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

QReadWriteLock positionLock;

Laboratory::Laboratory(QWidget *parent) :
    QOpenGLWidget(parent),
    ui(new Ui::Laboratory)
{
    ui->setupUi(this);

    mSpeed = 10;

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
    QReadLocker locker(&positionLock);
    int numAnimals = 0;
    foreach ( Species * species, mSpecies )
    {
        if ( species->type() == Species::typeAnimal )
        {
            numAnimals += species->animals().size();
        }
    }
    return numAnimals;
}

QString Laboratory::statistics() const
{
    QReadLocker locker(&positionLock);
    QString stats;
    foreach ( Species * species, mSpecies )
    {
        if ( species->type() == Species::typePlant )
        {
            stats += species->statistics();
        }
    }
    foreach ( Species * species, mSpecies )
    {
        if ( species->type() == Species::typeAnimal )
        {
            stats += species->statistics();
        }
    }
    return stats;
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

//    glEnableClientState(GL_VERTEX_ARRAY);

//    foreach ( Species * species, mSpecies )
//    {
////        glColor4f(species->color().redF(), species->color().blueF(), species->color().greenF(), 0.25f);
//        glColor3f(species->color().redF(), species->color().greenF(), species->color().blueF());

//        int vertexIndex = 0;
//        int index = 0;
//        for ( ; index < species->animals().size(); index++ )
//        {
//            vertexIndex = index * 8;
//            Animal * animal = species->animals().at(index);
//            mVertices[vertexIndex] = animal->pos().x() - 1;
//            mVertices[vertexIndex + 1] = animal->pos().y() + 1;
//            mVertices[vertexIndex + 2] = animal->pos().x() + 1;
//            mVertices[vertexIndex + 3] = animal->pos().y() + 1;
//            mVertices[vertexIndex + 4] = animal->pos().x() + 1;
//            mVertices[vertexIndex + 5] = animal->pos().y() - 1;
//            mVertices[vertexIndex + 6] = animal->pos().x() - 1;
//            mVertices[vertexIndex + 7] = animal->pos().y() - 1;
//        }

//        glVertexPointer(2, GL_FLOAT, 0, mVertices);
//        glDrawArrays(GL_QUADS, 0, (GLsizei)(index * 8));

//    }
//    glDisableClientState(GL_VERTEX_ARRAY);  // disable vertex arrays

    struct PaintQuad
    {
        GLfloat r, g, b;
        QPointF pos;
    };
    QVector<PaintQuad> quads;
    {
        QReadLocker locker(&positionLock);
        for (const Species * species : qAsConst(mSpecies))
        {
            const QColor color = species->color();
            const PaintQuad style{
                static_cast<GLfloat>(color.redF()),
                static_cast<GLfloat>(color.greenF()),
                static_cast<GLfloat>(color.blueF()),
                {}
            };
            const QList<Animal *> animals = species->animals();
            for (const Animal * animal : animals)
            {
                if (!animal)
                {
                    continue;
                }
                quads.append({style.r, style.g, style.b, animal->pos()});
            }
        }
    }

    glBegin(GL_QUADS);
    for (const PaintQuad & quad : qAsConst(quads))
    {
        glColor3f(quad.r, quad.g, quad.b);
        glVertex2f(quad.pos.x() - 1, quad.pos.y() + 1);
        glVertex2f(quad.pos.x() + 1, quad.pos.y() + 1);
        glVertex2f(quad.pos.x() + 1, quad.pos.y() - 1);
        glVertex2f(quad.pos.x() - 1, quad.pos.y() - 1);
    }
    glEnd();
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
}

void moveAnimal(Animal * & animal)
{
    animal->calculateMovement();
}

void moveAnimals(const QList<Animal*> & animal)
{
    foreach ( Animal * animal, animal )
    {
        animal->calculateMovement();
    }
}

void moveSpecies(Species * & species)
{
    foreach ( Animal * animal, species->animals() )
    {
        animal->calculateMovement();
    }
}

void executeMovement(Species * & species)
{
    foreach ( Animal * animal, species->animals() )
    {
        animal->executeMovement();
    }
}

CalculationThread::CalculationThread(Laboratory * laboratory, QObject * parent) : QThread(parent),
    mLaboratory(laboratory),
    mStop(false)
{
//    QueryPerformanceFrequency(&mCountsPerSecond);
}

double CalculationThread::cyclesPerSecond()
{
    QMutexLocker locker(&mDataMutex);

    qint64 numCycles = mNumCycles - mLastNumCycles;
    mLastNumCycles = mNumCycles;

    return double(numCycles) / (mPerformanceTimer.restart() / 1000.0);
//    qint64 lastCounts = mCounts.QuadPart;

//    QueryPerformanceCounter(&mCounts);
//    qint64 numCounts = mCounts.QuadPart - lastCounts;
//    qint64 numCycles = mNumCycles - mLastNumCycles;
//    mLastNumCycles = mNumCycles;

//    double seconds = double(numCounts) / double(mCountsPerSecond.QuadPart);

//    return double(numCycles) / seconds;
}

void CalculationThread::stop()
{
    QMutexLocker locker(&mStopMutex);
    mStop = true;
}

void CalculationThread::run()
{
    mPerformanceTimer.start();

    mStopMutex.lock();
    mStop = false;
    mStopMutex.unlock();

    forever
    {
        {
            QMutexLocker locker(&mStopMutex);
            if ( mStop )
            {
                qWarning() << "Stopping";
                return;
            }
        }
//        QList<Animal *> animals;
//        foreach ( Species * species, mLaboratory->species() )
//        {
//            animals += species->animals();
//        }
//        QtConcurrent::blockingMap(animals, moveAnimal);

//        QtConcurrent::blockingMap(mLaboratory->species(), moveSpecies);

        QList<Animal *> animals;
        {
            QReadLocker locker(&positionLock);
            foreach ( Species * species, mLaboratory->species() )
            {
                animals += species->animals();
            }
        }
        auto numAnimalsPerThread = animals.size() / 16;
        QList<QList<Animal *>> animalLists;
        auto index = 0;
        for ( int thread = 0; thread < 16; ++thread )
        {
            QList<Animal*> threadList;
            threadList = animals.mid(index, numAnimalsPerThread);
            index += threadList.size();
            animalLists.append(threadList);
        }
        QtConcurrent::blockingMap(animalLists, moveAnimals);

        {
            QMutexLocker locker(&mStopMutex);
            if ( mStop )
            {
                return;
            }
        }

        positionLock.lockForWrite();
        QtConcurrent::blockingMap(mLaboratory->species(), executeMovement);
        foreach ( Species * species, mLaboratory->species() )
        {
            species->respawn(1, 500);
        }
        positionLock.unlock();

        mDataMutex.lock();
        mNumCycles++;
        mDataMutex.unlock();

        if ( mLaboratory->speed() != 0 )
        {
//            DELAY_CYCLE(mLaboratory->speed());
            usleep(mLaboratory->speed());
        }
    }
}


