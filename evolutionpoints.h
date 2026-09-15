#ifndef EVOLUTIONPOINTS_H
#define EVOLUTIONPOINTS_H

#include "runstate.h"

// Evolution Points (EP) are the between-run currency spent in the hub/
// loadout screen (see HubDialog / MetaProgression). This header is
// intentionally free of any Qt widget dependency - like runstate.h, it is
// plain data plus pure functions - so it can be unit tested and reused by
// both the meta-progression UI and the difficulty/encounter workstream
// without pulling in the rest of the application.

// Tunable weights for converting a completed run's RunResult into EP.
// Kept as a separate, default-constructible struct so callers (tests, the
// hub UI, a future difficulty-scaling system) can tune or preview payouts
// without touching the formula itself.
//
// Phase 3 balance pass: these defaults were checked (not just guessed)
// against benchmarks/balance_simulator.cpp's simulated data and left
// unchanged. With the retuned difficulty curve (see difficultycurve.cpp),
// the free/default starting kit (RABBITS) earns ~450-580 EP/run on
// average; the cheapest metaprogression.cpp unlocks (50-150 EP) are
// affordable in under a run, and unlocking every catalog entry costs
// ~5700 EP total - roughly 11-12 runs, a reasonable "sustained play"
// target for a small-scope roguelike. See the balance-pass commit message
// for the full numbers. One caveat worth flagging: perFinalPopulation
// below sums RunResult::speciesStats across every tracked animal species,
// which in practice (see Laboratory::buildRunResult()) is every bestiary
// species, not just the player's - a run with a large rival population at
// the end earns more EP from this term than an identical run against
// weaker rivals, independent of the player's own performance. Not fixed
// in this pass (RunResult has no player-species marker to filter on
// without a struct-shape change, out of scope here) - flagged for a
// follow-up.
struct EvolutionPointsWeights
{
    // EP awarded per epoch fully cleared.
    double perEpochCleared = 25.0;

    // EP per point of final population, summed across every tracked
    // species in RunResult::speciesStats (normally just the player's
    // species, but the struct supports tracking more).
    double perFinalPopulation = 0.15;

    // EP per generation reached, using the highest generation seen
    // across tracked species.
    double perGeneration = 4.0;

    // EP per child born across tracked species over the run.
    double perChild = 0.05;

    // Flat EP bonus applied only when the run outcome is RunOutcome::Won.
    double winBonus = 100.0;

    // Multiplier applied to the whole total when the run did not end in
    // a win (RunOutcome::Lost or InProgress), so a loss still earns
    // partial credit but is always worth less than an equivalent win.
    double lossMultiplier = 0.5;
};

// A breakdown of how an EP payout was computed, so UI (e.g. a run-summary
// screen) can show the player where their points came from instead of
// just a final number.
struct EvolutionPointsBreakdown
{
    // Inputs, copied out of RunResult for convenient display.
    int epochsCleared = 0;
    int finalPopulation = 0;
    uint highestGeneration = 0;
    uint totalChildren = 0;
    bool won = false;

    // Individual weighted terms.
    double epochsPoints = 0.0;
    double populationPoints = 0.0;
    double generationPoints = 0.0;
    double childrenPoints = 0.0;
    double outcomeBonus = 0.0;

    double rawTotal = 0.0; // Sum of the terms above, before lossMultiplier.
    double total = 0.0;    // Final EP value, after any loss multiplier.

    // total(), rounded to a whole EP amount. Never negative.
    int totalEP() const;
};

// Computes the EP payout for a completed run. RunOutcome::InProgress is
// treated the same as RunOutcome::Lost (no win bonus, lossMultiplier
// applies) so this is safe to call defensively/early.
EvolutionPointsBreakdown computeEvolutionPoints(
    const RunResult & result,
    const EvolutionPointsWeights & weights = EvolutionPointsWeights());

// Convenience wrapper returning just the rounded EP total.
int evolutionPointsEarned(
    const RunResult & result,
    const EvolutionPointsWeights & weights = EvolutionPointsWeights());

#endif // EVOLUTIONPOINTS_H
