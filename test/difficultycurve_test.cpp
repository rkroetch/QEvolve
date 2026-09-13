#include <gtest/gtest.h>

#include "difficultycurve.h"

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
    EXPECT_EQ(slugs->count, 1);
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
    EXPECT_EQ(raptors->count, 2);
}

TEST(ComputeEncounterSpec, EpochSixIntroducesMcCoyPerTheDesignPlan)
{
    const EncounterSpec spec = computeEncounterSpec(6);
    const RivalSpawn * mccoy = findRival(spec, "MCCOY");
    ASSERT_NE(mccoy, nullptr);
    EXPECT_EQ(mccoy->count, 1);
}

TEST(ComputeEncounterSpec, EpochEightIntroducesTheApexBoss)
{
    const EncounterSpec spec = computeEncounterSpec(8);
    const RivalSpawn * apex = findRival(spec, "APEX");
    ASSERT_NE(apex, nullptr);
    EXPECT_EQ(apex->count, 1);
}

TEST(ComputeEncounterSpec, EpochsBetweenScriptedIntroductionsAddNoNewRivals)
{
    EXPECT_TRUE(computeEncounterSpec(2).rivals.isEmpty());
    EXPECT_TRUE(computeEncounterSpec(5).rivals.isEmpty());
}

TEST(ComputeEncounterSpec, PastScriptedEpochsKeepsEscalatingWithMoreApexPacks)
{
    // For ascension/endless runs that push past the default 10-epoch win
    // condition, the curve shouldn't just plateau at epoch 8's boss.
    const EncounterSpec epoch11 = computeEncounterSpec(11);
    const RivalSpawn * apex11 = findRival(epoch11, "APEX");
    ASSERT_NE(apex11, nullptr);
    EXPECT_EQ(apex11->count, 2);

    const EncounterSpec epoch14 = computeEncounterSpec(14);
    const RivalSpawn * apex14 = findRival(epoch14, "APEX");
    ASSERT_NE(apex14, nullptr);
    EXPECT_EQ(apex14->count, 3);

    // Epochs that aren't a multiple of the post-script interval past epoch 8
    // introduce nothing new.
    EXPECT_TRUE(computeEncounterSpec(9).rivals.isEmpty());
    EXPECT_TRUE(computeEncounterSpec(10).rivals.isEmpty());
}

TEST(ComputeEncounterSpec, NegativeEpochClampsToZero)
{
    EXPECT_EQ(computeEncounterSpec(-5).epoch, computeEncounterSpec(0).epoch);
    EXPECT_TRUE(computeEncounterSpec(-5).rivals.isEmpty());
}

TEST(ComputeEncounterSpec, PlantScarcityTightensWithEpochAndFloorsAt35Percent)
{
    const EncounterSpec epoch0 = computeEncounterSpec(0);
    const EncounterSpec epoch5 = computeEncounterSpec(5);
    EXPECT_DOUBLE_EQ(epoch5.plantCapMultiplier, 0.75); // 1.0 - 0.05*5
    EXPECT_LT(epoch5.plantCapMultiplier, epoch0.plantCapMultiplier);

    // Far beyond the scripted range, the multiplier floors rather than
    // going to zero or negative.
    const EncounterSpec farEpoch = computeEncounterSpec(100);
    EXPECT_DOUBLE_EQ(farEpoch.plantCapMultiplier, 0.35);
}

TEST(ComputeEncounterSpec, PlantSpawnEnergyRisesWithEpochAndCapsAt2Point5x)
{
    const EncounterSpec epoch5 = computeEncounterSpec(5);
    EXPECT_DOUBLE_EQ(epoch5.plantSpawnEnergyMultiplier, 1.3); // 1.0 + 0.06*5

    const EncounterSpec farEpoch = computeEncounterSpec(100);
    EXPECT_DOUBLE_EQ(farEpoch.plantSpawnEnergyMultiplier, 2.5);
}

TEST(ComputeEncounterSpec, HazardIsNoneForTheGracePeriod)
{
    EXPECT_EQ(computeEncounterSpec(0).hazard.type, HazardType::None);
    EXPECT_EQ(computeEncounterSpec(1).hazard.type, HazardType::None);
}

TEST(ComputeEncounterSpec, EpochTwoOffersAResourceBloomBreather)
{
    const HazardEvent hazard = computeEncounterSpec(2).hazard;
    EXPECT_EQ(hazard.type, HazardType::ResourceBloom);
    EXPECT_DOUBLE_EQ(hazard.chance, 0.25);
    EXPECT_GT(hazard.magnitude, 0.0);
}

TEST(ComputeEncounterSpec, OddEpochsRollMetabolismSurgeHazards)
{
    const HazardEvent hazard = computeEncounterSpec(3).hazard;
    EXPECT_EQ(hazard.type, HazardType::MetabolismSurge);
    EXPECT_DOUBLE_EQ(hazard.chance, 0.10);
    EXPECT_DOUBLE_EQ(hazard.magnitude, 1.25); // 1.1 + 0.05*3
    EXPECT_EQ(hazard.durationEpochs, 1);
}

TEST(ComputeEncounterSpec, EvenEpochsPastTwoRollPlantDieOffHazards)
{
    const HazardEvent hazard = computeEncounterSpec(4).hazard;
    EXPECT_EQ(hazard.type, HazardType::PlantDieOff);
    EXPECT_DOUBLE_EQ(hazard.chance, 0.15); // 0.10 + 0.05*(4-3)
    EXPECT_DOUBLE_EQ(hazard.magnitude, 0.27); // 0.15 + 0.03*4
    EXPECT_EQ(hazard.durationEpochs, 0);
}

TEST(ComputeEncounterSpec, HazardChanceIncreasesWithMetaTier)
{
    const HazardEvent tier0 = computeEncounterSpec(4, 0).hazard;
    const HazardEvent tier2 = computeEncounterSpec(4, 2).hazard;
    EXPECT_GT(tier2.chance, tier0.chance);
}

TEST(ComputeEncounterSpec, RivalStatMultiplierScalesWithEpochAndTier)
{
    EXPECT_DOUBLE_EQ(computeEncounterSpec(0, 0).rivalStatMultiplier, 1.0);

    // Epoch-only escalation: 1.0 * (1 + 0.08*5).
    EXPECT_DOUBLE_EQ(computeEncounterSpec(5, 0).rivalStatMultiplier, 1.4);

    // Tier floor (1 + 0.15*2 = 1.3) folded in on top of epoch escalation.
    EXPECT_DOUBLE_EQ(computeEncounterSpec(5, 2).rivalStatMultiplier, 1.3 * 1.4);
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
