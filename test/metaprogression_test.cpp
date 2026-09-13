#include <gtest/gtest.h>

#include <memory>

#include "metaprogression.h"

namespace {

// Every test uses this settings group instead of MetaProgression's default
// ("Meta"), so unit tests never read or clobber a real player's saved
// progress in the shared QSettings("ryank", "Evolve") store.
const QString kTestGroup = QStringLiteral("MetaProgressionUnitTest");

} // namespace

class MetaProgressionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mMeta = std::make_unique<MetaProgression>(nullptr, kTestGroup);
        mMeta->resetProgress(); // Start every test from a clean slate.
    }

    void TearDown() override
    {
        mMeta->resetProgress(); // Leave nothing behind for the next run.
        mMeta.reset();
    }

    std::unique_ptr<MetaProgression> mMeta;
};

TEST_F(MetaProgressionTest, StartsAtZeroEPWithFreeDefaultsUnlocked)
{
    EXPECT_EQ(mMeta->evolutionPoints(), 0);
    EXPECT_TRUE(mMeta->isUnlocked("species.RABBITS"));
    EXPECT_TRUE(mMeta->isUnlocked("biome.oneGroup"));
    EXPECT_FALSE(mMeta->isUnlocked("species.RAPTORS"));
    EXPECT_FALSE(mMeta->isUnlocked("biome.random"));
}

TEST_F(MetaProgressionTest, AwardEPIncreasesBalanceAndEmitsSignal)
{
    int lastBalance = -1;
    QObject::connect(mMeta.get(), &MetaProgression::evolutionPointsChanged, [&](int balance) { lastBalance = balance; });

    mMeta->awardEP(50);

    EXPECT_EQ(mMeta->evolutionPoints(), 50);
    EXPECT_EQ(lastBalance, 50);
}

TEST_F(MetaProgressionTest, AwardEPNeverGoesNegative)
{
    mMeta->awardEP(10);
    mMeta->awardEP(-1000);

    EXPECT_EQ(mMeta->evolutionPoints(), 0);
}

TEST_F(MetaProgressionTest, CannotPurchaseWithoutEnoughEP)
{
    ASSERT_FALSE(mMeta->canAfford("species.RAPTORS"));
    EXPECT_FALSE(mMeta->purchaseUnlock("species.RAPTORS"));
    EXPECT_FALSE(mMeta->isUnlocked("species.RAPTORS"));
    EXPECT_EQ(mMeta->evolutionPoints(), 0);
}

TEST_F(MetaProgressionTest, PurchasingDeductsEPAndUnlocks)
{
    mMeta->awardEP(500);
    const int costBefore = mMeta->findDefinition("species.RAPTORS")->cost;

    QString purchasedId;
    QObject::connect(mMeta.get(), &MetaProgression::unlockPurchased, [&](QString id) { purchasedId = id; });

    ASSERT_TRUE(mMeta->purchaseUnlock("species.RAPTORS"));

    EXPECT_TRUE(mMeta->isUnlocked("species.RAPTORS"));
    EXPECT_EQ(mMeta->evolutionPoints(), 500 - costBefore);
    EXPECT_EQ(purchasedId, "species.RAPTORS");
}

TEST_F(MetaProgressionTest, CannotPurchaseTheSameUnlockTwice)
{
    mMeta->awardEP(1000);
    ASSERT_TRUE(mMeta->purchaseUnlock("species.RAPTORS"));
    const int balanceAfterFirst = mMeta->evolutionPoints();

    EXPECT_FALSE(mMeta->purchaseUnlock("species.RAPTORS"));
    EXPECT_EQ(mMeta->evolutionPoints(), balanceAfterFirst);
}

TEST_F(MetaProgressionTest, StatBonusTiersRequirePreviousTier)
{
    mMeta->awardEP(1000);

    EXPECT_FALSE(mMeta->canUnlock("stat.metabolism.2"));
    EXPECT_TRUE(mMeta->canUnlock("stat.metabolism.1"));

    ASSERT_TRUE(mMeta->purchaseUnlock("stat.metabolism.1"));
    EXPECT_TRUE(mMeta->canUnlock("stat.metabolism.2"));

    ASSERT_TRUE(mMeta->purchaseUnlock("stat.metabolism.2"));
    EXPECT_DOUBLE_EQ(mMeta->metabolismBonus(), 0.10);
}

