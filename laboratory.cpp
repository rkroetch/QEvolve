#include "laboratory.h"
#include "ui_laboratory.h"
#include "animal.h"
#include "species.h"
#include "delay.h"
#include "deatheffects.h"

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
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QVector2D>
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

// --- Core-profile (GL 3.3) shaders - see main.cpp's QSurfaceFormat setup,
// which requests a core context, and setupShaders(), which compiles these.
// ---

// Shared by both the animal/plant quads and the death-effect lines (see
// renderScene()/drawPrimitives()): transforms a logical-space position by
// uProjection (built in resizeGL(), replacing the old fixed-function
// glOrtho()) and passes the per-vertex RGBA color straight through.
const char * const kPrimitiveVertexShaderSource = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;
uniform mat4 uProjection;
out vec4 vColor;
void main()
{
    gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);
    vColor = aColor;
}
)GLSL";

const char * const kPrimitiveFragmentShaderSource = R"GLSL(
#version 330 core
in vec4 vColor;
out vec4 fragColor;
void main()
{
    fragColor = vColor;
}
)GLSL";

// Optional CRT post-process shader (see Laboratory::renderCrtPass()): draws
// mSceneFbo's color texture through a static, full-screen NDC quad (see
// setupCrtQuadBuffers()) rather than the old fixed-function fullscreen quad
// keyed off gl_Vertex/gl_ModelViewProjectionMatrix.
const char * const kCrtVertexShaderSource = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main()
{
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)GLSL";

const char * const kCrtFragmentShaderSource = R"GLSL(
#version 330 core
uniform sampler2D uScene;
uniform vec2 uResolution;
uniform float uTime;
in vec2 vTexCoord;
out vec4 fragColor;

void main()
{
    // Barrel distortion, centered on the screen.
    vec2 centered = vTexCoord * 2.0 - 1.0;
    vec2 warped = centered + centered.yx * centered.yx * centered * 0.16;
    vec2 uv = warped * 0.5 + 0.5;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
    {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 color = texture(uScene, uv).rgb;

    // Pixel bloom: sample two concentric rings of neighboring texels (a
    // tight one and a wider one, so the glow actually reaches a few cells
    // out rather than just softening each sprite's immediate edge) and add
    // back their bright parts, so saturated sprite pixels glow outward into
    // the darker cells around them instead of blurring everything equally.
    // A low luminance threshold means most palette colors (not just
    // near-white ones) contribute, and the final multiplier is pushed well
    // past "subtle" - this is meant to be a clearly visible glow.
    vec2 texel = 1.0 / uResolution;
    vec3 bloom = vec3(0.0);
    float bloomWeight = 0.0;
    for (int ring = 1; ring <= 2; ++ring)
    {
        float radius = float(ring) * 2.2;
        float ringWeight = 1.0 / float(ring);
        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                if (dx == 0 && dy == 0)
                {
                    continue;
                }
                vec3 neighbor = texture(uScene, uv + vec2(float(dx), float(dy)) * texel * radius).rgb;
                float luminance = dot(neighbor, vec3(0.299, 0.587, 0.114));
                bloom += neighbor * smoothstep(0.12, 0.85, luminance) * ringWeight;
                bloomWeight += ringWeight;
            }
        }
    }
    color += bloom * (2.4 / bloomWeight);

    // Horizontal scanlines. Density is tied to the actual pixel resolution
    // (so line spacing stays consistent regardless of window size/zoom) but
    // divided down from a 1:1 pixel rate - chunkier, more clearly "low-res"
    // scanlines with visible gaps between them, rather than a near-solid
    // dark/light dither every single row.
    const float kScanlineResolutionDivisor = 4.0;
    float scanline = sin(uv.y * (uResolution.y / kScanlineResolutionDivisor) * 3.14159265);
    color *= mix(0.72, 1.0, scanline * 0.5 + 0.5);

    // Soft vignette toward the corners.
    vec2 vignetteCoord = uv - 0.5;
    float vignette = 1.0 - dot(vignetteCoord, vignetteCoord) * 0.9;
    color *= clamp(vignette, 0.0, 1.0);

    // Faint flicker, subtle enough not to be distracting.
    color *= 0.96 + 0.04 * sin(uTime * 47.0);

    fragColor = vec4(color, 1.0);
}
)GLSL";

