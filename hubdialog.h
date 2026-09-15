#ifndef HUBDIALOG_H
#define HUBDIALOG_H

#include <QDialog>
#include <QList>

#include "common.h" // PlantPattern
#include "metaprogression.h"
#include "runstate.h"

class Species;
class QListWidget;
class QListWidgetItem;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

// Between-run "hub"/loadout screen: lets the player spend Evolution
// Points (via a MetaProgression) on unlocks - starting species presets,
// permanent stat bonuses, biomes, cosmetics, and revive/reroll charges -
// then pick a starting species + biome and kick off the next run.
//
// Deliberately self-contained: it holds no Laboratory/MainWindow
// reference and does not construct or own any Species. The caller wires
// it up by:
//   - constructing it with a MetaProgression the app keeps around,
//   - calling setAvailableSpecies() with the Species* pointers to offer
//     (normally Laboratory::species() filtered to Species::typeAnimal;
//     per runstate.h, RunConfig::playerSpecies must be one of those),
//   - connecting to startRunRequested(RunConfig) to actually start a run
//     (e.g. via Laboratory::beginRun()) and, if desired, to
//     closedWithoutStartingRun() for a "cancelled" path,
//   - calling applyRunResult(RunResult) after a run ends to award EP and
//     refresh the hub display.
class HubDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HubDialog(MetaProgression * meta, QWidget * parent = nullptr);

    MetaProgression * metaProgression() const;

    // Species the player may choose as a starting preset. HubDialog does
    // not take ownership; only species whose name matches an unlocked
    // "species.<NAME>" catalog entry are actually selectable.
    void setAvailableSpecies(const QList<Species *> & species);
    QList<Species *> availableSpecies() const;

    // Enough state, together, to build a RunConfig.
    Species * selectedSpecies() const;
    PlantPattern selectedBiome() const;
    qint64 ticksPerEpoch() const;
    int targetEpochs() const;

    void setTicksPerEpoch(qint64 ticks);
    void setTargetEpochs(int epochs);

public slots:
    // Scores a just-finished run, awards EP via the MetaProgression, and
    // refreshes the hub display (balance, affordability, unlock lists).
    void applyRunResult(const RunResult & result);

    // Re-pulls state from MetaProgression and re-renders every panel.
    // Called automatically after purchases/applyRunResult(); exposed for
    // callers that award EP directly via MetaProgression::awardEP().
    void refresh();

    // Overridden so closing the dialog (window X, Esc, cancel) without
    // having started a run emits closedWithoutStartingRun() exactly once.
    void reject() override;

signals:
    // Emitted when the player clicks "Start Run" with a valid selection.
    void startRunRequested(RunConfig config);

    // Emitted when the dialog is closed/rejected without starting a run.
    void closedWithoutStartingRun();

private slots:
    void onCatalogSelectionChanged();
    void onPurchaseClicked();
    void onStartRunClicked();
    void onSpeciesComboChanged(int index);
    void onBiomeComboChanged(int index);

private:
    void buildUi();
    void populateCatalogList();
    void populateSpeciesCombo();
    void populateBiomeCombo();
    void updateEpLabel();
    void updatePurchaseButtonState();
    void updateStartButtonState();

    MetaProgression * mMeta = nullptr;
    QList<Species *> mAvailableSpecies;
    QList<Species *> mSelectableSpecies; // Parallel to mSpeciesCombo's items.

    QLabel * mEpLabel = nullptr;
    QListWidget * mCatalogList = nullptr;
    QLabel * mCatalogDescriptionLabel = nullptr;
    QPushButton * mPurchaseButton = nullptr;
    QComboBox * mSpeciesCombo = nullptr;
    QComboBox * mBiomeCombo = nullptr;
    QSpinBox * mTargetEpochsSpin = nullptr;
    QPushButton * mStartButton = nullptr;

    Species * mSelectedSpecies = nullptr;
    PlantPattern mSelectedBiome = plantPatternOneGroup;
    // Tracks RunConfig's own default rather than duplicating a literal here
    // - RunConfig{}.ticksPerEpoch is the single source of truth for epoch
    // pacing (see runstate.h; a prior hardcoded 10000 here silently drifted
    // out of sync when that default was retuned to 800 in the Phase 3
    // balance pass, so every hub-started run ignored the retuned pacing).
    qint64 mTicksPerEpoch = RunConfig().ticksPerEpoch;
    bool mRunRequested = false;
};

#endif // HUBDIALOG_H
