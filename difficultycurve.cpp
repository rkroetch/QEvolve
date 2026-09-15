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

// Counts were retuned from Phase 3's simulated data (see the balance-pass
// commit message/report): Animal::calculateMovement() only lets a species
// prey on another when it has >=3 same-species friends AND >=1 enemy in its
// 3x3 neighborhood (the "friends >= 3" pack-hunting gate baked into the
// pre-existing simulation core - see animal.cpp), and every rival species
// also gets a free +10-animal scattering the moment it's first activated
// (Species::activate(), invoked by Laboratory::applyEncounterSpec()/this
// module's mirror in benchmarks/balance_simulator.cpp). At the original
// counts (1-2 per introduction) a rival pack almost never reached that
// friends>=3 threshold before the run ended: simulated win rates for the
// free starting kit (RABBITS) and the next-easiest preset (CHICKEN) came
// back at 60-93% at these original counts - trivial, not "hard but fair".
// Raising counts here (so activate()'s +10 plus this many more gives packs
// a real chance of clustering into effective hunting groups) was the
// single biggest lever for closing that gap.
constexpr RivalIntroduction kRivalIntroductions[] = {
    {1, "SLUGS",    3}, // early mild rival: slow scavenger, low threat
    {3, "HATFIELD", 3}, // early-mid duelist: aggressive but solitary
    {4, "RAPTORS",  6}, // mid-run pack predator: two independent packs
    {6, "MCCOY",    5}, // mid-late all-rounder rival
    {7, "BRACHIO",  4}, // late heavy hitter: tanky, high spawn-energy
    {8, "APEX",     3}, // capstone boss (new archetype, see species/APEX.SPC)
};

constexpr int kLastScriptedEpoch = 8;
// Beyond the last scripted introduction (i.e. an ascension/endless run
// pushing past the default 10-epoch win condition), keep escalating by
// dropping in another APEX-tier pack every couple of epochs rather than
// plateauing (tightened from every 3 epochs so endless/ascension runs keep
// climbing at roughly the same pace the scripted 1-8 schedule does).
constexpr int kPostScriptEpochInterval = 2;

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
    // Counter-intuitive simulated result (see the balance-pass commit
    // message): raising this rate makes rivals WEAKER, not stronger.
    // rivalStatMultiplier scales a rival's metabolism and spawningEnergy by
    // the same factor (Laboratory::applyEncounterSpec()), but a newly
    // spawned rival's *starting* energy is always the fixed
    // ANIMAL_INITIAL_ENERGY constant, not scaled. Higher metabolism makes
    // each move cover more distance (Animal::executeMovement() spends
    // energy proportional to that distance), so a scaled-up rival burns
    // through its fixed starting energy pool faster while also needing
    // more accumulated energy to reach its now-higher spawningEnergy
    // threshold - it dies out before building a self-sustaining pack.
    // Simulated win rates for the free/default kit went UP (60% -> 92%)
    // when this was raised from 8%->15%/epoch; dropping it to 3% (below
    // the original 8%) was part of what got RABBITS/CHICKEN back into a
    // "hard but fair" range - see rivalsForEpoch()'s counts above for the
    // lever that actually worked (bigger packs, not tougher individuals).
    return rivalAggressionFloorForTier(tier) * (1.0 + 0.03 * double(epoch));
}

double plantCapMultiplierForEpoch(int epoch, int tier)
{
    // 8% fewer plants per epoch and per ascension tier, floored at 25% of
    // MAX_NUM_PLANTS so late runs are starved but food never fully
    // vanishes. NOTE: as of this balance pass, Laboratory::applyEncounterSpec()
    // never actually reads plantCapMultiplier (only plantSpawnEnergyMultiplier
    // below is wired up) - this is a real integration gap from Phase 1/0,
    // flagged in the balance-pass report rather than fixed here since it's
    // an architecture change, not a numeric one. The formula is still tuned
    // to a sensible curve in case that wiring lands later.
    return clamp01Plus(1.0 - 0.08 * double(epoch) - 0.08 * double(tier), 0.25, 1.0);
}

