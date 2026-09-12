#include <gtest/gtest.h>

#include <memory>

#include "animal.h"
#include "deatheffects.h"
#include "species.h"

namespace {

class AnimalTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mSpecies = std::make_unique<Species>(Species::typeAnimal);
        // One active animal (created internally by initialize()) plus a
        // handful of inactive ones available for spawnSelf() to draw from.
        mSpecies->initialize(0, 10, 900);
    }

    void TearDown() override
    {
        mSpecies.reset();
    }

    Animal * animal() const { return mSpecies->animals().first(); }

    std::unique_ptr<Species> mSpecies;
};

// Unlike AnimalTest, this fixture doesn't pre-populate the species with a
// randomly-positioned animal, so Merge/Split neighbor tests can place
// animals at exact, known cells without risking incidental collisions.
class AnimalMovementNeighborTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mSpecies = std::make_unique<Species>(Species::typeAnimal);
    }

    void TearDown() override
    {
        mSpecies.reset();
    }

    std::unique_ptr<Species> mSpecies;
};

// Places a fully-controlled Animal into `species` at `pos` with a known
// direction and gene table, mirroring species_test.cpp's addAnimalAt() but
// exposing the extra fields Merge/Split tests need to control.
Animal * placeAnimal(Species & species, QPointF pos, QPointF direction, const Movements & movements)
{
    auto * animal = new Animal();
    animal->initialize(pos, &species, 900, 1000, 100, movements, direction, nullptr);
    species.addAnimal(animal->cellX(), animal->cellY(), animal);
    return animal;
}

} // namespace

TEST_F(AnimalTest, ConstructionRecordsEnergyAndSpecies)
{
    Animal * a = animal();
    EXPECT_DOUBLE_EQ(a->energy(), 900.0);
    EXPECT_EQ(a->species(), mSpecies.get());
    EXPECT_EQ(a->statistics().mAge, 0u);
    EXPECT_EQ(a->statistics().mGeneration, 0u);
}

TEST_F(AnimalTest, SetMetabolismClampsToOneHundred)
{
    Animal * a = animal();
    a->setMetabolism(500);
    EXPECT_EQ(a->metabolism(), 100);
    a->setMetabolism(-5);
    EXPECT_EQ(a->metabolism(), 1);
    a->setMetabolism(42);
    EXPECT_EQ(a->metabolism(), 42);
}

TEST_F(AnimalTest, SetSpawningEnergyClampsToValidRange)
{
    Animal * a = animal();
    a->setSpawningEnergy(1);
    EXPECT_EQ(a->spawningEnergy(), 10);
    a->setSpawningEnergy(999999);
    EXPECT_EQ(a->spawningEnergy(), 2500);
    a->setSpawningEnergy(1500);
    EXPECT_EQ(a->spawningEnergy(), 1500);
}

TEST_F(AnimalTest, SetEnergyUpdatesStatistics)
{
    Animal * a = animal();
    a->setEnergy(123.5);
    EXPECT_DOUBLE_EQ(a->energy(), 123.5);
    EXPECT_DOUBLE_EQ(a->statistics().mEnergy, 123.5);
}

TEST_F(AnimalTest, SetPosUpdatesCellCoordinates)
{
    Animal * a = animal();
    a->setPos(QPointF(12.7, LABORATORY_HEIGHT - 19.8));
    EXPECT_EQ(a->cellX(), 12);
    EXPECT_EQ(a->cellY(), LABORATORY_HEIGHT - 20);
}

TEST_F(AnimalTest, TryClaimEatenSucceedsOnlyOnce)
{
    Animal * a = animal();
    EXPECT_FALSE(a->isEaten());
    EXPECT_TRUE(a->tryClaimEaten());
    EXPECT_TRUE(a->isEaten());
    EXPECT_FALSE(a->tryClaimEaten());
}

TEST_F(AnimalTest, MarkAsEatenForcesEatenState)
{
    Animal * a = animal();
    a->markAsEaten();
    EXPECT_TRUE(a->isEaten());
}

