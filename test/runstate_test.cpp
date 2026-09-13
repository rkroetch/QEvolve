#include <gtest/gtest.h>

#include "runstate.h"

TEST(ComputeEpoch, ZeroTicksIsEpochZero)
{
    EXPECT_EQ(computeEpoch(0, 500), 0);
}

TEST(ComputeEpoch, BelowOneEpochLengthIsEpochZero)
{
    EXPECT_EQ(computeEpoch(499, 500), 0);
}

TEST(ComputeEpoch, ExactMultipleAdvancesEpoch)
{
    EXPECT_EQ(computeEpoch(500, 500), 1);
    EXPECT_EQ(computeEpoch(1000, 500), 2);
}

TEST(ComputeEpoch, RoundsDownWithinAnEpoch)
{
    EXPECT_EQ(computeEpoch(1499, 500), 2);
}

TEST(ComputeEpoch, NonPositiveTicksPerEpochIsSafe)
{
    EXPECT_EQ(computeEpoch(1000, 0), 0);
    EXPECT_EQ(computeEpoch(1000, -5), 0);
}

TEST(ComputeEpoch, NegativeTicksIsSafe)
{
    EXPECT_EQ(computeEpoch(-10, 500), 0);
}

TEST(EvaluateRunOutcome, ExtinctionAlwaysLosesEvenAtTargetEpoch)
{
    EXPECT_EQ(evaluateRunOutcome(true, 10, 10), RunOutcome::Lost);
}

TEST(EvaluateRunOutcome, ReachingTargetEpochWins)
{
    EXPECT_EQ(evaluateRunOutcome(false, 10, 10), RunOutcome::Won);
}

TEST(EvaluateRunOutcome, PastTargetEpochStillWins)
{
    EXPECT_EQ(evaluateRunOutcome(false, 11, 10), RunOutcome::Won);
}

TEST(EvaluateRunOutcome, BelowTargetWithSurvivorsIsInProgress)
{
    EXPECT_EQ(evaluateRunOutcome(false, 9, 10), RunOutcome::InProgress);
}

TEST(RunResult, DefaultsToInProgressWithNoStats)
{
    RunResult result;
    EXPECT_EQ(result.outcome, RunOutcome::InProgress);
    EXPECT_EQ(result.epochsCleared, 0);
    EXPECT_EQ(result.ticksSurvived, 0);
    EXPECT_TRUE(result.speciesStats.isEmpty());
}
