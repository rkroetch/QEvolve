#include <gtest/gtest.h>

#include "difficultycurve.h"

// NOTE: these expected values match the Phase 3 balance-pass retuning of
// difficultycurve.cpp (see that file's comments and the balance-pass commit
// message for the simulated data behind each change) - they are
// deliberately different from the original placeholder schedule this test
// file shipped with.

namespace {

// Finds a rival by archetype name in an EncounterSpec, or nullptr if this
// epoch didn't introduce it.
const RivalSpawn * findRival(const EncounterSpec & spec, const QString & archetype)
{
    for (const RivalSpawn & rival : spec.rivals)
    {
        if (rival.archetype == archetype)
        {
            return &rival;
        }
    }
    return nullptr;
}

} // namespace

TEST(ComputeEncounterSpec, EpochZeroIsAGracePeriod)
{
    const EncounterSpec spec = computeEncounterSpec(0);
    EXPECT_EQ(spec.epoch, 0);
    EXPECT_TRUE(spec.rivals.isEmpty());
    EXPECT_DOUBLE_EQ(spec.plantCapMultiplier, 1.0);
    EXPECT_DOUBLE_EQ(spec.plantSpawnEnergyMultiplier, 1.0);
    EXPECT_DOUBLE_EQ(spec.rivalStatMultiplier, 1.0);
    EXPECT_EQ(spec.hazard.type, HazardType::None);
    EXPECT_DOUBLE_EQ(spec.hazard.chance, 0.0);
}

TEST(ComputeEncounterSpec, EpochOneIntroducesTheEarlyMildRival)
{
    const EncounterSpec spec = computeEncounterSpec(1);
    const RivalSpawn * slugs = findRival(spec, "SLUGS");
    ASSERT_NE(slugs, nullptr);
    EXPECT_EQ(slugs->count, 3);
    // No other archetype should show up this early.
    EXPECT_EQ(spec.rivals.size(), 1);
}

TEST(ComputeEncounterSpec, EpochFourIntroducesTheRaptorsPackPerTheDesignPlan)
{
    // Section 2a of the redesign plan calls out "a RAPTORS-style pack" as
    // the canonical mid-run escalation example.
    const EncounterSpec spec = computeEncounterSpec(4);
    const RivalSpawn * raptors = findRival(spec, "RAPTORS");
    ASSERT_NE(raptors, nullptr);
    EXPECT_EQ(raptors->count, 6);
}

TEST(ComputeEncounterSpec, EpochSixIntroducesMcCoyPerTheDesignPlan)
{
    const EncounterSpec spec = computeEncounterSpec(6);
    const RivalSpawn * mccoy = findRival(spec, "MCCOY");
    ASSERT_NE(mccoy, nullptr);
    EXPECT_EQ(mccoy->count, 5);
}

TEST(ComputeEncounterSpec, EpochEightIntroducesTheApexBoss)
{
    const EncounterSpec spec = computeEncounterSpec(8);
    const RivalSpawn * apex = findRival(spec, "APEX");
    ASSERT_NE(apex, nullptr);
    EXPECT_EQ(apex->count, 3);
}

TEST(ComputeEncounterSpec, EpochsBetweenScriptedIntroductionsAddNoNewRivals)
{
    EXPECT_TRUE(computeEncounterSpec(2).rivals.isEmpty());
    EXPECT_TRUE(computeEncounterSpec(5).rivals.isEmpty());
}

TEST(ComputeEncounterSpec, PastScriptedEpochsKeepsEscalatingWithMoreApexPacks)
{
    // For ascension/endless runs that push past the default 10-epoch win
    // condition, the curve shouldn't just plateau at epoch 8's boss. The
    // post-script interval was tightened from every 3 epochs to every 2
    // (see kPostScriptEpochInterval) so endless/ascension runs keep
    // climbing at roughly the pace the scripted 1-8 schedule does.
    EXPECT_TRUE(computeEncounterSpec(9).rivals.isEmpty());

    const EncounterSpec epoch10 = computeEncounterSpec(10);
    const RivalSpawn * apex10 = findRival(epoch10, "APEX");
    ASSERT_NE(apex10, nullptr);
    EXPECT_EQ(apex10->count, 2);

    EXPECT_TRUE(computeEncounterSpec(11).rivals.isEmpty());

    const EncounterSpec epoch12 = computeEncounterSpec(12);
    const RivalSpawn * apex12 = findRival(epoch12, "APEX");
    ASSERT_NE(apex12, nullptr);
    EXPECT_EQ(apex12->count, 3);
}

