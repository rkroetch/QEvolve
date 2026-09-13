#include <gtest/gtest.h>

#include "evolutionpoints.h"

namespace {

RunResult makeResult(RunOutcome outcome, int epochsCleared, qint64 ticksSurvived,
    std::initializer_list<SpeciesRunStats> stats)
{
    RunResult result;
    result.outcome = outcome;
    result.epochsCleared = epochsCleared;
    result.ticksSurvived = ticksSurvived;
    for (const auto & s : stats)
    {
        result.speciesStats.append(s);
    }
    return result;
}

} // namespace

TEST(EvolutionPointsTest, EmptyLossEarnsNothing)
{
    RunResult result = makeResult(RunOutcome::Lost, 0, 0, {});
    EXPECT_EQ(evolutionPointsEarned(result), 0);
}

TEST(EvolutionPointsTest, InProgressIsTreatedLikeALoss)
{
    RunResult inProgress = makeResult(RunOutcome::InProgress, 3, 1000,
        { SpeciesRunStats{"Rabbits", 20, 1, 5} });
    RunResult lost = makeResult(RunOutcome::Lost, 3, 1000,
        { SpeciesRunStats{"Rabbits", 20, 1, 5} });

    EXPECT_EQ(evolutionPointsEarned(inProgress), evolutionPointsEarned(lost));
}

TEST(EvolutionPointsTest, MoreEpochsClearedEarnsMoreEP)
{
    RunResult fewer = makeResult(RunOutcome::Lost, 1, 1000, {});
    RunResult more = makeResult(RunOutcome::Lost, 5, 1000, {});

    EXPECT_LT(evolutionPointsEarned(fewer), evolutionPointsEarned(more));
}

TEST(EvolutionPointsTest, HigherFinalPopulationEarnsMoreEP)
{
    RunResult small = makeResult(RunOutcome::Lost, 2, 1000, { SpeciesRunStats{"A", 10, 0, 0} });
    RunResult large = makeResult(RunOutcome::Lost, 2, 1000, { SpeciesRunStats{"A", 200, 0, 0} });

    EXPECT_LT(evolutionPointsEarned(small), evolutionPointsEarned(large));
}

TEST(EvolutionPointsTest, HighestGenerationAcrossSpeciesIsUsed)
{
    RunResult result = makeResult(RunOutcome::Lost, 1, 1000,
        { SpeciesRunStats{"A", 5, 2, 0}, SpeciesRunStats{"B", 5, 7, 0} });

    EvolutionPointsBreakdown breakdown = computeEvolutionPoints(result);
    EXPECT_EQ(breakdown.highestGeneration, 7u);
}

TEST(EvolutionPointsTest, ChildrenAreSummedAcrossSpecies)
{
    RunResult result = makeResult(RunOutcome::Lost, 1, 1000,
        { SpeciesRunStats{"A", 5, 0, 3}, SpeciesRunStats{"B", 5, 0, 4} });

    EvolutionPointsBreakdown breakdown = computeEvolutionPoints(result);
    EXPECT_EQ(breakdown.totalChildren, 7u);
}

TEST(EvolutionPointsTest, WinEarnsStrictlyMoreThanAnIdenticalLoss)
{
    RunResult won = makeResult(RunOutcome::Won, 10, 100000, { SpeciesRunStats{"A", 50, 3, 20} });
    RunResult lost = makeResult(RunOutcome::Lost, 10, 100000, { SpeciesRunStats{"A", 50, 3, 20} });

    EXPECT_GT(evolutionPointsEarned(won), evolutionPointsEarned(lost));
}

TEST(EvolutionPointsTest, TotalEPIsNeverNegative)
{
    EvolutionPointsWeights weights;
    weights.perEpochCleared = -1000.0; // Pathological weights shouldn't produce negative EP.
    RunResult result = makeResult(RunOutcome::Lost, 5, 1000, {});

    EXPECT_GE(evolutionPointsEarned(result, weights), 0);
}

TEST(EvolutionPointsTest, CustomWeightsAreRespected)
{
    EvolutionPointsWeights weights;
    weights.perEpochCleared = 10.0;
    weights.perFinalPopulation = 0.0;
    weights.perGeneration = 0.0;
    weights.perChild = 0.0;
    weights.winBonus = 0.0;
    weights.lossMultiplier = 1.0;

    RunResult result = makeResult(RunOutcome::Lost, 4, 0, {});
    EXPECT_EQ(evolutionPointsEarned(result, weights), 40);
}

TEST(EvolutionPointsTest, DefaultWeightsProduceExpectedBreakdown)
{
    EvolutionPointsWeights weights; // Defaults, made explicit for a stable expected value.
    RunResult result = makeResult(RunOutcome::Won, 10, 100000,
        { SpeciesRunStats{"A", 40, 5, 60} });

    EvolutionPointsBreakdown breakdown = computeEvolutionPoints(result, weights);

    EXPECT_DOUBLE_EQ(breakdown.epochsPoints, 250.0);   // 10 * 25.0
    EXPECT_DOUBLE_EQ(breakdown.populationPoints, 6.0); // 40 * 0.15
    EXPECT_DOUBLE_EQ(breakdown.generationPoints, 20.0);// 5 * 4.0
    EXPECT_DOUBLE_EQ(breakdown.childrenPoints, 3.0);   // 60 * 0.05
    EXPECT_DOUBLE_EQ(breakdown.outcomeBonus, 100.0);
    EXPECT_DOUBLE_EQ(breakdown.rawTotal, 379.0);
    EXPECT_DOUBLE_EQ(breakdown.total, 379.0); // Win: no loss multiplier applied.
    EXPECT_EQ(breakdown.totalEP(), 379);
}