TEST_F(MetaProgressionTest, UnrelatedStatTargetsDoNotAccumulate)
{
    mMeta->awardEP(1000);
    ASSERT_TRUE(mMeta->purchaseUnlock("stat.metabolism.1"));

    EXPECT_DOUBLE_EQ(mMeta->metabolismBonus(), 0.05);
    EXPECT_DOUBLE_EQ(mMeta->spawningEnergyBonus(), 0.0);
    EXPECT_DOUBLE_EQ(mMeta->mutationRateBonus(), 0.0);
}

TEST_F(MetaProgressionTest, ChargesStockUpToMaxAndCanBeConsumed)
{
    mMeta->awardEP(10000);
    const UnlockDefinition * def = mMeta->findDefinition("charge.revive");
    ASSERT_NE(def, nullptr);

    for (int i = 0; i < def->maxCharges; ++i)
    {
        EXPECT_TRUE(mMeta->purchaseUnlock("charge.revive"));
    }
    EXPECT_EQ(mMeta->chargesRemaining("charge.revive"), def->maxCharges);

    // At cap: cannot buy another.
    EXPECT_FALSE(mMeta->canUnlock("charge.revive"));
    EXPECT_FALSE(mMeta->purchaseUnlock("charge.revive"));

    EXPECT_TRUE(mMeta->consumeCharge("charge.revive"));
    EXPECT_EQ(mMeta->chargesRemaining("charge.revive"), def->maxCharges - 1);

    // Back under the cap, another purchase is allowed again.
    EXPECT_TRUE(mMeta->canUnlock("charge.revive"));
}

TEST_F(MetaProgressionTest, ConsumingAChargeWithNoStockFails)
{
    EXPECT_EQ(mMeta->chargesRemaining("charge.reroll"), 0);
    EXPECT_FALSE(mMeta->consumeCharge("charge.reroll"));
}

TEST_F(MetaProgressionTest, UnknownIdIsNeverUnlockableOrAffordable)
{
    mMeta->awardEP(10000);
    EXPECT_FALSE(mMeta->canAfford("no.such.id"));
    EXPECT_FALSE(mMeta->canUnlock("no.such.id"));
    EXPECT_FALSE(mMeta->purchaseUnlock("no.such.id"));
}

TEST_F(MetaProgressionTest, ApplyRunResultAwardsComputedEP)
{
    RunResult result;
    result.outcome = RunOutcome::Won;
    result.epochsCleared = 3;
    result.speciesStats.append(SpeciesRunStats{"Rabbits", 30, 2, 10});

    const int earned = mMeta->applyRunResult(result);

    EXPECT_GT(earned, 0);
    EXPECT_EQ(mMeta->evolutionPoints(), earned);
}

TEST_F(MetaProgressionTest, StateSurvivesReloadFromSettings)
{
    mMeta->awardEP(1000);
    ASSERT_TRUE(mMeta->purchaseUnlock("species.RAPTORS"));
    ASSERT_TRUE(mMeta->purchaseUnlock("charge.revive"));
    const int balance = mMeta->evolutionPoints();

    // Destroy and recreate against the same settings group, simulating an
    // app restart.
    mMeta.reset();
    mMeta = std::make_unique<MetaProgression>(nullptr, kTestGroup);

    EXPECT_EQ(mMeta->evolutionPoints(), balance);
    EXPECT_TRUE(mMeta->isUnlocked("species.RAPTORS"));
    EXPECT_EQ(mMeta->chargesRemaining("charge.revive"), 1);
}

TEST_F(MetaProgressionTest, ResetProgressClearsEverythingButDefaults)
{
    mMeta->awardEP(1000);
    ASSERT_TRUE(mMeta->purchaseUnlock("species.RAPTORS"));
    ASSERT_TRUE(mMeta->purchaseUnlock("charge.revive"));

    mMeta->resetProgress();

    EXPECT_EQ(mMeta->evolutionPoints(), 0);
    EXPECT_FALSE(mMeta->isUnlocked("species.RAPTORS"));
    EXPECT_TRUE(mMeta->isUnlocked("species.RABBITS"));
    EXPECT_EQ(mMeta->chargesRemaining("charge.revive"), 0);
}
