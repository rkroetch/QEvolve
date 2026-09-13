#ifndef RUNSTATE_H
#define RUNSTATE_H

#include <QString>
#include <QVector>

class Species;

// --- Phase 0 (Run-Loop Engineer) -------------------------------------------
// This header defines the contract the run-loop publishes to every other
// redesign workstream: meta-progression consumes RunResult to award
// Evolution Points, the difficulty/encounter layer consumes epoch numbers via
// Laboratory::epochAdvanced(), and the UI layer consumes RunState/RunResult
// to render the HUD and end-of-run screens. Treat the shapes below as a
// stable public API once other agents build against them - extend with new
// fields rather than renaming/removing existing ones.

enum class RunOutcome
{
    InProgress,
    Won,
    Lost
};

// Tick-based epoch length and win condition for a run. ticksPerEpoch counts
// CalculationThread simulation cycles (see Laboratory::beginRun()), not
// wall-clock time, so pacing is independent of the speed slider.
struct RunConfig
{
    Species * playerSpecies = nullptr;
    // Default epoch length, in simulation ticks (CalculationThread cycles,
    // not wall-clock time - see Laboratory::beginRun()). 10000 was chosen
    // per design direction that 500 was far too short; still a placeholder
    // for the difficulty/encounter designer to tune against real playtests.
    qint64 ticksPerEpoch = 10000;
    int targetEpochs = 10;
    // Meta-ascension tier this run starts at (see computeMetaTier() in
    // difficultycurve.h) - 0 is a fresh, unascended run. Owned by the
    // meta-progression workstream; the run-loop just forwards it to
    // computeEncounterSpec() each epoch.
    int metaTier = 0;
};

// A snapshot of one animal species' standing at the moment a run ended.
// Deliberately minimal for Phase 0: everything here is derived from the
// live Animal/Species state at run-end time, so it's cheap and correct, but
// it can't see stats from animals that died earlier in the run (e.g. a
// species that peaked at 200 and crashed to 5 reports 5, not 200). If the
// meta-progression/difficulty agents need lifetime peaks or kill counts,
// add cumulative counters to Species/Animal and extend this struct rather
// than trying to reconstruct history after the fact.
struct SpeciesRunStats
{
    QString name;
    int finalPopulation = 0;
    uint highestGeneration = 0;
    uint totalChildren = 0;
};

// Published once via Laboratory::runEnded() when a run concludes.
struct RunResult
{
    RunOutcome outcome = RunOutcome::InProgress;
    int epochsCleared = 0;
    qint64 ticksSurvived = 0;
    QVector<SpeciesRunStats> speciesStats;
};

// Pure, dependency-free decision rules extracted so they're unit-testable
// without pulling in Species/Animal/Laboratory (which drag in Windows.h/GL
// via laboratory.h). Laboratory::updateRunState() is the only caller in the
// app; test/runstate_test.cpp exercises these directly.
inline int computeEpoch(qint64 ticksSurvived, qint64 ticksPerEpoch)
{
    if (ticksSurvived <= 0 || ticksPerEpoch <= 0)
    {
        return 0;
    }
    return int(ticksSurvived / ticksPerEpoch);
}

inline RunOutcome evaluateRunOutcome(bool playerExtinct, int currentEpoch, int targetEpochs)
{
    if (playerExtinct)
    {
        return RunOutcome::Lost;
    }
    if (currentEpoch >= targetEpochs)
    {
        return RunOutcome::Won;
    }
    return RunOutcome::InProgress;
}

#endif // RUNSTATE_H
