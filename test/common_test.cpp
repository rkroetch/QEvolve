#include <gtest/gtest.h>

#include "common.h"

TEST(WrapLabWidth, LeavesInRangeValuesUnchanged)
{
    int x = 5;
    wrapLabWidth(x);
    EXPECT_EQ(x, 5);
}

TEST(WrapLabWidth, WrapsAboveMaxToOne)
{
    int x = LABORATORY_WIDTH;
    wrapLabWidth(x);
    EXPECT_EQ(x, 1);
}

TEST(WrapLabWidth, WrapsBelowMinToMaxMinusOne)
{
    int x = 0;
    wrapLabWidth(x);
    EXPECT_EQ(x, LABORATORY_WIDTH - 1);
}

TEST(WrapLabHeight, WrapsAboveMaxToOne)
{
    int y = LABORATORY_HEIGHT + 3;
    wrapLabHeight(y);
    EXPECT_EQ(y, 1);
}

TEST(WrapLabHeight, WrapsBelowMinToMaxMinusOne)
{
    int y = -2;
    wrapLabHeight(y);
    EXPECT_EQ(y, LABORATORY_HEIGHT - 1);
}

TEST(ClampCellX, ClampsBelowZeroToZero)
{
    EXPECT_EQ(clampCellX(-10), 0);
}

TEST(ClampCellX, ClampsAboveMaxToMax)
{
    EXPECT_EQ(clampCellX(LABORATORY_WIDTH + 10), LABORATORY_WIDTH - 1);
}

TEST(ClampCellX, LeavesInRangeValuesUnchanged)
{
    EXPECT_EQ(clampCellX(42), 42);
}

TEST(ClampCellY, ClampsBelowZeroToZero)
{
    EXPECT_EQ(clampCellY(-1), 0);
}

TEST(ClampCellY, ClampsAboveMaxToMax)
{
    EXPECT_EQ(clampCellY(LABORATORY_HEIGHT + 1), LABORATORY_HEIGHT - 1);
}

TEST(RandIntInclusive, StaysWithinBounds)
{
    for (int i = 0; i < 200; ++i)
    {
        const int value = randIntInclusive(3, 7);
        EXPECT_GE(value, 3);
        EXPECT_LE(value, 7);
    }
}

TEST(RandIntInclusive, HandlesDegenerateRange)
{
    EXPECT_EQ(randIntInclusive(4, 4), 4);
}

TEST(Movements, DefaultsToStopEverywhere)
{
    const Movements movements;
    for (int friends = 0; friends <= 2; ++friends)
    {
        for (int enemies = 0; enemies <= 2; ++enemies)
        {
            EXPECT_EQ(movements.getMovement(friends, enemies), MoveStop);
        }
    }
}

TEST(Movements, SetAndGetRoundTrips)
{
    Movements movements;
    movements.setMovement(1, 2, MoveUpRight);
    EXPECT_EQ(movements.getMovement(1, 2), MoveUpRight);
    // Unrelated buckets remain untouched.
    EXPECT_EQ(movements.getMovement(0, 0), MoveStop);
}

TEST(Movements, SetMovementClampsOutOfRangeIndices)
{
    Movements movements;
    movements.setMovement(-5, 10, MoveLeft);
    EXPECT_EQ(movements.getMovement(0, 2), MoveLeft);
    EXPECT_EQ(movements.getMovement(-100, 100), MoveLeft);
}

TEST(Movements, ToggleMovementAdvancesByOneAndWraps)
{
    Movements movements;
    movements.setMovement(0, 0, MoveMax == 0 ? MoveUp : static_cast<MovementDirections>(MoveMax - 1));
    movements.toggleMovement(0, 0);
    EXPECT_EQ(movements.getMovement(0, 0), MoveUp);
}

TEST(MovementDirectionString, ReturnsDistinctNonEmptyNames)
{
    EXPECT_STREQ(movementDirectionString(MoveUp), "MoveUp");
    EXPECT_STREQ(movementDirectionString(MoveGo), "MoveGo");
    EXPECT_STRNE(movementDirectionString(MoveUp), movementDirectionString(MoveDown));
}