TEST_F(AnimalTest, MovementMapsCardinalDirectionsToUnitVectors)
{
    Movements movements;
    movements.setMovement(0, 0, MoveUp);
    movements.setMovement(0, 1, MoveRight);
    movements.setMovement(0, 2, MoveStop);
    Animal a(QPointF(50, 50), mSpecies.get(), 900, 1000, 100, movements);

    EXPECT_EQ(a.movement(0, 0, QPointF(0, 0)), QPointF(0.0, -1.0));
    EXPECT_EQ(a.movement(0, 1, QPointF(0, 0)), QPointF(1.0, 0.0));
    EXPECT_EQ(a.movement(0, 2, QPointF(1, 1)), QPointF(0.0, 0.0));
}

TEST_F(AnimalTest, MovementGoReturnsCurrentDirection)
{
    Movements movements;
    movements.setMovement(0, 0, MoveGo);
    Animal a(QPointF(50, 50), mSpecies.get(), 900, 1000, 100, movements);

    const QPointF direction(0.5, -0.5);
    EXPECT_EQ(a.movement(0, 0, direction), direction);
}

TEST_F(AnimalMovementNeighborTest, MergeWithNoNeighborKeepsCurrentDirection)
{
    // Regression test: firstNeighbor() used to find the querying animal in
    // its own cell and "merge" with itself, so this never used to reach the
    // no-neighbor fallback despite there being no other animal around.
    Movements movements;
    movements.setMovement(0, 0, MoveMerge);
    Animal * self = placeAnimal(*mSpecies, QPointF(50, 50), QPointF(0, 0), movements);

    const QPointF curDirection(0.5, -0.5);
    EXPECT_EQ(self->movement(0, 0, curDirection), curDirection);
}

TEST_F(AnimalMovementNeighborTest, MergeWithNeighborAdoptsNeighborDirection)
{
    Movements movements;
    movements.setMovement(0, 0, MoveMerge);
    Animal * self = placeAnimal(*mSpecies, QPointF(50, 50), QPointF(0, 0), movements);
    placeAnimal(*mSpecies, QPointF(50, 50), QPointF(1.0, 0.0), Movements());

    EXPECT_EQ(self->movement(0, 0, QPointF(0, -1)), QPointF(1.0, 0.0));
}

TEST_F(AnimalMovementNeighborTest, MergeFindsNeighborInAdjacentCell)
{
    Movements movements;
    movements.setMovement(0, 0, MoveMerge);
    Animal * self = placeAnimal(*mSpecies, QPointF(50, 50), QPointF(0, 0), movements);
    placeAnimal(*mSpecies, QPointF(49, 50), QPointF(0.0, 1.0), Movements());

    EXPECT_EQ(self->movement(0, 0, QPointF(0, -1)), QPointF(0.0, 1.0));
}

TEST_F(AnimalMovementNeighborTest, SplitWithNoNeighborKeepsCurrentDirection)
{
    // Regression test: same self-as-neighbor issue as Merge above meant this
    // fallback was previously unreachable, and MoveSplit always randomized.
    Movements movements;
    movements.setMovement(0, 0, MoveSplit);
    Animal * self = placeAnimal(*mSpecies, QPointF(50, 50), QPointF(0, 0), movements);

    const QPointF curDirection(0.5, -0.5);
    EXPECT_EQ(self->movement(0, 0, curDirection), curDirection);
}

TEST_F(AnimalMovementNeighborTest, SplitMovesDirectlyAwayFromNeighbor)
{
    Movements movements;
    movements.setMovement(0, 0, MoveSplit);
    Animal * self = placeAnimal(*mSpecies, QPointF(50, 50), QPointF(0, 0), movements);
    placeAnimal(*mSpecies, QPointF(51, 50), QPointF(0, 0), Movements());

    EXPECT_EQ(self->movement(0, 0, QPointF(0, -1)), QPointF(-1.0, 0.0));
}

