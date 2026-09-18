#ifndef LABORATORY_H
#define LABORATORY_H

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QElapsedTimer>
#include <QPaintEvent>
#include <QList>
#include <QFuture>
#include <QReadWriteLock>
#include <QMutex>
#include <QHash>
#include <QThread>
#include <QVector>
#include <QAtomicInteger>
#include <QColor>
#include <QPoint>
#include <atomic>

#include "common.h"
#include "runstate.h"
#include "difficultycurve.h"

class Animal;
class Species;
class CalculationThread;
class QScrollArea;
class QWheelEvent;
class QMouseEvent;
class QShowEvent;
class QEvent;
class QOpenGLShaderProgram;
class QOpenGLFramebufferObject;

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

// Renders via modern core-profile OpenGL (3.3 core - see main.cpp's
// QSurfaceFormat setup): all geometry goes through VAOs/VBOs and GLSL 330
// shaders (mPrimitiveShaderProgram for the animal/plant quads and
// death-effect lines, mCrtShaderProgram for the optional post-process
// pass). No fixed-function calls (glBegin/glEnd, the matrix stack, client-
// state vertex arrays) anywhere - QOpenGLFunctions_3_3_Core deliberately
// doesn't expose them, so misusing a removed function is a compile error
// rather than a silent no-op/GL_INVALID_OPERATION at runtime.
class Laboratory : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
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

    // --- Run mode (Phase 0: Run-Loop Engineer) ---
    // Sandbox mode (the default, unchanged from before) has no epochs and no
    // win/lose: isRunModeActive() stays false and none of the accessors
    // below advance. beginRun() switches into a bounded run; reset() (the
    // existing sandbox "Reset" action) drops back out of run mode, so the
    // toolbar's Reset always returns to a clean sandbox regardless of state.
    bool isRunModeActive() const { return mRunModeActive; }
    RunOutcome runOutcome() const { return mRunOutcome; }
    int currentEpoch() const { return mCurrentEpoch; }
    int targetEpochs() const { return mRunConfig.targetEpochs; }
    qint64 runTicks() const { return mRunTicks; }
    Species * playerSpecies() const { return mRunConfig.playerSpecies; }

    // Optional CRT post-process (scanlines/vignette/curvature) applied to
    // the whole canvas - see the Settings dialog (settings-gear overlay,
    // top-right of the laboratory view). Persisted via QSettings so it
    // survives an app restart; off by default.
    bool crtShaderEnabled() const { return mCrtShaderEnabled; }

public slots:
    //0 being fastest
    void setSpeed(int speed) { mSpeed.storeRelaxed(speed); }
    void setCrtShaderEnabled(bool enabled);
    void toggleStart();
    void start();
    void stop();
    void reset();
    void setPlantPattern(int pattern);
    void setSpeciesActive(Species * species, bool active);

    // Resets the lab and starts a bounded, tick-based run against config.
    // config.playerSpecies must already be one of species() and must be a
    // typeAnimal species. No-op (with a warning) otherwise.
    void beginRun(const RunConfig & config);
    // Ends the run early (e.g. player backs out to the hub mid-run) as a
    // loss if it was still in progress; does nothing in sandbox mode.
    void endRun();

signals:
    void plantPatternChanged(PlantPattern pattern);
    // Fired once per epoch boundary while a run is active, on the main
    // thread. Consumers (difficulty scaling, HUD) can read currentEpoch()
    // from within the slot.
    void epochAdvanced(int epoch);
    // Fired exactly once when a run concludes (win or loss). The lab is
    // already stopped by the time this fires.
    void runEnded(RunResult result);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    int heightForWidth(int w) const override;
    void wheelEvent(QWheelEvent * event) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void showEvent(QShowEvent * event) override;
    bool eventFilter(QObject * watched, QEvent * event) override;

