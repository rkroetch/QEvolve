#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>
#include <memory>

#include "animal.h"
#include "species.h"

namespace {

// Places a fully-controlled Animal into `species` at `pos`, bypassing the
// random extra animal Species::initialize() creates, so combat/neighborhood
// tests get deterministic cell placement. Ownership passes to `species`
// (deleted by Species::clear()/killAnimal(), same as any other occupant).
Animal * addAnimalAt(Species & species, QPointF pos, double energy)
{
    auto * animal = new Animal();
    animal->initialize(pos, &species, energy, 1000, 100, Movements(), QPointF(0, 0), nullptr);
    species.addAnimal(animal->cellX(), animal->cellY(), animal);
    return animal;
}

class SpeciesBasicTest : public ::testing::Test
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

class CombatTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mPlant = std::make_unique<Species>(Species::typePlant);
        mSpeciesA = std::make_unique<Species>(Species::typeAnimal);
        mSpeciesB = std::make_unique<Species>(Species::typeAnimal);
    }

    void TearDown() override
    {
        // Destroy in an order that leaves mPlantSpecies/mEnemySpecies caches
        // consistent for any later test's construction.
        mSpeciesB.reset();
        mSpeciesA.reset();
        mPlant.reset();
    }

    std::unique_ptr<Species> mPlant;
    std::unique_ptr<Species> mSpeciesA;
    std::unique_ptr<Species> mSpeciesB;
};

} // namespace

TEST_F(SpeciesBasicTest, TypeIsPreserved)
{
    EXPECT_EQ(mSpecies->type(), Species::typeAnimal);
}

TEST_F(SpeciesBasicTest, NameSetterEmitsSignal)
{
    int emitCount = 0;
    QString emittedName;
    QObject::connect(mSpecies.get(), &Species::nameChanged, [&](const QString & name) {
        ++emitCount;
        emittedName = name;
    });

    mSpecies->setName("Raptors");

    EXPECT_EQ(mSpecies->name(), "Raptors");
    EXPECT_EQ(emitCount, 1);
    EXPECT_EQ(emittedName, "Raptors");
}

TEST_F(SpeciesBasicTest, ColorSetterEmitsSignal)
{
    int emitCount = 0;
    QObject::connect(mSpecies.get(), &Species::colorChanged, [&](QColor) { ++emitCount; });

    mSpecies->setColor(QColor(10, 20, 30));

    EXPECT_EQ(mSpecies->color(), QColor(10, 20, 30));
    EXPECT_EQ(emitCount, 1);
}

TEST_F(SpeciesBasicTest, MetabolismGetterSetter)
{
    mSpecies->setMetabolism(77);
    EXPECT_EQ(mSpecies->metabolism(), 77);
    mSpecies->setMetabolismMutation(4);
    EXPECT_EQ(mSpecies->metabolismMutation(), 4);
}

TEST_F(SpeciesBasicTest, SpawningEnergyGetterSetter)
{
    mSpecies->setSpawningEnergy(1200);
    EXPECT_EQ(mSpecies->spawningEnergy(), 1200);
    mSpecies->setSpawningEnergyMutation(9);
    EXPECT_EQ(mSpecies->spawningEnergyMutation(), 9);
}

TEST_F(SpeciesBasicTest, MovementGetterSetterAndToggle)
{
    mSpecies->setMovement(1, 1, MoveDown);
    EXPECT_EQ(mSpecies->movements().getMovement(1, 1), MoveDown);
    mSpecies->setMovementMutation(6);
    EXPECT_EQ(mSpecies->movementMutation(), 6);

    mSpecies->setMovement(0, 0, MoveUp);
    mSpecies->toggleMovement(0, 0);
    EXPECT_EQ(mSpecies->movements().getMovement(0, 0), MoveUpRight);
}

TEST_F(SpeciesBasicTest, InitializeCreatesOneActiveAnimalAndReservesInactive)
{
    mSpecies->initialize(0, 5, 900);
    EXPECT_EQ(mSpecies->animals().size(), 1);
    EXPECT_DOUBLE_EQ(mSpecies->animals().first()->energy(), 900.0);
}

TEST_F(SpeciesBasicTest, CanSpawnReflectsMaximumAnimals)
{
    mSpecies->initialize(0, 2, 900);
    EXPECT_TRUE(mSpecies->canSpawn());
    mSpecies->spawnAnimal(QPointF(100, 100), 900, 1000, 100, Movements(), QPointF(0, 0), nullptr);
    EXPECT_EQ(mSpecies->animals().size(), 2);
    EXPECT_FALSE(mSpecies->canSpawn());
}

