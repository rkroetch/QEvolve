#include "laboratory.h"
#include "ui_laboratory.h"
#include "animal.h"
#include "species.h"
#include "delay.h"
#include "deatheffects.h"

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
#include <QtMath>
#include <algorithm>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QShowEvent>
#include <QCursor>
#include "animalinfodialog.h"

#include "cycleconcurrent.h"

// Threading backend for CalculationThread::run()'s two blockingMap() calls.
// Default is QtConcurrent, which re-dispatches to Qt's shared global thread
// pool on every call - the right choice when cycles are paced (speed != 0)
// since dispatch overhead is noise next to the usleep(). Defining
// QEVOLVE_USE_PERSISTENT_THREAD_POOL (see the QEVOLVE_USE_PERSISTENT_THREAD_POOL
// CMake option) switches to CycleConcurrent (cycleconcurrent.h) instead: a
// persistent worker pool kept alive for the life of the calculation thread,
// which trades higher CPU utilization for substantially lower wall time
// when cycles run back to back with no delay (speed == 0, the default) -
// see cycleconcurrent.h and benchmarks/species_benchmark.cpp's
// BM_LargeScaleSimulation vs BM_LargeScaleSimulationPersistentPool for the
// measured trade-off. Both backends expose the same blockingMap(sequence,
// fn) contract, so this is a namespace swap - no call site below needs to
// change with the mode.
#define QEVOLVE_USE_PERSISTENT_THREAD_POOL
#ifdef QEVOLVE_USE_PERSISTENT_THREAD_POOL
namespace ActiveConcurrent = CycleConcurrent;
#else
namespace ActiveConcurrent = QtConcurrent;
#endif

QReadWriteLock positionLock;

// Timing/shape of the death-effect "asterisk" burst drawn in paintGL().
constexpr qint64 kDeathEffectDurationMs = 450;
constexpr int kDeathEffectRayCount = 8;

Laboratory::Laboratory(QWidget *parent) :
    QOpenGLWidget(parent),
    ui(new Ui::Laboratory)
{
    ui->setupUi(this);

    mSpeed = 0;

    resize(qRound(LABORATORY_WIDTH * mZoom), qRound(LABORATORY_HEIGHT * mZoom));

    mSpecies = loadSpecies();

    QSettings settings("ryank", "Evolve", this);
    setPlantPattern(settings.value("Plants.Pattern", plantPatternOneGroup).value<PlantPattern>());

    auto * plants = new Species(Species::typePlant);
    plants->setName("Plant");
    plants->setColor(QColor(Qt::green));
    plants->setMetabolism(10.0);
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
    plants->setPlantPattern(mSettings.plantPattern);
    mSpecies.append(plants);

    mCalculationThread = new CalculationThread(this);

    mEffectsTimer.start();
    initActors();

    mAdvanceTimer.setInterval(20);
    connect(&mAdvanceTimer, &QTimer::timeout, this, &Laboratory::onAdvanceTick);
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

    const QVector<DeathEvent> deaths = DeathEffects::takeAll();

    QMutexLocker locker(&mPaintMutex);
    mPaintSnapshot.swap(quads);
    mCachedStatistics = stats;
    mCachedNumAnimals.storeRelaxed(numAnimals);

    if (!deaths.isEmpty())
    {
        const qint64 now = mEffectsTimer.elapsed();
        for (const DeathEvent & death : deaths)
        {
            mDeathEffects.append({death.pos, death.color, now});
        }
    }
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
    // Always drop back to a clean sandbox, whether or not a run was active -
    // the toolbar's Reset action shouldn't leave stale run state behind.
    mRunModeActive = false;
    mRunOutcome = RunOutcome::InProgress;
    mCurrentEpoch = 0;
    mRunTicks = 0;
    mMetabolismSurgeBaseline.clear();
    mMetabolismSurgeEpochsRemaining = 0;
    initActors();
    update();
}

void Laboratory::beginRun(const RunConfig & config)
{
    if (!config.playerSpecies || config.playerSpecies->type() != Species::typeAnimal)
    {
        qWarning() << "Laboratory::beginRun() requires a valid animal player species";
        return;
    }
    if (!mSpecies.contains(config.playerSpecies))
    {
        qWarning() << "Laboratory::beginRun() playerSpecies is not one of this lab's species()";
        return;
    }

    stop();
    mRunConfig = config;
    mRunModeActive = true;
    mRunOutcome = RunOutcome::InProgress;
    mCurrentEpoch = 0;
    mRunTicks = 0;
    mMetabolismSurgeBaseline.clear();
    mMetabolismSurgeEpochsRemaining = 0;
    initActors();
    mRunStartTickBaseline = mCalculationThread->totalCycles();
    start();
}