TEST(ComputeEncounterSpec, NegativeEpochClampsToZero)
{
    EXPECT_EQ(computeEncounterSpec(-5).epoch, computeEncounterSpec(0).epoch);
    EXPECT_TRUE(computeEncounterSpec(-5).rivals.isEmpty());
}

TEST(ComputeEncounterSpec, PlantScarcityTightensWithEpochAndFloorsAt25Percent)
{
    const EncounterSpec epoch0 = computeEncounterSpec(0);
    const EncounterSpec epoch5 = computeEncounterSpec(5);
    EXPECT_DOUBLE_EQ(epoch5.plantCapMultiplier, 0.6); // 1.0 - 0.08*5
    EXPECT_LT(epoch5.plantCapMultiplier, epoch0.plantCapMultiplier);

    // Far beyond the scripted range, the multiplier floors rather than
    // going to zero or negative.
    const EncounterSpec farEpoch = computeEncounterSpec(100);
    EXPECT_DOUBLE_EQ(farEpoch.plantCapMultiplier, 0.25);
}

TEST(ComputeEncounterSpec, PlantSpawnEnergyRisesWithEpochAndCapsAt4x)
{
    const EncounterSpec epoch5 = computeEncounterSpec(5);
    EXPECT_DOUBLE_EQ(epoch5.plantSpawnEnergyMultiplier, 1.7); // 1.0 + 0.14*5

    const EncounterSpec farEpoch = computeEncounterSpec(100);
    EXPECT_DOUBLE_EQ(farEpoch.plantSpawnEnergyMultiplier, 4.0);
}

TEST(ComputeEncounterSpec, HazardIsNoneOnlyForTheEpochZeroGracePeriod)
{
    // Simulated data (see difficultycurve.cpp's hazardForEpoch() comment)
    // found almost all of a fresh run's losses happen very early - around
    // epoch 1-2 - so unlike the original placeholder schedule, epoch 1 now
    // carries real hazard risk instead of waiting until epoch 3.
    EXPECT_EQ(computeEncounterSpec(0).hazard.type, HazardType::None);
    EXPECT_NE(computeEncounterSpec(1).hazard.type, HazardType::None);
}

TEST(ComputeEncounterSpec, EpochOneIsARealEarlyPlantShortage)
{
    const HazardEvent hazard = computeEncounterSpec(1).hazard;
    EXPECT_EQ(hazard.type, HazardType::PlantDieOff);
    EXPECT_DOUBLE_EQ(hazard.chance, 0.8);
    EXPECT_DOUBLE_EQ(hazard.magnitude, 0.85);
    EXPECT_EQ(hazard.durationEpochs, 0);
}

TEST(ComputeEncounterSpec, EpochTwoOffersAResourceBloomBreather)
{
    const HazardEvent hazard = computeEncounterSpec(2).hazard;
    EXPECT_EQ(hazard.type, HazardType::ResourceBloom);
    EXPECT_DOUBLE_EQ(hazard.chance, 0.20);
    EXPECT_GT(hazard.magnitude, 0.0);
}

TEST(ComputeEncounterSpec, OddEpochsFromThreeOnwardRollMetabolismSurgeHazards)
{
    const HazardEvent hazard = computeEncounterSpec(3).hazard;
    EXPECT_EQ(hazard.type, HazardType::MetabolismSurge);
    EXPECT_DOUBLE_EQ(hazard.chance, 0.42);   // 0.30 + 0.06*(3-1)
    EXPECT_DOUBLE_EQ(hazard.magnitude, 1.43); // 1.25 + 0.06*3
    EXPECT_EQ(hazard.durationEpochs, 1);
}