TEST_F(AnimalMovementNeighborTest, SplitMovesDiagonallyAwayFromNeighbor)
{
    Movements movements;
    movements.setMovement(0, 0, MoveSplit);
    Animal * self = placeAnimal(*mSpecies, QPointF(50, 50), QPointF(0, 0), movements);
    placeAnimal(*mSpecies, QPointF(51, 51), QPointF(0, 0), Movements());

    EXPECT_EQ(self->movement(0, 0, QPointF(0, -1)), QPointF(-0.5, -0.5));
}

TEST_F(AnimalMovementNeighborTest, SplitPicksRandomDirectionWhenCoincidentWithNeighbor)
{
    Movements movements;
    movements.setMovement(0, 0, MoveSplit);
    Animal * self = placeAnimal(*mSpecies, QPointF(50, 50), QPointF(0, 0), movements);
    placeAnimal(*mSpecies, QPointF(50, 50), QPointF(0, 0), Movements());

    for (int i = 0; i < 20; ++i)
    {
        const QPointF result = self->movement(0, 0, QPointF(0, -1));
        EXPECT_TRUE(result.x() == -0.5 || result.x() == 0.0 || result.x() == 0.5);
        EXPECT_TRUE(result.y() == -0.5 || result.y() == 0.0 || result.y() == 0.5);
    }
}

TEST_F(AnimalTest, ExecuteMovementKillsSelfWhenEnergyDepleted)
{
    Animal * a = animal();
    a->setEnergy(0.05); // Less than BASE_METABOLISM, so it drops to <= 0.
    ASSERT_EQ(mSpecies->animals().size(), 1);

    a->executeMovement();

    EXPECT_EQ(mSpecies->animals().size(), 0);
}

TEST_F(AnimalTest, ExecuteMovementIncrementsAge)
{
    Animal * a = animal();
    a->executeMovement();
    EXPECT_EQ(a->statistics().mAge, 1u);
}

TEST_F(AnimalTest, ExecuteMovementSpawnsWhenEnergyReachesThreshold)
{
    Animal * a = animal();
    ASSERT_EQ(mSpecies->animals().size(), 1);

    // executeMovement() only spawns once the animal is older than
    // ANIMAL_MINIMUM_SPAWN_AGE (and that many cycles past its last spawn),
    // so age it up first. Energy stays well under the species' default
    // spawning energy the whole time, so this can't trigger a spawn early.
    for (int i = 0; i <= ANIMAL_MINIMUM_SPAWN_AGE; ++i)
    {
        a->executeMovement();
    }
    ASSERT_EQ(mSpecies->animals().size(), 1);

    a->setSpawningEnergy(100);
    a->setEnergy(500);

    a->executeMovement();

    // spawnSelf() halves the parent's energy and adds a child pulled from
    // the inactive pool.
    EXPECT_EQ(mSpecies->animals().size(), 2);
    EXPECT_DOUBLE_EQ(a->energy(), (500.0 - Animal::BASE_METABOLISM) / 2.0);
    EXPECT_EQ(a->statistics().mNumChildren, 1u);
}

TEST_F(AnimalTest, ExecuteMovementDoesNotReviveAlreadyEatenAnimal)
{
    Animal * a = animal();
    a->markAsEaten();
    ASSERT_EQ(mSpecies->animals().size(), 1);

    a->executeMovement();

    EXPECT_EQ(mSpecies->animals().size(), 0);
}

TEST_F(AnimalTest, ExecuteMovementOnEatenAnimalRaisesDeathEffect)
{
    DeathEffects::takeAll(); // Drain any residue from earlier tests.

    Animal * a = animal();
    const QPointF pos = a->pos();
    const QColor color = a->color();
    a->markAsEaten();

    a->executeMovement();

    const QVector<DeathEvent> events = DeathEffects::takeAll();
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events.first().pos, pos);
    EXPECT_EQ(events.first().color, color);
}