void Laboratory::endRun()
{
    if (!mRunModeActive || mRunOutcome != RunOutcome::InProgress)
    {
        return;
    }
    finishRun(RunOutcome::Lost);
}

void Laboratory::onAdvanceTick()
{
    if (mRunModeActive)
    {
        updateRunState();
    }
    update();
}

void Laboratory::updateRunState()
{
    if (mRunOutcome != RunOutcome::InProgress)
    {
        return;
    }

    const qint64 elapsedTicks = mCalculationThread->totalCycles() - mRunStartTickBaseline;
    mRunTicks = qMax<qint64>(0, elapsedTicks);

    // Looping (rather than jumping straight to the new epoch) matters when a
    // poll spans more than one epoch boundary (e.g. running at high sim
    // speed): every intermediate epoch still gets its epochAdvanced() signal
    // and EncounterSpec applied, so a rival/hazard introduction can never be
    // silently skipped.
    const int epoch = computeEpoch(mRunTicks, mRunConfig.ticksPerEpoch);
    while (mCurrentEpoch < epoch)
    {
        ++mCurrentEpoch;
        emit epochAdvanced(mCurrentEpoch);
        advanceHazards();
        applyEncounterSpec(computeEncounterSpec(mCurrentEpoch, mRunConfig.metaTier));
    }

    bool playerExtinct = false;
    {
        QReadLocker locker(&positionLock);
        playerExtinct = mRunConfig.playerSpecies->animals().isEmpty();
    }

    const RunOutcome outcome = evaluateRunOutcome(playerExtinct, mCurrentEpoch, mRunConfig.targetEpochs);
    if (outcome != RunOutcome::InProgress)
    {
        finishRun(outcome);
    }
}

void Laboratory::finishRun(RunOutcome outcome)
{
    mRunOutcome = outcome;
    const RunResult result = buildRunResult(outcome);
    stop();
    emit runEnded(result);
}

RunResult Laboratory::buildRunResult(RunOutcome outcome) const
{
    RunResult result;
    result.outcome = outcome;
    result.epochsCleared = mCurrentEpoch;
    result.ticksSurvived = mRunTicks;

    QReadLocker locker(&positionLock);
    for (Species * species : mSpecies)
    {
        if (species->type() != Species::typeAnimal)
        {
            continue;
        }

        SpeciesRunStats stats;
        stats.name = species->name();
        const QVector<Animal*> & animals = species->animals();
        stats.finalPopulation = int(animals.size());
        for (const Animal * animal : animals)
        {
            const Animal::Statistics & animalStats = animal->statistics();
            stats.highestGeneration = qMax(stats.highestGeneration, animalStats.mGeneration);
            stats.totalChildren += animalStats.mNumChildren;
        }
        result.speciesStats.append(stats);
    }
    return result;
}

void Laboratory::applyEncounterSpec(const EncounterSpec & spec)
{
    positionLock.lockForWrite();

    for (const RivalSpawn & rival : spec.rivals)
    {
        Species * target = nullptr;
        for (Species * species : qAsConst(mSpecies))
        {
            if (species->type() == Species::typeAnimal &&
                species->name().compare(rival.archetype, Qt::CaseInsensitive) == 0)
            {
                target = species;
                break;
            }
        }
        if (!target)
        {
            qWarning() << "Difficulty curve requested unknown rival archetype:" << rival.archetype;
            continue;
        }

        if (!target->isActive())
        {
            target->activate();
        }

        // Scale the archetype's stock stats by rivalStatMultiplier rather
        // than mutating the species' own SpeciesUserData (spawnAnimal takes
        // spawning-energy/metabolism per-call), so its .SPC-defined baseline
        // stays intact for animals spawned in later, less-escalated epochs.
        const int spawningEnergy = qMax(1, int(target->spawningEnergy() * spec.rivalStatMultiplier));
        const int metabolism = qMax(1, int(target->metabolism() * spec.rivalStatMultiplier));
        for (int i = 0; i < rival.count; ++i)
        {
            const QPointF pos(randIntInclusive(5, LABORATORY_WIDTH - 5), randIntInclusive(5, LABORATORY_HEIGHT - 5));
            target->spawnAnimal(pos, ANIMAL_INITIAL_ENERGY, spawningEnergy, metabolism, target->movements(), QPointF(0, 0), nullptr);
        }
    }

    if (Species * plants = Species::plantSpecies())
    {
        plants->setSpawningEnergy(qMax(1, int(PLANT_SPAWN_ENERGY * spec.plantSpawnEnergyMultiplier)));
    }

    positionLock.unlock();

    if (spec.hazard.type != HazardType::None && randDouble01() < spec.hazard.chance)
    {
        triggerHazard(spec.hazard);
    }
}

