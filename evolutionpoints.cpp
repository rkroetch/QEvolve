#include "evolutionpoints.h"

#include <algorithm>
#include <cmath>

int EvolutionPointsBreakdown::totalEP() const
{
    return static_cast<int>(std::lround(std::max(0.0, total)));
}

EvolutionPointsBreakdown computeEvolutionPoints(const RunResult & result, const EvolutionPointsWeights & weights)
{
    EvolutionPointsBreakdown breakdown;

    int finalPopulation = 0;
    uint highestGeneration = 0;
    uint totalChildren = 0;
    for (const SpeciesRunStats & stats : result.speciesStats)
    {
        finalPopulation += stats.finalPopulation;
        highestGeneration = std::max(highestGeneration, stats.highestGeneration);
        totalChildren += stats.totalChildren;
    }

    breakdown.epochsCleared = result.epochsCleared;
    breakdown.finalPopulation = finalPopulation;
    breakdown.highestGeneration = highestGeneration;
    breakdown.totalChildren = totalChildren;
    breakdown.won = (result.outcome == RunOutcome::Won);

    breakdown.epochsPoints = weights.perEpochCleared * std::max(0, result.epochsCleared);
    breakdown.populationPoints = weights.perFinalPopulation * std::max(0, finalPopulation);
    breakdown.generationPoints = weights.perGeneration * static_cast<double>(highestGeneration);
    breakdown.childrenPoints = weights.perChild * static_cast<double>(totalChildren);
    breakdown.outcomeBonus = breakdown.won ? weights.winBonus : 0.0;

    breakdown.rawTotal = breakdown.epochsPoints + breakdown.populationPoints
        + breakdown.generationPoints + breakdown.childrenPoints + breakdown.outcomeBonus;

    breakdown.total = breakdown.won ? breakdown.rawTotal : breakdown.rawTotal * weights.lossMultiplier;

    return breakdown;
}

int evolutionPointsEarned(const RunResult & result, const EvolutionPointsWeights & weights)
{
    return computeEvolutionPoints(result, weights).totalEP();
}
