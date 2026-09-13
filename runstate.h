#ifndef RUNSTATE_H
#define RUNSTATE_H

#include <QString>
#include <QVector>

class Species;

// Frozen Phase 0 contract shared by the roguelike-redesign workstreams
// (management loop / difficulty-encounter / meta-progression). Mirrors the
// state a single bounded "run" produces so downstream systems (EP awards,
// unlock progress, hub UI, difficulty tuning) can consume it without
// depending on Laboratory internals.

enum class RunOutcome { InProgress, Won, Lost };

struct RunConfig {
    Species * playerSpecies = nullptr;
    qint64 ticksPerEpoch = 10000;
    int targetEpochs = 10;
};

struct SpeciesRunStats {
    QString name;
    int finalPopulation = 0;
    uint highestGeneration = 0;
    uint totalChildren = 0;
};

struct RunResult {
    RunOutcome outcome = RunOutcome::InProgress;
    int epochsCleared = 0;
    qint64 ticksSurvived = 0;
    QVector<SpeciesRunStats> speciesStats;
};

#endif // RUNSTATE_H