TEST_F(SpeciesBasicTest, SpawnAnimalDrainsInactivePoolThenNoOps)
{
    mSpecies->initialize(0, 2, 900); // 1 active + 2 inactive.
    mSpecies->spawnAnimal(QPointF(50, 50), 900, 1000, 100, Movements(), QPointF(0, 0), nullptr);
    mSpecies->spawnAnimal(QPointF(50, 50), 900, 1000, 100, Movements(), QPointF(0, 0), nullptr);
    EXPECT_EQ(mSpecies->animals().size(), 3);

    // Pool is exhausted now; a further spawn is a silent no-op.
    mSpecies->spawnAnimal(QPointF(50, 50), 900, 1000, 100, Movements(), QPointF(0, 0), nullptr);
    EXPECT_EQ(mSpecies->animals().size(), 3);
}

TEST_F(SpeciesBasicTest, KillAnimalRemovesFromActiveList)
{
    mSpecies->initialize(0, 5, 900);
    Animal * animal = mSpecies->animals().first();
    mSpecies->killAnimal(animal->cellX(), animal->cellY(), animal);
    EXPECT_EQ(mSpecies->animals().size(), 0);
}

TEST_F(SpeciesBasicTest, MoveAnimalUpdatesOccupancyCounts)
{
    // Cell 0 is a wrap seam for forEachNeighborCell() (wrapLabWidth/Height
    // treat coordinate 0 as needing to wrap to WIDTH/HEIGHT - 1), so use two
    // ordinary interior cells far enough apart that they share no neighbors.
    Animal * animal = addAnimalAt(*mSpecies, QPointF(50, 50), 900);
    ASSERT_EQ(mSpecies->friendCount(50, 50), 1);
    ASSERT_EQ(mSpecies->friendCount(300, 150), 0);

    mSpecies->moveAnimal(animal->cellX(), animal->cellY(), 300, 150, animal);
    animal->setPos(QPointF(300, 150));

    EXPECT_EQ(mSpecies->friendCount(50, 50), 0);
    EXPECT_EQ(mSpecies->friendCount(300, 150), 1);
}

TEST_F(SpeciesBasicTest, FirstNeighborExcludesSelfWhenAlone)
{
    // Regression test: firstNeighbor() used to return cell->first() without
    // filtering out the querying animal, so a lone animal always "found"
    // itself as its own neighbor.
    Animal * self = addAnimalAt(*mSpecies, QPointF(50, 50), 900);
    EXPECT_EQ(mSpecies->firstNeighbor(self->cellX(), self->cellY(), self), nullptr);
}

TEST_F(SpeciesBasicTest, FirstNeighborFindsOtherOccupantInSameCellButExcludesSelf)
{
    Animal * self = addAnimalAt(*mSpecies, QPointF(50, 50), 900);
    Animal * other = addAnimalAt(*mSpecies, QPointF(50, 50), 900);

    EXPECT_EQ(mSpecies->firstNeighbor(self->cellX(), self->cellY(), self), other);
    EXPECT_EQ(mSpecies->firstNeighbor(self->cellX(), self->cellY(), other), self);
}

TEST_F(SpeciesBasicTest, FirstNeighborFindsOccupantInAdjacentCell)
{
    Animal * self = addAnimalAt(*mSpecies, QPointF(50, 50), 900);
    Animal * other = addAnimalAt(*mSpecies, QPointF(51, 50), 900);

    EXPECT_EQ(mSpecies->firstNeighbor(self->cellX(), self->cellY(), self), other);
}

TEST_F(SpeciesBasicTest, FirstNeighborWithoutExclusionReturnsNullOnEmptyNeighborhood)
{
    EXPECT_EQ(mSpecies->firstNeighbor(50, 50), nullptr);
}

TEST_F(SpeciesBasicTest, EatWeakestPlantReturnsZeroWhenNoPlantSpeciesExists)
{
    Animal * animal = addAnimalAt(*mSpecies, QPointF(50, 50), 900);
    EXPECT_DOUBLE_EQ(mSpecies->eatWeakestPlant(animal->cellX(), animal->cellY()), 0.0);
}

TEST_F(CombatTest, PlantSpeciesReturnsTheRegisteredPlant)
{
    EXPECT_EQ(Species::plantSpecies(), mPlant.get());
}

TEST_F(CombatTest, FriendEnemyAndPlantCountsAreScopedPerSpecies)
{
    addAnimalAt(*mSpeciesA, QPointF(10, 10), 900);
    addAnimalAt(*mSpeciesA, QPointF(10, 10), 900);
    addAnimalAt(*mSpeciesB, QPointF(10, 10), 900);
    addAnimalAt(*mPlant, QPointF(10, 10), 500);
    addAnimalAt(*mPlant, QPointF(10, 10), 500);

    EXPECT_EQ(mSpeciesA->friendCount(10, 10), 2);
    EXPECT_EQ(mSpeciesA->enemyCount(10, 10), 1);
    EXPECT_EQ(mSpeciesA->plantCount(10, 10), 2);

    EXPECT_EQ(mSpeciesB->friendCount(10, 10), 1);
    EXPECT_EQ(mSpeciesB->enemyCount(10, 10), 2);
}