double plantSpawnEnergyMultiplierForEpoch(int epoch, int tier)
{
    // Plants need progressively more energy to reproduce: +14%/epoch (up
    // from 6%), +8%/tier, capped at 4.0x (up from 2.5x) so regrowth slows
    // to a near-crawl late-run. This was the second biggest lever after
    // rival counts: a grazer/omnivore starting kit (RABBITS/CHICKEN) is
    // otherwise very lightly threatened by predation at all (see the
    // rival-count comment above) since it never needs to enter combat to
    // grow - throttling plant regrowth directly is the reliable way to
    // apply late-run pressure to that playstyle.
    return clamp01Plus(1.0 + 0.14 * double(epoch) + 0.08 * double(tier), 1.0, 4.0);
}

HazardEvent hazardForEpoch(int epoch, int tier)
{
    // Epoch 0: no hazards yet, matching the rival-free grace period above.
    //
    // Simulated data (see the balance-pass commit message for the full
    // numbers) found something the original placeholder curve missed
    // entirely: almost all of a fresh RABBITS run's losses happen very
    // early, around epoch 1-2, from the small starting cohort failing to
    // find the still-sparse/regrowing plant cluster in time - not from
    // anything introduced later. Once a run gets past that opening window
    // it reliably goes on to win regardless of how hard epochs 3+ are
    // pushed: raising rival pack sizes 3-6x, and independently pushing
    // hazard chance/magnitude to their practical maximum from epoch 3
    // onward, each only cost the free starting kit a few points of win
    // rate. Two structural reasons why, both flagged in the report as
    // follow-ups rather than fixed here (they're Laboratory/common.h
    // architecture, out of this pass's numeric-only scope):
    //   1. Rival packs are spawned as independently-scattered individuals
    //      (Laboratory::applyEncounterSpec()), and Animal::calculateMovement()
    //      only lets a species prey on another once it has >=3 same-species
    //      neighbors AND >=1 enemy in its own 3x3 cell neighborhood - actual
    //      clustering only builds up slowly through reproduction, so a
    //      rival introduced mid-run rarely reaches effective pack size
    //      before the run ends.
    //   2. plantCapMultiplier (below) is computed here but never actually
    //      consumed by Laboratory::applyEncounterSpec() - only
    //      plantSpawnEnergyMultiplier is wired up - and plants also get an
    //      unconditional +1/tick baseline regrowth every cycle
    //      (Species::respawn(1, 500), called for every species every tick)
    //      that isn't gated by any scarcity multiplier at all. So scarcity
    //      tuning has much less bite than the EncounterSpec fields suggest.
    // Given that, hazards now start at epoch 1 (not epoch 3) specifically
    // to land real pressure inside the window that actually decides a run,
    // rather than only after it's already effectively over.
    if (epoch <= 0)
    {
        return HazardEvent{};
    }

    // Epoch 1: a real, non-deterministic early plant shortage - the single
    // most effective lever found in testing for threatening the free
    // starting kit (RABBITS' win rate dropped from ~80-92% to ~65-75%
    // across repeated simulated batches with this in place, versus
    // reintroducing the same magnitude as an ordinary epoch-3+ escalation,
    // which barely moved it). Rolled as a real chance (not guaranteed) so
    // an unlucky opening is a real risk rather than a scripted death.
    if (epoch == 1)
    {
        return HazardEvent{HazardType::PlantDieOff, 0.8, 0.85, 0};
    }

    // Epoch 2 is a scripted "breather" - a rare positive event to offset
    // epoch 1's shortage for runs that weathered it, rather than only ever
    // piling on. Chance/magnitude trimmed from the original placeholder
    // now that epoch 1 already applies real pressure of its own.
    if (epoch == 2)
    {
        return HazardEvent{HazardType::ResourceBloom, 0.20, 0.15, 0};
    }

    const double chance = clamp01Plus(0.30 + 0.06 * double(epoch - 1) + 0.05 * double(tier), 0.30, 0.85);
    const bool metabolismSurge = (epoch % 2) != 0;
    if (metabolismSurge)
    {
        const double magnitude = clamp01Plus(1.25 + 0.06 * double(epoch), 1.25, 2.2);
        return HazardEvent{HazardType::MetabolismSurge, chance, magnitude, 1};
    }

    const double magnitude = clamp01Plus(0.25 + 0.04 * double(epoch), 0.25, 0.6);
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
