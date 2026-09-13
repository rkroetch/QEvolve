#include "difficultycurve.h"

#include <algorithm>

namespace {

// --- In-run rival escalation table ------------------------------------------
// Fixed epoch boundaries at which a new rival archetype is introduced,
// directly following the redesign plan's example escalation ("a
// RAPTORS-style pack, then a MCCOY, etc." - section 2a). Epoch 0 is a grace
// period with no rivals so the player gets a few thousand ticks to establish
// a foothold before anything hostile shows up. See species/APEX.SPC for the
// new capstone boss added alongside this module (species/*.SPC content
// choices are explained in the accompanying report, not here).
struct RivalIntroduction
{
    int epoch;
    const char * archetype;
    int count;
};

constexpr RivalIntroduction kRivalIntroductions[] = {
    {1, "SLUGS",    1}, // early mild rival: slow scavenger, low threat
    {3, "HATFIELD", 1}, // early-mid duelist: aggressive but solitary
    {4, "RAPTORS",  2}, // mid-run pack predator: two independent packs
    {6, "MCCOY",    1}, // mid-late all-rounder rival
    {7, "BRACHIO",  1}, // late heavy hitter: tanky, high spawn-energy
    {8, "APEX",     1}, // capstone boss (new archetype, see species/APEX.SPC)
};

constexpr int kLastScriptedEpoch = 8;
// Beyond the last scripted introduction (i.e. an ascension/endless run
// pushing past the default 10-epoch win condition), keep escalating by
// dropping in another APEX-tier pack every few epochs rather than plateauing.
constexpr int kPostScriptEpochInterval = 3;

double clamp01Plus(double value, double lo, double hi)
{
    return std::clamp(value, lo, hi);
}

// Shared by computeMetaTier() and computeEncounterSpec() so the "how much
// tougher are rivals at ascension tier N" number only lives in one place.
double rivalAggressionFloorForTier(int tier)
{
    // +15% rival metabolism/spawning-energy aggression per cleared tier.
    return 1.0 + 0.15 * double(tier);
}

double startingResourceMultiplierForTier(int tier)
{
    // -8% starting resources per cleared tier, floored at 40% so an
    // ascended run is brutal but never literally unwinnable at turn one.
    return clamp01Plus(1.0 - 0.08 * double(tier), 0.4, 1.0);
}

QVector<RivalSpawn> rivalsForEpoch(int epoch)
{
    QVector<RivalSpawn> rivals;
    for (const RivalIntroduction & intro : kRivalIntroductions)
    {
        if (intro.epoch == epoch)
        {
            rivals.append(RivalSpawn{QString::fromLatin1(intro.archetype), intro.count});
        }
    }

    if (epoch > kLastScriptedEpoch &&
        (epoch - kLastScriptedEpoch) % kPostScriptEpochInterval == 0)
    {
        const int extraPacks = 1 + (epoch - kLastScriptedEpoch) / kPostScriptEpochInterval;
        rivals.append(RivalSpawn{QStringLiteral("APEX"), extraPacks});
    }

    return rivals;
}

double rivalStatMultiplierForEpoch(int epoch, int tier)
{
    // Rivals get 8% tougher per epoch on top of whatever floor the current
    // ascension tier sets.
    return rivalAggressionFloorForTier(tier) * (1.0 + 0.08 * double(epoch));
}

double plantCapMultiplierForEpoch(int epoch, int tier)
{
    // 5% fewer plants per epoch and per ascension tier, floored at 35% of
    // MAX_NUM_PLANTS so late runs are starved but food never fully vanishes.
    return clamp01Plus(1.0 - 0.05 * double(epoch) - 0.05 * double(tier), 0.35, 1.0);
}

double plantSpawnEnergyMultiplierForEpoch(int epoch, int tier)
{
    // Plants need progressively more energy to reproduce: +6%/epoch,
    // +8%/tier, capped at 2.5x so regrowth slows to a crawl but never stops.
    return clamp01Plus(1.0 + 0.06 * double(epoch) + 0.08 * double(tier), 1.0, 2.5);
}

HazardEvent hazardForEpoch(int epoch, int tier)
{
    // Epoch 0-1: no hazards yet, matching the rival-free grace period above.
    if (epoch <= 1)
    {
        return HazardEvent{};
    }

    // Epoch 2 is a single scripted "breather" - a rare positive event to
    // offset the first rival/scarcity spike the player just absorbed,
    // rather than only ever piling on.
    if (epoch == 2)
    {
        return HazardEvent{HazardType::ResourceBloom, 0.25, 0.15, 0};
    }

    const double chance = clamp01Plus(0.10 + 0.05 * double(epoch - 3) + 0.05 * double(tier), 0.10, 0.75);
    const bool metabolismSurge = (epoch % 2) != 0;
    if (metabolismSurge)
    {
        const double magnitude = clamp01Plus(1.1 + 0.05 * double(epoch), 1.1, 2.0);
        return HazardEvent{HazardType::MetabolismSurge, chance, magnitude, 1};
    }

    const double magnitude = clamp01Plus(0.15 + 0.03 * double(epoch), 0.15, 0.5);
    return HazardEvent{HazardType::PlantDieOff, chance, magnitude, 0};
}

} // namespace

EncounterSpec computeEncounterSpec(int epoch, int metaTier)
{
    epoch = std::max(0, epoch);
    metaTier = std::max(0, metaTier);

    EncounterSpec spec;
    spec.epoch = epoch;
    spec.rivals = rivalsForEpoch(epoch);
    spec.rivalStatMultiplier = rivalStatMultiplierForEpoch(epoch, metaTier);
    spec.plantCapMultiplier = plantCapMultiplierForEpoch(epoch, metaTier);
    spec.plantSpawnEnergyMultiplier = plantSpawnEnergyMultiplierForEpoch(epoch, metaTier);
    spec.hazard = hazardForEpoch(epoch, metaTier);
    return spec;
}

MetaTier computeMetaTier(int clearedRunCount)
{
    const int tier = std::max(0, clearedRunCount);

    MetaTier result;
    result.tier = tier;
    result.rivalAggressionFloor = rivalAggressionFloorForTier(tier);
    result.startingResourceMultiplier = startingResourceMultiplierForTier(tier);
    return result;
}