TEST(ComputeEncounterSpec, EvenEpochsFromFourOnwardRollPlantDieOffHazards)
{
    const HazardEvent hazard = computeEncounterSpec(4).hazard;
    EXPECT_EQ(hazard.type, HazardType::PlantDieOff);
    EXPECT_DOUBLE_EQ(hazard.chance, 0.48);   // 0.30 + 0.06*(4-1)
    EXPECT_DOUBLE_EQ(hazard.magnitude, 0.41); // 0.25 + 0.04*4
    EXPECT_EQ(hazard.durationEpochs, 0);
}

TEST(ComputeEncounterSpec, HazardChanceIncreasesWithMetaTier)
{
    const HazardEvent tier0 = computeEncounterSpec(4, 0).hazard;
    const HazardEvent tier2 = computeEncounterSpec(4, 2).hazard;
    EXPECT_GT(tier2.chance, tier0.chance);
}

TEST(ComputeEncounterSpec, RivalStatMultiplierScalesGentlyWithEpochAndTier)
{
    // Retuned from an original 8%/epoch to 3%/epoch: simulated data found
    // that a faster-growing rivalStatMultiplier made rivals WEAKER, not
    // stronger (it scales both metabolism and spawningEnergy together, but
    // a spawned rival's *starting* energy is fixed - see
    // rivalStatMultiplierForEpoch()'s comment in difficultycurve.cpp) - so
    // this is deliberately a gentle ramp now.
    EXPECT_DOUBLE_EQ(computeEncounterSpec(0, 0).rivalStatMultiplier, 1.0);

    // Epoch-only escalation: 1.0 * (1 + 0.03*5).
    EXPECT_DOUBLE_EQ(computeEncounterSpec(5, 0).rivalStatMultiplier, 1.15);

    // Tier floor (1 + 0.15*2 = 1.3) folded in on top of epoch escalation.
    EXPECT_DOUBLE_EQ(computeEncounterSpec(5, 2).rivalStatMultiplier, 1.3 * 1.15);
}

TEST(ComputeEncounterSpec, NegativeMetaTierClampsToZero)
{
    EXPECT_DOUBLE_EQ(computeEncounterSpec(5, -3).rivalStatMultiplier,
                      computeEncounterSpec(5, 0).rivalStatMultiplier);
}

TEST(ComputeMetaTier, TierZeroIsNeutral)
{
    const MetaTier tier = computeMetaTier(0);
    EXPECT_EQ(tier.tier, 0);
    EXPECT_DOUBLE_EQ(tier.rivalAggressionFloor, 1.0);
    EXPECT_DOUBLE_EQ(tier.startingResourceMultiplier, 1.0);
}

TEST(ComputeMetaTier, EachClearedTierRaisesAggressionAndLowersResources)
{
    const MetaTier tier3 = computeMetaTier(3);
    EXPECT_EQ(tier3.tier, 3);
    EXPECT_DOUBLE_EQ(tier3.rivalAggressionFloor, 1.45);       // 1.0 + 0.15*3
    EXPECT_DOUBLE_EQ(tier3.startingResourceMultiplier, 0.76); // 1.0 - 0.08*3
}

TEST(ComputeMetaTier, StartingResourceMultiplierFloorsAt40Percent)
{
    const MetaTier highTier = computeMetaTier(20);
    EXPECT_DOUBLE_EQ(highTier.startingResourceMultiplier, 0.4);
}

TEST(ComputeMetaTier, NegativeClearedRunCountClampsToTierZero)
{
    const MetaTier tier = computeMetaTier(-4);
    EXPECT_EQ(tier.tier, 0);
    EXPECT_DOUBLE_EQ(tier.rivalAggressionFloor, 1.0);
    EXPECT_DOUBLE_EQ(tier.startingResourceMultiplier, 1.0);
}

TEST(ComputeMetaTier, AggressionFloorMatchesEncounterSpecAtEpochZero)
{
    // computeEncounterSpec()'s rivalStatMultiplier at epoch 0 should equal
    // the tier's own aggression floor exactly (no in-run escalation yet).
    for (int tier = 0; tier <= 5; ++tier)
    {
        EXPECT_DOUBLE_EQ(computeEncounterSpec(0, tier).rivalStatMultiplier,
                          computeMetaTier(tier).rivalAggressionFloor);
    }
}
