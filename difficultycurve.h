#ifndef DIFFICULTYCURVE_H
#define DIFFICULTYCURVE_H

#include <QString>
#include <QVector>

// --- Phase 1 (Difficulty & Encounter Designer) ------------------------------
// Pure, dependency-free difficulty numbers for the roguelike redesign (see
// "Claude outputs/qevolve-roguelike-redesign-plan.md", sections 2a/2c and the
// "Agent 3 - Difficulty & Encounter Designer" entry). Given the epoch number
// a run has reached (see runstate.h's computeEpoch(), fed by
// Laboratory::epochAdvanced(int)) and an optional meta-ascension tier (see
// computeMetaTier() below), this module answers "what should the world look
// like right now": which rival species/*.SPC archetypes to introduce and how
// many, how scarce plant resources should be, and what hazard event might
// fire.
//
// Deliberately mirrors runstate.h's style: plain structs and free functions,
// no Species/Animal/Laboratory dependency, safe to unit-test in isolation
// (see test/difficultycurve_test.cpp). A consumer of
// Laboratory::epochAdvanced(int) is expected to translate an EncounterSpec
// into actual species-loading/spawn/hazard calls in a follow-up integration
// phase - this module never touches Laboratory/Species/Animal itself.

// A hazard the run-loop may want to trigger for the epoch just entered.
// PlantDieOff and ResourceBloom are one-off effects; MetabolismSurge is a
// standing penalty for durationEpochs epochs once triggered.
enum class HazardType
{
    None,
    MetabolismSurge, // temporary metabolism increase for all animal species
    PlantDieOff,     // one-off cull of the current plant population
    ResourceBloom    // rare, temporary relief: plants regrow more easily
};

struct HazardEvent
{
    HazardType type = HazardType::None;

    // Probability in [0, 1] that the run-loop should roll this hazard for
    // the epoch it describes. Kept as a probability rather than a firm
    // yes/no so the actual coin-flip (and its RNG) stays with the caller -
    // this module never rolls dice itself, which keeps computeEncounterSpec()
    // a pure function of (epoch, metaTier) and therefore trivially testable.
    double chance = 0.0;

    // Severity if the hazard fires: for MetabolismSurge, a multiplier
    // applied to animal metabolism (>1 = harsher); for PlantDieOff, the
    // fraction of the current plant population to remove (0..1); for
    // ResourceBloom, the fractional temporary boost to plant capacity
    // (>0 = easier). Unused (0.0) when type is None.
    double magnitude = 0.0;

    // How many epochs the effect persists once triggered. 0 means
    // instantaneous (PlantDieOff, ResourceBloom); MetabolismSurge lasts
    // durationEpochs epochs before reverting.
    int durationEpochs = 0;
};

// One rival species to introduce this epoch: `archetype` names a base
// filename under species/ (without the .SPC extension, e.g. "RAPTORS"),
// and `count` is how many independent instances/colonies of it to spawn
// (e.g. RAPTORS' "pack" flavor spawns as more than one colony).
struct RivalSpawn
{
    QString archetype;
    int count = 1;
};

// Everything the run-loop needs to apply for one epoch transition.
struct EncounterSpec
{
    int epoch = 0;

    // Rival archetypes newly introduced *this* epoch only (empty on epochs
    // that don't introduce anything new - already-introduced rivals are
    // expected to persist in the simulation on their own, this module only
    // reports new arrivals, not a cumulative roster).
    QVector<RivalSpawn> rivals;

    // Multiplier the run-loop should apply to a newly-spawned rival's base
    // metabolism/spawning-energy aggression relative to its .SPC archetype
    // defaults - how much tougher rivals are running right now, folding in
    // both in-run epoch escalation and the meta-ascension floor. 1.0 = the
    // archetype's stock stats.
    double rivalStatMultiplier = 1.0;

    // Multiplier applied to common.h's MAX_NUM_PLANTS. Decreases with epoch
    // and meta tier (fewer plants the map can support at once).
    double plantCapMultiplier = 1.0;

    // Multiplier applied to common.h's PLANT_SPAWN_ENERGY. Increases with
    // epoch and meta tier (each plant takes longer to reach the energy
    // threshold needed to reproduce).
    double plantSpawnEnergyMultiplier = 1.0;

    // Hazard candidate for this epoch (chance == 0 means "none possible").
    HazardEvent hazard;
};

// The meta ascension/tier curve: numbers for a New-Game+-style progression,
// unlocked once a player clears a run (see RunOutcome::Won in runstate.h).
// Raises the floor on rival aggression and lowers starting resources for
// every tier cleared, gated behind however the meta-progression workstream
// decides to spend Evolution Points/clear counts - this module only supplies
// the numbers for a given tier index.
struct MetaTier
{
    int tier = 0;

    // Floor multiplier applied to rival metabolism/spawning-energy
    // aggression from epoch 0 of a run at this tier onward (before any
    // in-run epoch escalation is layered on top via rivalStatMultiplier).
    double rivalAggressionFloor = 1.0;

    // Multiplier applied to the player's starting resources (starting
    // MAX_NUM_PLANTS/PLANT_SPAWN_ENERGY, and/or starting spawning energy)
    // at the start of a run at this tier. <1.0 = harsher starting position.
    double startingResourceMultiplier = 1.0;
};

// computeEncounterSpec() is the main entry point: given the epoch a run has
// just reached and (optionally) the player's current meta-ascension tier
// (see computeMetaTier()), returns what the world should look like now.
// epoch is expected to be >= 0 (as computeEpoch() in runstate.h returns);
// negative epochs are clamped to 0. metaTier is expected to be >= 0;
// negative values are clamped to 0.
EncounterSpec computeEncounterSpec(int epoch, int metaTier = 0);

// computeMetaTier() is the cross-run ascension curve: given how many runs a
// player has cleared (RunOutcome::Won), returns the tier's stat floor for
// their *next* run. clearedRunCount is expected to be >= 0; negative values
// are clamped to 0 (tier 0, i.e. no ascension bonus/penalty yet).
MetaTier computeMetaTier(int clearedRunCount);

#endif // DIFFICULTYCURVE_H