void Laboratory::triggerHazard(const HazardEvent & hazard)
{
    switch (hazard.type)
    {
    case HazardType::None:
        break;

    case HazardType::MetabolismSurge:
    {
        // One surge at a time: a re-roll while one is already active just
        // refreshes its remaining duration rather than compounding the
        // multiplier on top of an already-elevated baseline.
        positionLock.lockForWrite();
        if (mMetabolismSurgeBaseline.isEmpty())
        {
            for (Species * species : qAsConst(mSpecies))
            {
                if (species->type() == Species::typeAnimal)
                {
                    mMetabolismSurgeBaseline.insert(species, species->metabolism());
                    species->setMetabolism(qMax(1, int(species->metabolism() * (1.0 + hazard.magnitude))));
                }
            }
        }
        positionLock.unlock();
        mMetabolismSurgeEpochsRemaining = qMax(mMetabolismSurgeEpochsRemaining, hazard.durationEpochs);
        break;
    }

    case HazardType::PlantDieOff:
    {
        positionLock.lockForWrite();
        if (Species * plants = Species::plantSpecies())
        {
            const QVector<Animal *> snapshot = plants->animals();
            const int cullCount = int(snapshot.size() * qBound(0.0, hazard.magnitude, 1.0));
            for (int i = 0; i < cullCount; ++i)
            {
                plants->killAnimal(snapshot[i]->cellX(), snapshot[i]->cellY(), snapshot[i]);
            }
        }
        positionLock.unlock();
        break;
    }

    case HazardType::ResourceBloom:
    {
        positionLock.lockForWrite();
        if (Species * plants = Species::plantSpecies())
        {
            plants->respawn(int(MAX_NUM_PLANTS * qBound(0.0, hazard.magnitude, 1.0)), PLANT_INITIAL_ENERGY);
        }
        positionLock.unlock();
        break;
    }
    }
}

void Laboratory::advanceHazards()
{
    if (mMetabolismSurgeEpochsRemaining <= 0)
    {
        return;
    }

    if (--mMetabolismSurgeEpochsRemaining == 0)
    {
        positionLock.lockForWrite();
        for (auto it = mMetabolismSurgeBaseline.constBegin(); it != mMetabolismSurgeBaseline.constEnd(); ++it)
        {
            it.key()->setMetabolism(it.value());
        }
        mMetabolismSurgeBaseline.clear();
        positionLock.unlock();
    }
}

void Laboratory::setSpeciesActive(Species * species, bool active)
{
    if (!species || species->isActive() == active)
    {
        return;
    }

    positionLock.lockForWrite();
    if (active)
    {
        species->activate();
    }
    else
    {
        species->deactivate();
    }
    positionLock.unlock();

    captureFrame();
    update();
}