Laboratory::Laboratory(QWidget *parent) :
    QOpenGLWidget(parent),
    ui(new Ui::Laboratory)
{
    ui->setupUi(this);

    mSpeed = 0;

    resize(targetSize());

    mSpecies = loadSpecies();

    QSettings settings("ryank", "Evolve", this);
    setPlantPattern(settings.value("Plants.Pattern", plantPatternOneGroup).value<PlantPattern>());
    mCrtShaderEnabled = settings.value("Display/CrtShaderEnabled", false).toBool();

    auto * plants = new Species(Species::typePlant);
    plants->setName("Plant");
    // DB16 "green" swatch (#6daa2c) - see colorForIndex() below for the rest
    // of the palette mapping. Kept brighter than DB16's darker #346524 so
    // single-pixel plant quads stay readable against the near-black canvas.
    plants->setColor(QColor(0x6d, 0xaa, 0x2c));
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

    // Every GL object below is only ever allocated while this widget's own
    // GL context is current (mSceneVao/mSceneVbo/mCrtVao/mCrtVbo/
    // mPrimitiveShaderProgram unconditionally, from initializeGL();
    // mSceneFbo/mCrtShaderProgram lazily, from ensureCrtResources() the
    // first time the CRT shader is enabled) - deleting them needs that same
    // context current again, not whatever happens to be current now.
    if (mSceneVao != 0 || mSceneFbo != nullptr || mCrtShaderProgram != nullptr)
    {
        makeCurrent();
        glDeleteVertexArrays(1, &mSceneVao);
        glDeleteBuffers(1, &mSceneVbo);
        glDeleteVertexArrays(1, &mCrtVao);
        glDeleteBuffers(1, &mCrtVbo);
        delete mPrimitiveShaderProgram;
        mPrimitiveShaderProgram = nullptr;
        delete mSceneFbo;
        mSceneFbo = nullptr;
        delete mCrtShaderProgram;
        mCrtShaderProgram = nullptr;
        doneCurrent();
    }

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
    // Player species population for updateRunState()'s playerExtinct check
    // (see mCachedPlayerAnimalCount's declaration in laboratory.h). Captured
    // here, alongside the other mPaintMutex-guarded snapshots below, rather
    // than having updateRunState() take its own positionLock read-lock on
    // the main thread: captureFrame() always runs already inside a
    // positionLock write-lock section (see the callers of this function),
    // so this read of species->animals() is already safe without any
    // further locking here.
    int playerAnimalCount = 0;
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
        if (species == mRunConfig.playerSpecies)
        {
            playerAnimalCount = int(animals.size());
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
    mCachedPlayerAnimalCount = playerAnimalCount;

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

    // A run starts with only the player's species active - every other
    // animal archetype comes online on the difficulty curve's own schedule
    // via applyEncounterSpec()'s activate() calls (see difficultycurve.cpp).
    // Without this, every bestiary species is active by default (sandbox
    // mode's usual state) and initActors() below would spawn the player
    // into a map already swarming with all of them from tick 0, ignoring
    // the scripted escalation entirely - a real gap found by the Phase 3
    // balance pass (its report predicted this from simulated data; the
    // fix - controlling the roster here - is out of that pass's scope
    // since it's a Laboratory wiring change, not a numeric one).
    positionLock.lockForWrite();
    for (Species * species : qAsConst(mSpecies))
    {
        if (species->type() != Species::typeAnimal)
        {
            continue;
        }
        if (species == config.playerSpecies)
        {
            species->activate();
        }
        else
        {
            species->deactivate();
        }
    }
    positionLock.unlock();

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

    // Read the player's last-captured population instead of taking a fresh
    // positionLock read-lock here: this used to be `QReadLocker
    // locker(&positionLock); playerExtinct =
    // mRunConfig.playerSpecies->animals().isEmpty();`, but that races
    // CalculationThread's own positionLock usage - see the crash writeup in
    // the commit that introduced mCachedPlayerAnimalCount for the full
    // root-cause analysis. mPaintMutex is the same, already-safe mechanism
    // numAnimals()/statistics() use for exactly this kind of cross-thread
    // polling from the main thread.
    bool playerExtinct = false;
    {
        QMutexLocker locker(&mPaintMutex);
        playerExtinct = (mCachedPlayerAnimalCount == 0);
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
    // Stop CalculationThread *before* reading species/animal data below:
    // buildRunResult() takes its own positionLock read-lock on the main
    // thread (see below), and doing that while CalculationThread might
    // still be mid-cycle on another thread is exactly the cross-thread
    // positionLock contention that crashes inside Qt's QReadWriteLock (see
    // mCachedPlayerAnimalCount's declaration in laboratory.h for the fuller
    // writeup) - this call order used to be reversed.
    stop();
    const RunResult result = buildRunResult(outcome);
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

        // plantCapMultiplier was computed by the difficulty curve but never
        // actually applied anywhere - a real integration gap flagged by the
        // Phase 3 balance pass. Species::mMaximumAnimals (the real
        // ceiling on plant population) is fixed at initActors() time and
        // can't shrink without reinitializing the species, which would
        // wipe the existing plant population - so enforce the epoch's
        // scaled-down cap here instead as a soft ceiling, culling any
        // excess the same way triggerHazard()'s PlantDieOff does.
        const int cap = qMax(1, int(MAX_NUM_PLANTS * spec.plantCapMultiplier));
        const QVector<Animal *> plantSnapshot = plants->animals();
        const int cullCount = plantSnapshot.size() - cap;
        for (int i = 0; i < cullCount; ++i)
        {
            plants->killAnimal(plantSnapshot[i]->cellX(), plantSnapshot[i]->cellY(), plantSnapshot[i]);
        }
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

void Laboratory::setCrtShaderEnabled(bool enabled)
{
    if (mCrtShaderEnabled == enabled)
    {
        return;
    }
    mCrtShaderEnabled = enabled;
    QSettings settings("ryank", "Evolve", this);
    settings.setValue("Display/CrtShaderEnabled", enabled);
    update();
}

void Laboratory::initializeGL()
{
    initializeOpenGLFunctions();

    // DB16 "near-black" (#140c1c) - the same dark swatch used for the
    // chart/settings-button backgrounds elsewhere in the theme. Slightly
    // lighter than pure black so the CRT shader's scanlines (which darken
    // alternate rows) are actually visible - on a true-black background
    // "darker than black" has nothing to show.
    glClearColor(0.078f, 0.047f, 0.110f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    // No polygon/line smoothing to disable here, unlike the old fixed-
    // function pipeline - core-profile rendering has no such implicit
    // fixed-function AA path in the first place, so this canvas is already
    // hard-edged by default.

    setupShaders();
    setupSceneBuffers();
    setupCrtQuadBuffers();
}

void Laboratory::resizeGL(int w, int h)
{
    if (h == 0) // Prevent a divide by zero (heightForWidth() et al) by
    {
        h = 1; // making height equal one.
    }

    glViewport(0, 0, w, h);

    // Replaces the old fixed-function glOrtho()/matrix-stack setup: same
    // top-left-origin logical (0,0)-(LABORATORY_WIDTH,LABORATORY_HEIGHT)
    // space, now built as an explicit matrix uploaded to
    // mPrimitiveShaderProgram's uProjection uniform in renderScene().
    mProjection.setToIdentity();
    mProjection.ortho(0.0f, float(LABORATORY_WIDTH), float(LABORATORY_HEIGHT), 0.0f, -1.0f, 1.0f);

    mViewportWidth = w;
    mViewportHeight = h;
}

void Laboratory::setupShaders()
{
    mPrimitiveShaderProgram = new QOpenGLShaderProgram();
    mPrimitiveShaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kPrimitiveVertexShaderSource);
    mPrimitiveShaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kPrimitiveFragmentShaderSource);
    if (!mPrimitiveShaderProgram->link())
    {
        qWarning() << "Laboratory: primitive shader failed to link:" << mPrimitiveShaderProgram->log();
    }

    // mCrtShaderProgram is deliberately NOT compiled here - see
    // ensureCrtResources(), which lazily compiles it (and mSceneFbo) only
    // once the CRT shader is actually turned on.
}

void Laboratory::setupSceneBuffers()
{
    glGenVertexArrays(1, &mSceneVao);
    glGenBuffers(1, &mSceneVbo);

    glBindVertexArray(mSceneVao);
    glBindBuffer(GL_ARRAY_BUFFER, mSceneVbo);
    // No data uploaded here - the scene changes every tick, so
    // drawPrimitives() re-uploads it fresh each frame via glBufferData();
    // this just establishes the vertex layout (interleaved vec2 position +
    // vec4 color) against mSceneVbo, which stays bound to these attribute
    // slots for the life of the VAO.
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), reinterpret_cast<void *>(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Laboratory::setupCrtQuadBuffers()
{
    // Two triangles covering NDC space exactly (the CRT pass needs no
    // projection matrix - it's already drawing directly in clip space).
    // Texcoords match mSceneFbo's texel convention: world-y=0 (the
    // logical/visual top row of the scene) was rendered to NDC y=+1 (see
    // resizeGL()'s ortho(..., bottom=LABORATORY_HEIGHT, top=0, ...)), which
    // is also where OpenGL's texture t=1 ends up - so NDC-top maps straight
    // to texcoord-top with no flip needed.
    const GLfloat quad[] = {
        // x,     y,     u,    v
        -1.0f,  1.0f,  0.0f, 1.0f, // top-left
         1.0f,  1.0f,  1.0f, 1.0f, // top-right
         1.0f, -1.0f,  1.0f, 0.0f, // bottom-right
        -1.0f,  1.0f,  0.0f, 1.0f, // top-left
         1.0f, -1.0f,  1.0f, 0.0f, // bottom-right
        -1.0f, -1.0f,  0.0f, 0.0f, // bottom-left
    };

    glGenVertexArrays(1, &mCrtVao);
    glGenBuffers(1, &mCrtVbo);

    glBindVertexArray(mCrtVao);
    glBindBuffer(GL_ARRAY_BUFFER, mCrtVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), reinterpret_cast<void *>(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Laboratory::drawPrimitives(GLenum mode, const QVector<GLfloat> & vertices)
{
    if (vertices.isEmpty())
    {
        return;
    }

    glBindVertexArray(mSceneVao);
    glBindBuffer(GL_ARRAY_BUFFER, mSceneVbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * qsizetype(sizeof(GLfloat)), vertices.constData(), GL_DYNAMIC_DRAW);

    // 6 floats (x, y, r, g, b, a) per vertex - see setupSceneBuffers().
    glDrawArrays(mode, 0, GLsizei(vertices.size() / 6));

    glBindVertexArray(0);
}

void Laboratory::paintGL()
{
    if (mCrtShaderEnabled && mViewportWidth > 0 && mViewportHeight > 0)
    {
        ensureCrtResources(mViewportWidth, mViewportHeight);
    }

    if (mCrtShaderEnabled && mSceneFbo != nullptr && mCrtShaderProgram != nullptr && mCrtShaderProgram->isLinked())
    {
        mSceneFbo->bind();
        renderScene();
        mSceneFbo->release();

        renderCrtPass();
    }
    else
    {
        renderScene();
    }
}

void Laboratory::ensureCrtResources(int w, int h)
{
    if (mSceneFbo == nullptr || mSceneFbo->size() != QSize(w, h))
    {
        delete mSceneFbo;
        mSceneFbo = new QOpenGLFramebufferObject(w, h);
    }

    if (mCrtShaderProgram == nullptr)
    {
        mCrtShaderProgram = new QOpenGLShaderProgram();
        mCrtShaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kCrtVertexShaderSource);
        mCrtShaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kCrtFragmentShaderSource);
        if (!mCrtShaderProgram->link())
        {
            qWarning() << "Laboratory: CRT shader failed to link, falling back to unshaded rendering:"
                       << mCrtShaderProgram->log();
        }
    }
}

void Laboratory::renderCrtPass()
{
    glClear(GL_COLOR_BUFFER_BIT);

    mCrtShaderProgram->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mSceneFbo->texture());
    mCrtShaderProgram->setUniformValue("uScene", 0);
    mCrtShaderProgram->setUniformValue("uResolution", QVector2D(float(mViewportWidth), float(mViewportHeight)));
    mCrtShaderProgram->setUniformValue("uTime", float(mEffectsTimer.elapsed()) / 1000.0f);

    glBindVertexArray(mCrtVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    mCrtShaderProgram->release();
}

void Laboratory::renderScene()
{
    glClear(GL_COLOR_BUFFER_BIT);

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

    mPrimitiveShaderProgram->bind();
    mPrimitiveShaderProgram->setUniformValue("uProjection", mProjection);

    if (!effects.isEmpty())
    {
        // 6 floats per vertex (x, y, r, g, b, a), 2 vertices per ray - see
        // setupSceneBuffers()/drawPrimitives().
        QVector<GLfloat> lineVertices;
        lineVertices.reserve(effects.size() * kDeathEffectRayCount * 2 * 6);

        const qint64 now = mEffectsTimer.elapsed();
        for (const DeathEffectAnim & effect : qAsConst(effects))
        {
            const float t = qBound(0.0f, float(now - effect.spawnMs) / float(kDeathEffectDurationMs), 1.0f);
            const float ease = 1.0f - (1.0f - t) * (1.0f - t);
            const float outerR = 1.0f + ease * 5.0f;
            const float innerR = qMax(0.0f, outerR - 2.5f);
            const float alpha = 1.0f - t;

            // Pixel-art pass: snap the burst's origin to the same whole-unit
            // grid the animal/plant quads snap to below, so the death effect
            // reads as an extension of the blocky sprite it replaced rather
            // than a smoothly-floating vector burst.
            const float cx = qFloor(float(effect.pos.x())) + 0.5f;
            const float cy = qFloor(float(effect.pos.y())) + 0.5f;
            const float r = float(effect.color.redF());
            const float g = float(effect.color.greenF());
            const float b = float(effect.color.blueF());

            for (int i = 0; i < kDeathEffectRayCount; ++i)
            {
                const float angle = (2.0f * float(M_PI) * i) / kDeathEffectRayCount;
                const float c = qCos(angle);
                const float s = qSin(angle);
                lineVertices << cx + c * innerR << cy + s * innerR << r << g << b << alpha;
                lineVertices << cx + c * outerR << cy + s * outerR << r << g << b << alpha;
            }
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(1.5f);
        drawPrimitives(GL_LINES, lineVertices);
        glLineWidth(1.0f);
        glDisable(GL_BLEND);
    }

    if (!quads.isEmpty())
    {
        // 6 vertices (2 triangles) per quad, since core profile has no
        // GL_QUADS - fan-triangulated as (0,1,2) + (0,2,3), preserving the
        // old GL_QUADS corner order/winding exactly.
        QVector<GLfloat> triVertices;
        triVertices.reserve(quads.size() * 6 * 6);
        static constexpr int kTriangleCornerIndices[6] = { 0, 1, 2, 0, 2, 3 };
        for (const PaintQuad & quad : qAsConst(quads))
        {
            // Pixel-art pass: snap each animal/plant's draw position to the
            // nearest whole lab-grid cell before building its quad, instead
            // of drawing at its exact sub-unit simulated position. This is
            // purely a rendering-time rounding of captureFrame()'s already-
            // captured snapshot - it never touches the underlying
            // Animal::pos() the simulation actually uses - but it's what
            // turns continuous motion into the blocky, grid-aligned look
            // the rest of the theme uses.
            const GLfloat x = qFloor(quad.x) + 0.5f;
            const GLfloat y = qFloor(quad.y) + 0.5f;
            const GLfloat corners[4][2] = {
                { x - 1, y + 1 },
                { x + 1, y + 1 },
                { x + 1, y - 1 },
                { x - 1, y - 1 },
            };
            for (int cornerIndex : kTriangleCornerIndices)
            {
                triVertices << corners[cornerIndex][0] << corners[cornerIndex][1]
                            << quad.r << quad.g << quad.b << 1.0f;
            }
        }

        drawPrimitives(GL_TRIANGLES, triVertices);
    }

    mPrimitiveShaderProgram->release();
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

QSize Laboratory::targetSize() const
{
    QScrollArea * scrollArea = enclosingScrollArea();
    const QSize viewportSize = scrollArea ? scrollArea->viewport()->size() : QSize();
    if (!scrollArea || viewportSize.width() <= 0 || viewportSize.height() <= 0)
    {
        return QSize(qRound(LABORATORY_WIDTH * mZoom), qRound(LABORATORY_HEIGHT * mZoom));
    }

    const qreal fit = fitZoom();
    const qreal relativeZoom = (fit > 0.0) ? (mZoom / fit) : 1.0;
    return QSize(qRound(viewportSize.width() * relativeZoom), qRound(viewportSize.height() * relativeZoom));
}

void Laboratory::applyZoom(qreal newZoom, const QPoint & anchor)
{
    const qreal fit = fitZoom();
    newZoom = qBound(fit, newZoom, kMaxZoom);
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
    // Back to exactly the fit floor (e.g. the user wheel-zoomed all the way
    // back out) - resume auto-filling the panel on future resizes instead
    // of staying pinned at whatever pixel size this zoom-out landed on.
    mZoomedByUser = !qFuzzyCompare(mZoom, fit);
    resize(targetSize());

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
        // (see fitZoom()) may have moved too. If the user hasn't manually
        // zoomed in (mZoomedByUser), snap straight to the new fit so the
        // lab keeps exactly covering the panel (see targetSize()) with no
        // letterboxing on either axis; otherwise just re-clamp their
        // current zoom against the new floor, same as before.
        if (QScrollArea * scrollArea = enclosingScrollArea(); scrollArea && watched == scrollArea->viewport())
        {
            const QPoint anchor(scrollArea->viewport()->width() / 2, scrollArea->viewport()->height() / 2);
            applyZoom(mZoomedByUser ? mZoom : fitZoom(), anchor);
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

// DawnBringer 16 (DB16) palette, remapped from the original smooth-gradient
// per-species colors to the nearest/most-distinct DB16 swatches (see the
// pixel-art presentation pass: resources/theme.qss carries the same
// palette for the app chrome). #346524/#6daa2c (DB16's two greens) and
// #140c1c/#ffffff (canvas background / reserved bright text) are
// deliberately left out of this list so animal species never get
// confused with plants or with UI text drawn over the canvas.
QColor Laboratory::colorForIndex(int index) const
{
    switch(index)
    {
    case 0:
        return QColor(0x59, 0x7d, 0xce); // blue
    case 1:
        return QColor(0x6d, 0xc2, 0xca); // cyan
    case 2:
        return QColor(0x85, 0x95, 0xa1); // light steel
    case 3:
        return QColor(0xd0, 0x46, 0x48); // red
    case 4:
        return QColor(0xd2, 0x7d, 0x2c); // orange
    case 5:
        return QColor(0xd2, 0xaa, 0x99); // tan
    case 6:
        return QColor(0x85, 0x4c, 0x30); // brown
    case 7:
        return QColor(0x44, 0x24, 0x34); // dark maroon
    case 8:
        return QColor(0x30, 0x34, 0x6d); // dark indigo
    case 9:
        return QColor(0x4e, 0x4a, 0x4e); // slate
    case 10:
        return QColor(0x75, 0x71, 0x61); // khaki
    case 11:
        return QColor(0xd2, 0xd2, 0xd2); // light gray
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


