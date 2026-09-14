#include <gtest/gtest.h>

#include "mutationchoice.h"

TEST(GenerateMutationChoices, ClampsCountToOneToThree)
{
    MutationChoiceContext context;
    EXPECT_EQ(generateMutationChoices(context, 0, 1).size(), 1);
    EXPECT_EQ(generateMutationChoices(context, -5, 1).size(), 1);
    EXPECT_EQ(generateMutationChoices(context, 2, 1).size(), 2);
    EXPECT_EQ(generateMutationChoices(context, 5, 1).size(), 3);
}

TEST(GenerateMutationChoices, SameSeedIsDeterministic)
{
    MutationChoiceContext context;
    context.movements.setMovement(1, 1, MoveUp);
    context.metabolism = 80;
    context.spawningEnergy = 900;

    const QVector<MutationChoice> a = generateMutationChoices(context, 3, 12345);
    const QVector<MutationChoice> b = generateMutationChoices(context, 3, 12345);

    ASSERT_EQ(a.size(), b.size());
    for (int i = 0; i < a.size(); ++i)
    {
        EXPECT_EQ(a[i].kind, b[i].kind);
        EXPECT_EQ(a[i].title, b[i].title);
        EXPECT_EQ(a[i].description, b[i].description);
        EXPECT_EQ(a[i].moveFriends, b[i].moveFriends);
        EXPECT_EQ(a[i].moveEnemies, b[i].moveEnemies);
        EXPECT_EQ(a[i].moveDirection, b[i].moveDirection);
        EXPECT_DOUBLE_EQ(a[i].statDeltaFraction, b[i].statDeltaFraction);
    }
}

TEST(GenerateMutationChoices, DifferentSeedsCanDiffer)
{
    MutationChoiceContext context;
    bool foundDifference = false;
    const QVector<MutationChoice> baseline = generateMutationChoices(context, 3, 1);
    for (quint32 seed = 2; seed < 30; ++seed)
    {
        const QVector<MutationChoice> other = generateMutationChoices(context, 3, seed);
        if (other[0].kind != baseline[0].kind || other[0].description != baseline[0].description)
        {
            foundDifference = true;
            break;
        }
    }
    EXPECT_TRUE(foundDifference);
}

TEST(GenerateMutationChoices, ThreeChoicesCoverAllThreeKinds)
{
    MutationChoiceContext context;
    const QVector<MutationChoice> choices = generateMutationChoices(context, 3, 999);
    ASSERT_EQ(choices.size(), 3);

    bool hasMovement = false;
    bool hasMetabolism = false;
    bool hasSpawning = false;
    for (const MutationChoice & choice : choices)
    {
        hasMovement |= (choice.kind == MutationChoiceKind::MovementTweak);
        hasMetabolism |= (choice.kind == MutationChoiceKind::MetabolismShift);
        hasSpawning |= (choice.kind == MutationChoiceKind::SpawningEnergyShift);
    }
    EXPECT_TRUE(hasMovement);
    EXPECT_TRUE(hasMetabolism);
    EXPECT_TRUE(hasSpawning);
}

TEST(GenerateMutationChoices, StatShiftsAreAlwaysAReductionWithinExpectedRange)
{
    MutationChoiceContext context;
    for (quint32 seed = 0; seed < 50; ++seed)
    {
        for (const MutationChoice & choice : generateMutationChoices(context, 3, seed))
        {
            if (choice.kind == MutationChoiceKind::MetabolismShift ||
                choice.kind == MutationChoiceKind::SpawningEnergyShift)
            {
                EXPECT_LT(choice.statDeltaFraction, 0.0);
                EXPECT_GE(choice.statDeltaFraction, -0.20);
                EXPECT_LE(choice.statDeltaFraction, -0.08);
            }
        }
    }
}

TEST(GenerateMutationChoices, MovementTweakOffersADifferentDirectionWhenPossible)
{
    MutationChoiceContext context; // Defaults to MoveStop everywhere.
    bool foundMovementTweak = false;
    for (quint32 seed = 0; seed < 50; ++seed)
    {
        for (const MutationChoice & choice : generateMutationChoices(context, 3, seed))
        {
            if (choice.kind == MutationChoiceKind::MovementTweak)
            {
                foundMovementTweak = true;
                EXPECT_NE(choice.moveDirection, MoveStop);
            }
        }
    }
    EXPECT_TRUE(foundMovementTweak);
}

TEST(GenerateMutationChoices, MovementTweakCellIsWithinTheThreeByThreeTable)
{
    MutationChoiceContext context;
    for (quint32 seed = 0; seed < 50; ++seed)
    {
        for (const MutationChoice & choice : generateMutationChoices(context, 3, seed))
        {
            if (choice.kind == MutationChoiceKind::MovementTweak)
            {
                EXPECT_GE(choice.moveFriends, 0);
                EXPECT_LE(choice.moveFriends, 2);
                EXPECT_GE(choice.moveEnemies, 0);
                EXPECT_LE(choice.moveEnemies, 2);
            }
        }
    }
}