void Laboratory::setPlantPattern(int pattern)
{
    if (pattern != mSettings.plantPattern)
    {
        mSettings.plantPattern = (PlantPattern)pattern;
        if (Species * plants = Species::plantSpecies())
        {
            plants->setPlantPattern(mSettings.plantPattern);
        }
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
    QVector<DeathEffectAnim> effects;
    {
        QMutexLocker locker(&mPaintMutex);
        quads = mPaintSnapshot;

        const qint64 now = mEffectsTimer.elapsed();
        mDeathEffects.erase(std::remove_if(mDeathEffects.begin(), mDeathEffects.end(),
            [now](const DeathEffectAnim & effect) { return now - effect.spawnMs > kDeathEffectDurationMs; }),
            mDeathEffects.end());
        effects = mDeathEffects;
    }

    if (!effects.isEmpty())
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(1.5f);
        const qint64 now = mEffectsTimer.elapsed();
        for (const DeathEffectAnim & effect : qAsConst(effects))
        {
            const float t = qBound(0.0f, float(now - effect.spawnMs) / float(kDeathEffectDurationMs), 1.0f);
            const float ease = 1.0f - (1.0f - t) * (1.0f - t);
            const float outerR = 1.0f + ease * 5.0f;
            const float innerR = qMax(0.0f, outerR - 2.5f);
            const float alpha = 1.0f - t;

            glBegin(GL_LINES);
            glColor4f(float(effect.color.redF()), float(effect.color.greenF()), float(effect.color.blueF()), alpha);
            for (int i = 0; i < kDeathEffectRayCount; ++i)
            {
                const float angle = (2.0f * float(M_PI) * i) / kDeathEffectRayCount;
                const float c = qCos(angle);
                const float s = qSin(angle);
                glVertex2f(float(effect.pos.x()) + c * innerR, float(effect.pos.y()) + s * innerR);
                glVertex2f(float(effect.pos.x()) + c * outerR, float(effect.pos.y()) + s * outerR);
            }
            glEnd();
        }
        glLineWidth(1.0f);
        glDisable(GL_BLEND);
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

QScrollArea * Laboratory::enclosingScrollArea() const
{
    for (QWidget * p = parentWidget(); p; p = p->parentWidget())
    {
        if (auto * area = qobject_cast<QScrollArea *>(p))
        {
            return area;
        }
    }
    return nullptr;
}

qreal Laboratory::fitZoom() const
{
    QScrollArea * scrollArea = enclosingScrollArea();
    if (!scrollArea)
    {
        return kMinZoom;
    }

    const QSize viewportSize = scrollArea->viewport()->size();
    if (viewportSize.width() <= 0 || viewportSize.height() <= 0)
    {
        return kMinZoom;
    }

    const qreal widthFit = qreal(viewportSize.width()) / LABORATORY_WIDTH;
    const qreal heightFit = qreal(viewportSize.height()) / LABORATORY_HEIGHT;
    return qMin(widthFit, heightFit);
}

void Laboratory::applyZoom(qreal newZoom, const QPoint & anchor)
{
    newZoom = qBound(fitZoom(), newZoom, kMaxZoom);
    if (qFuzzyCompare(newZoom, mZoom))
    {
        return;
    }

    QScrollArea * scrollArea = enclosingScrollArea();
    int oldHValue = 0;
    int oldVValue = 0;
    if (scrollArea)
    {
        oldHValue = scrollArea->horizontalScrollBar()->value();
        oldVValue = scrollArea->verticalScrollBar()->value();
    }

    const qreal scaleFactor = newZoom / mZoom;
    mZoom = newZoom;
    resize(qRound(LABORATORY_WIDTH * mZoom), qRound(LABORATORY_HEIGHT * mZoom));

    if (scrollArea)
    {
        const int newHValue = qRound(oldHValue + anchor.x() * (scaleFactor - 1));
        const int newVValue = qRound(oldVValue + anchor.y() * (scaleFactor - 1));
        scrollArea->horizontalScrollBar()->setValue(newHValue);
        scrollArea->verticalScrollBar()->setValue(newVValue);
    }
}

void Laboratory::wheelEvent(QWheelEvent * event)
{
    const int deltaY = event->angleDelta().y();
    if (deltaY == 0)
    {
        QOpenGLWidget::wheelEvent(event);
        return;
    }

    const qreal steps = deltaY / 120.0;
    applyZoom(mZoom * qPow(kZoomStepFactor, steps), event->position().toPoint());
    event->accept();
}

void Laboratory::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (QScrollArea * scrollArea = enclosingScrollArea())
        {
            mDragging = true;
            mDragStartMouse = event->globalPosition().toPoint();
            mPressLocalPos = event->position().toPoint();
            mDragStartHValue = scrollArea->horizontalScrollBar()->value();
            mDragStartVValue = scrollArea->verticalScrollBar()->value();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
    }
    QOpenGLWidget::mousePressEvent(event);
}

void Laboratory::mouseMoveEvent(QMouseEvent * event)
{
    if (mDragging)
    {
        if (QScrollArea * scrollArea = enclosingScrollArea())
        {
            const QPoint delta = event->globalPosition().toPoint() - mDragStartMouse;
            scrollArea->horizontalScrollBar()->setValue(mDragStartHValue - delta.x());
            scrollArea->verticalScrollBar()->setValue(mDragStartVValue - delta.y());
        }
        event->accept();
        return;
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void Laboratory::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton && mDragging)
    {
        mDragging = false;
        unsetCursor();

        const QPoint delta = event->globalPosition().toPoint() - mDragStartMouse;
        if (delta.manhattanLength() <= kClickMoveTolerance)
        {
            handleCanvasClicked(mPressLocalPos);
        }

        event->accept();
        return;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void Laboratory::showEvent(QShowEvent * event)
{
    QOpenGLWidget::showEvent(event);

    if (!mViewportFilterInstalled)
    {
        if (QScrollArea * scrollArea = enclosingScrollArea())
        {
            scrollArea->viewport()->installEventFilter(this);
            mViewportFilterInstalled = true;
        }
    }
}

bool Laboratory::eventFilter(QObject * watched, QEvent * event)
{
    if (event->type() == QEvent::Resize)
    {
        // The viewport just changed size, so the fit-to-window zoom floor
        // (see fitZoom()) may have moved too. Re-apply the current zoom so
        // it gets clamped back up to the new floor if it now falls under it
        // - otherwise growing the window would leave the lab under-filling
        // the viewport instead of covering it.
        if (QScrollArea * scrollArea = enclosingScrollArea(); scrollArea && watched == scrollArea->viewport())
        {
            const QPoint anchor(scrollArea->viewport()->width() / 2, scrollArea->viewport()->height() / 2);
            applyZoom(mZoom, anchor);
        }
    }
    return QOpenGLWidget::eventFilter(watched, event);
}

void Laboratory::handleCanvasClicked(const QPoint & localPos)
{
    if (width() <= 0 || height() <= 0)
    {
        return;
    }

    const QPointF logicalPos(localPos.x() * double(LABORATORY_WIDTH) / width(),
                              localPos.y() * double(LABORATORY_HEIGHT) / height());
    const int cellX = clampCellX(int(logicalPos.x()));
    const int cellY = clampCellY(int(logicalPos.y()));
    const qreal hitTestRadiusSq = kHitTestRadius * kHitTestRadius;

    // Animals are drawn as a 2-logical-unit-wide quad centered on their exact
    // position (see paintGL()), so a click on the visible square can easily
    // land in a cell other than the one that animal's (truncated) position
    // belongs to. Search the clicked cell's full 3x3 neighborhood - the same
    // neighbor set/wraparound the rest of the simulation uses, via
    // Species::forEachNeighborCell - and keep whatever falls within
    // kHitTestRadius of the actual click, so the whole visible quad (plus a
    // little slack) is clickable rather than just a sliver of its cell.
    QVector<QPair<qreal, AnimalSnapshot>> hits;

    {
        // A write lock is needed (not just a read lock) because the fields
        // we're copying here (pos, statistics, ...) are mutated by
        // CalculationThread's worker threads without any lock of their own
        // during the calculation phase - see the QReadLocker in
        // CalculationThread::run(), which only protects the species/cell
        // topology, not the per-Animal fields calculateMovement() writes.
        // Only a write lock actually excludes that phase.
        QWriteLocker locker(&positionLock);
        for (Species * species : qAsConst(mSpecies))
        {
            species->forEachNeighborCell(cellX, cellY, [&](int x, int y) {
                for (Animal * animal : species->cellOccupants(x, y))
                {
                    const QPointF delta = animal->pos() - logicalPos;
                    const qreal distSq = QPointF::dotProduct(delta, delta);
                    if (distSq > hitTestRadiusSq)
                    {
                        continue;
                    }

                    AnimalSnapshot snapshot;
                    snapshot.speciesName = species->name();
                    snapshot.species = species;
                    snapshot.color = animal->color();
                    snapshot.pos = animal->pos();
                    snapshot.movements = animal->movements();
                    snapshot.stats = animal->statistics();
                    hits.append({distSq, snapshot});
                }
            });
        }
    }

    if (hits.isEmpty())
    {
        return;
    }

    std::sort(hits.begin(), hits.end(), [](const auto & a, const auto & b) {
        return a.first < b.first;
    });

    QVector<AnimalSnapshot> snapshots;
    snapshots.reserve(hits.size());
    for (const auto & hit : hits)
    {
        snapshots.append(hit.second);
    }

    AnimalInfoDialog dialog(snapshots, this);
    dialog.exec();
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
            species->initialize(10, 10000, ANIMAL_INITIAL_ENERGY, species->isActive());
        }
        else if ( species->type() == Species::typePlant )
        {
            species->initialize(10, MAX_NUM_PLANTS, PLANT_INITIAL_ENERGY);
        }
    }
    positionLock.unlock();

    // Discard any death events still in flight so a reset doesn't leave a
    // stale burst frozen on screen: reset() stops mAdvanceTimer right before
    // this, so whatever captureFrame() below draws is the last repaint until
    // Start is pressed again - any not-yet-expired effect would otherwise
    // sit there indefinitely instead of fading out.
    DeathEffects::takeAll();
    {
        QMutexLocker locker(&mPaintMutex);
        mDeathEffects.clear();
    }

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

qint64 CalculationThread::totalCycles() const
{
    QMutexLocker locker(&mDataMutex);
    return mNumCycles;
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
            ActiveConcurrent::blockingMap(ranges, calculateRange);
        }

        if ( mStop.load(std::memory_order_relaxed) )
        {
            return;
        }

        positionLock.lockForWrite();
        ActiveConcurrent::blockingMap(mLaboratory->species(), executeSpecies);
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