private:
    QList<Species *> loadSpecies();
    void initActors();
    // The pre-CRT-shader body of paintGL(): clears, draws the death-effect
    // ray bursts, then the animal/plant quads. Renders into whatever
    // framebuffer is currently bound - the widget's own when the CRT shader
    // is off, or mSceneFbo (see ensureCrtResources()) when it's on.
    void renderScene();
    // Lazily (re)allocates mSceneFbo to match (w, h) and compiles/links
    // mCrtShaderProgram on first use. Both are kept alive for the rest of
    // the widget's life once allocated rather than torn down when the CRT
    // shader is toggled off, to avoid alloc/link churn on every toggle.
    void ensureCrtResources(int w, int h);
    // Draws mSceneFbo's texture as a screen-covering quad through
    // mCrtShaderProgram into whatever framebuffer is currently bound (the
    // widget's own, once mSceneFbo has been released).
    void renderCrtPass();

    // One-time (initializeGL()) setup: compiles/links mPrimitiveShaderProgram
    // and mCrtShaderProgram, and allocates every VAO/VBO this widget owns.
    void setupShaders();
    void setupSceneBuffers();
    void setupCrtQuadBuffers();
    // Re-uploads `vertices` into mSceneVbo and draws it as `mode` through
    // mPrimitiveShaderProgram - shared by renderScene()'s death-effect-line
    // and animal/plant-quad draw calls, which differ only in primitive type
    // and vertex data.
    void drawPrimitives(GLenum mode, const QVector<GLfloat> & vertices);
    // mAdvanceTimer's timeout target: drives run-state polling (if a run is
    // active) then repaints, replacing the old direct connect to update().
    void onAdvanceTick();
    QColor colorForIndex(int index) const;
    QScrollArea * enclosingScrollArea() const;
    void applyZoom(qreal newZoom, const QPoint & anchor);
    void handleCanvasClicked(const QPoint & localPos);

    // Polled every mAdvanceTimer tick (20ms, main thread) while a run is
    // active: advances the epoch counter from elapsed simulation ticks and
    // checks the win/lose conditions. A no-op in sandbox mode.
    void updateRunState();
    void finishRun(RunOutcome outcome);
    RunResult buildRunResult(RunOutcome outcome) const;

    // Applies one epoch's EncounterSpec (see difficultycurve.h): activates/
    // spawns any newly-introduced rival archetypes, retunes plant scarcity,
    // and rolls the epoch's hazard chance. Called once per epoch boundary
    // from updateRunState(), after epochAdvanced() is emitted for that epoch.
    void applyEncounterSpec(const EncounterSpec & spec);
    // Applies a hazard that won its chance roll in applyEncounterSpec().
    void triggerHazard(const HazardEvent & hazard);
    // Ticks down/reverts an in-progress MetabolismSurge hazard. Called once
    // per epoch boundary, before that epoch's new EncounterSpec is applied.
    void advanceHazards();

    // The zoom level at which the whole lab grid fits inside the enclosing
    // scroll area's current viewport with no scrolling required: whichever
    // axis is more constraining ends up exactly filling that dimension of
    // the viewport, and the other axis fits within it too, so nothing is
    // left cropped/needing a scroll. Recomputed from the live viewport size
    // rather than cached, since that size changes with the window.
    qreal fitZoom() const;

    // The widget size applyZoom()/the constructor should actually resize()
    // to for the current mZoom. Below/at fitZoom() (i.e. mZoomedByUser ==
    // false - see its declaration), this is exactly the enclosing scroll
    // area's viewport size, stretched non-uniformly to cover it completely
    // (no letterboxing on either axis) rather than the old uniform-scale
    // "contain" sizing that left a gap on whichever axis fit loosest. Above
    // fitZoom() (the user has wheel-zoomed in), both axes grow together
    // from that same filled baseline by mZoom/fitZoom(), so zooming in
    // never reintroduces new distortion beyond what filling already baked
    // in. Falls back to the plain LABORATORY_WIDTH/HEIGHT * mZoom formula
    // when there's no enclosing scroll area (or it has no usable size) yet
    // to measure against.
    QSize targetSize() const;

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
    // Player species population as of the last captureFrame() call, guarded
    // by mPaintMutex like mCachedNumAnimals/mCachedStatistics above.
    // updateRunState()'s playerExtinct check reads this instead of taking a
    // fresh positionLock read-lock on the main thread every tick - see the
    // long comment on captureFrame()'s player-population tracking in
    // laboratory.cpp for why a direct positionLock read from the main
    // thread here is unsafe.
    int mCachedPlayerAnimalCount = 0;

    // --- Run mode state (Phase 0) ---
    RunConfig mRunConfig;
    bool mRunModeActive = false;
    RunOutcome mRunOutcome = RunOutcome::InProgress;
    int mCurrentEpoch = 0;
    qint64 mRunTicks = 0;
    // CalculationThread::totalCycles() value at beginRun(); mRunTicks is
    // measured relative to this so ticksSurvived starts at 0 per run rather
    // than carrying over the cumulative cycle count from sandbox mode or an
    // earlier run in the same session.
    qint64 mRunStartTickBaseline = 0;

    // --- Hazard state (Phase 1.5 integration: difficulty curve -> Laboratory) ---
    // Baseline metabolism captured for every active animal species when a
    // MetabolismSurge hazard fires, so it can be restored exactly once the
    // surge's durationEpochs elapses. Empty when no surge is active.
    QHash<Species *, int> mMetabolismSurgeBaseline;
    int mMetabolismSurgeEpochsRemaining = 0;

    // --- Core-profile rendering resources (see setupShaders()/
    // setupSceneBuffers()/setupCrtQuadBuffers(), all called once from
    // initializeGL()) ---
    // Shared by both the death-effect lines and the animal/plant quads -
    // just a "transform position by uProjection, output per-vertex color"
    // shader (see kPrimitiveVertexShaderSource/kPrimitiveFragmentShaderSource
    // in laboratory.cpp).
    QOpenGLShaderProgram * mPrimitiveShaderProgram = nullptr;
    GLuint mSceneVao = 0;
    GLuint mSceneVbo = 0;
    // The logical (0,0)-(LABORATORY_WIDTH,LABORATORY_HEIGHT) -> clip-space
    // orthographic projection, rebuilt in resizeGL() whenever the viewport
    // size changes. Replaces the old fixed-function glOrtho()/matrix-stack
    // setup - uploaded to mPrimitiveShaderProgram's uProjection uniform.
    QMatrix4x4 mProjection;

    // --- Optional CRT post-process shader ---
    bool mCrtShaderEnabled = false;
    QOpenGLShaderProgram * mCrtShaderProgram = nullptr;
    QOpenGLFramebufferObject * mSceneFbo = nullptr;
    // Static (built once in setupCrtQuadBuffers()) full-screen NDC quad
    // used to draw mSceneFbo's texture through mCrtShaderProgram.
    GLuint mCrtVao = 0;
    GLuint mCrtVbo = 0;
    // Cached from the most recent resizeGL() call - paintGL() doesn't
    // receive the size directly, but needs it to (re)size mSceneFbo.
    int mViewportWidth = 0;
    int mViewportHeight = 0;

    // Fallback floor used only when the widget isn't (yet) hosted in a
    // scroll area to measure a viewport against - see fitZoom().
    static constexpr qreal kMinZoom = 0.5;
    static constexpr qreal kMaxZoom = 8.0;
    static constexpr qreal kDefaultZoom = 2.0;
    static constexpr qreal kZoomStepFactor = 1.1;
    // Above this many pixels of movement between press and release, a click
    // is treated as a drag-to-pan gesture instead of a selection click.
    static constexpr int kClickMoveTolerance = 4;
    // Selection hit-test radius, in logical lab units, around the click -
    // covers the visible quad (+-1 unit around an animal's position, see
    // paintGL()) plus a little slack.
    static constexpr qreal kHitTestRadius = 1.5;

    qreal mZoom = kDefaultZoom;
    // False until the user actually wheel-zooms: while false, every
    // viewport resize snaps mZoom straight to the new fitZoom() (see
    // eventFilter()), so the lab keeps exactly filling its panel with no
    // manual interaction needed. Set true on wheelEvent(); cleared again by
    // applyZoom() itself once the user zooms back out to exactly fitZoom(),
    // so zooming out all the way returns to auto-fill.
    bool mZoomedByUser = false;
    bool mViewportFilterInstalled = false;
    bool mDragging = false;
    QPoint mDragStartMouse;
    QPoint mPressLocalPos;
    int mDragStartHValue = 0;
    int mDragStartVValue = 0;
};

class CalculationThread : public QThread
{
public:
    explicit CalculationThread(Laboratory * laboratory, QObject * parent = nullptr);

    double cyclesPerSecond();
    // Thread-safe cumulative simulation cycle count, for Laboratory's
    // run-mode tick/epoch tracking (see updateRunState()). Unlike
    // cyclesPerSecond(), this doesn't reset anything on read.
    qint64 totalCycles() const;
    void stop();
    void run() override;

private:
    Laboratory * mLaboratory;
    std::atomic<bool> mStop{false};
    mutable QMutex mDataMutex;
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