TEST_F(CombatTest, EatWeakestPlantClaimsOnlyOncePerCombatCycle)
{
    Animal * weakPlant = addAnimalAt(*mPlant, QPointF(10, 10), 50);
    addAnimalAt(*mPlant, QPointF(10, 10), 200);
    Animal * predator = addAnimalAt(*mSpeciesA, QPointF(10, 10), 900);

    // tryClaimCombatCell() compares against the plant species' own combat
    // cycle counter, which starts equal to the (also zero-initialized)
    // per-cell claim array - so the very first claim needs a bump to make
    // the two differ.
    mPlant->advanceCombatCycle();

    const double firstEaten = mSpeciesA->eatWeakestPlant(predator->cellX(), predator->cellY());
    EXPECT_DOUBLE_EQ(firstEaten, 50.0);
    EXPECT_TRUE(weakPlant->isEaten());

    // Same combat cycle: the cell is already claimed, even though a
    // not-yet-eaten (stronger) plant is still present.
    EXPECT_DOUBLE_EQ(mSpeciesA->eatWeakestPlant(predator->cellX(), predator->cellY()), 0.0);

    mPlant->advanceCombatCycle();
    EXPECT_DOUBLE_EQ(mSpeciesA->eatWeakestPlant(predator->cellX(), predator->cellY()), 200.0);
}

TEST_F(CombatTest, KillWeakestEnemyPicksLowestEnergyAcrossEnemySpecies)
{
    Animal * weakEnemy = addAnimalAt(*mSpeciesB, QPointF(20, 20), 30);
    addAnimalAt(*mSpeciesB, QPointF(20, 20), 400);
    Animal * predator = addAnimalAt(*mSpeciesA, QPointF(20, 20), 900);

    // As above: killWeakestEnemy() claims against the *victim's* species
    // combat cycle (mSpeciesB here), which needs a bump before its first
    // claim can succeed.
    mSpeciesB->advanceCombatCycle();

    const double killed = mSpeciesA->killWeakestEnemy(predator->cellX(), predator->cellY());
    EXPECT_DOUBLE_EQ(killed, 30.0);
    EXPECT_TRUE(weakEnemy->isEaten());
}

TEST(SpeciesSaveLoad, RoundTripsUserData)
{
    // Species embeds several LABORATORY_HEIGHT x LABORATORY_WIDTH arrays
    // directly (mOccupants/mFriendCounts/mCellCombatCycle), making a single
    // instance several MB - too large for the default thread stack, so it
    // must always be heap-allocated (as every fixture above does via
    // unique_ptr) rather than declared as a local variable.
    auto original = std::make_unique<Species>(Species::typeAnimal);
    original->setMetabolism(65);
    original->setSpawningEnergy(1230); // Multiple of 10: save()/load() apply a /10, *10 round trip.
    original->setMovementMutation(3);
    original->setMetabolismMutation(2);
    original->setSpawningEnergyMutation(1);
    original->setMovement(0, 0, MoveUp);
    original->setMovement(1, 1, MoveMerge);
    original->setMovement(2, 2, MoveSplit);

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("TESTSPEC.SPC");
    original->save(path);

    auto loaded = std::make_unique<Species>(Species::typeAnimal);
    ASSERT_TRUE(Species::load(path, *loaded));

    EXPECT_EQ(loaded->metabolism(), 65);
    EXPECT_EQ(loaded->spawningEnergy(), 1230);
    EXPECT_EQ(loaded->movementMutation(), 3);
    EXPECT_EQ(loaded->metabolismMutation(), 2);
    EXPECT_EQ(loaded->spawningEnergyMutation(), 1);
    EXPECT_EQ(loaded->movements().getMovement(0, 0), MoveUp);
    EXPECT_EQ(loaded->movements().getMovement(1, 1), MoveMerge);
    EXPECT_EQ(loaded->movements().getMovement(2, 2), MoveSplit);
    EXPECT_EQ(loaded->name(), "TESTSPEC");
}

TEST(SpeciesLoad, RejectsUnknownFileVersion)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("BAD.SPC");
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("ver 2.0\r\n0 0 0 0 0 0 0 0 0\r\n50\r\n100\r\n0\r\n0\r\n0\r\n0\r\n");
    file.close();

    auto species = std::make_unique<Species>(Species::typeAnimal);
    EXPECT_FALSE(Species::load(path, *species));
}
