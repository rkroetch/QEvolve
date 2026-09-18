#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QMap>
#include <QTimer>
#include <QtCharts>
#include <QSettings>

#include "laboratory.h"
#include "metaprogression.h"

class HubDialog;
class QAction;
class QToolButton;

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void updateStatusBar();
    void toggleStart();
    void reset();

private slots:
    void onPlantPatternChanged(PlantPattern pattern);

    // --- Roguelike run wiring (Phase 2: UI/Game-Feel Engineer) -----------
    void showHub();
    void onStartRunRequested(RunConfig config);
    void onHubClosedWithoutRun();
    void onEpochAdvanced(int epoch);
    void onRunEnded(RunResult result);

    // Opens the modal Settings dialog (seeded with the laboratory's current
    // CRT-shader state), wiring its live-preview signal straight to
    // Laboratory::setCrtShaderEnabled().
    void showSettingsDialog();

protected:
    // Keeps mSettingsButton pinned to the laboratory viewport's top-right
    // corner as it's resized (splitter drags, window resize, zoom).
    bool eventFilter(QObject * watched, QEvent * event) override;

private:
    // Applies MetaProgression's permanent, purchased stat bonuses to
    // config.playerSpecies before a run begins. A metabolism/spawning-
    // energy *reduction* is the buff (lower upkeep, easier reproduction);
    // mutation-rate bonus is folded in additively (percentage points)
    // rather than multiplicatively so it still helps a starting kit whose
    // .SPC-defined mutation rate is 0%.
    void applyMetaBonuses(RunConfig & config) const;

    // Shows/hides and refreshes mRunHudLabel from Laboratory's current run
    // state; a no-op look-wise in sandbox mode (isRunModeActive() == false).
    void updateRunHud();

    // Reads a species' current population out of Laboratory::statistics()'s
    // cached "<Name> (<count>)" per-species header line (see
    // Species::statistics()) instead of touching Species::animals()
    // directly from the GUI thread - that string is already built
    // thread-safely under Laboratory's internal lock, so this avoids
    // racing the calculation thread while a run is active.
    int playerPopulationFromStatistics(const QString & speciesName) const;

    // Builds mPopulationSeries (one QLineSeries per animal Species, matched
    // to that species' color()) once at startup.
    void setupPopulationChart();

    // Repositions mSettingsButton to laboratoryScrollArea's top-right
    // corner (with a small margin); called on creation and from
    // eventFilter() whenever that scroll area resizes.
    void repositionSettingsButton();

private:
    Ui::MainWindow *ui;
    QSettings mSettings;
    QTimer mNumAnimalsTimer;
    QLabel mNumAnimalsLabel;
    QChart mChart;
    QLineSeries mSeries;

    // Optional per-species animal-count-over-time panel, mirroring the
    // calculations/second chart above but with one line series per animal
    // Species (colored to match that species' own color()). Hidden by
    // default, like the statistics/graph panels.
    QChart mPopulationChart;
    QMap<Species *, QLineSeries *> mPopulationSeries;

    // --- Roguelike run wiring (Phase 2: UI/Game-Feel Engineer) -----------
    MetaProgression mMeta;
    HubDialog * mHubDialog = nullptr;
    QAction * mActionNewRun = nullptr;
    QLabel * mRunHudLabel = nullptr;

    // Settings-gear overlay button, floating over the laboratory view's
    // top-right corner (see repositionSettingsButton()).
    QToolButton * mSettingsButton = nullptr;
};

#endif // MAINWINDOW_H
