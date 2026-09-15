#include "metaprogression.h"

#include <QSettings>
#include <algorithm>

namespace {

// Same org/app name Laboratory/MainWindow use for QSettings, so all
// persisted state (plant pattern, meta-progression, ...) lands in one
// settings store. See laboratory.cpp's QSettings("ryank", "Evolve", ...).
constexpr auto kSettingsOrg = "ryank";
constexpr auto kSettingsApp = "Evolve";

// The bestiary shipped in species/*.SPC (see Laboratory::loadSpecies()).
// Kept as a plain list here (rather than scanning the species/ directory)
// so this module has zero dependency on Species/QDir/file IO and stays
// trivially unit-testable.
const QStringList & bestiarySpeciesNames()
{
    static const QStringList names = {
        "RAPTORS", "MCCOY", "HATFIELD", "CRUISER", "RABBITS",
        "CHICKEN", "SLUGS", "PROTPLSM", "WILDPLSM", "SALT", "BRACHIO"
    };
    return names;
}

} // namespace

MetaProgression::MetaProgression(QObject * parent, const QString & settingsGroup) :
    QObject(parent),
    mSettingsGroup(settingsGroup)
{
    registerDefaultCatalog();
    load();
}

void MetaProgression::registerDefaultCatalog()
{
    // Phase 3 balance pass: catalog costs below were checked (not just
    // guessed) against benchmarks/balance_simulator.cpp's simulated EP
    // income for the free/default starting kit (~450-580 EP/run with the
    // retuned difficulty curve - see difficultycurve.cpp and the
    // balance-pass commit message) and left unchanged: the cheapest
    // entries (50-150 EP) are affordable in under a run, and unlocking
    // every entry in this catalog totals ~5700 EP (~11-12 runs) - already
    // a reasonable "cheap stuff fast, everything eventually" curve without
    // needing adjustment.
    mCatalog.clear();

    // --- Starting species presets -----------------------------------
    // RABBITS is the free/default starting species; every other bestiary
    // archetype must be unlocked with EP.
    for (const QString & name : bestiarySpeciesNames())
    {
        UnlockDefinition def;
        def.id = "species." + name;
        def.displayName = name;
        def.description = QStringLiteral("Unlocks %1 as a starting species preset.").arg(name);
        def.category = UnlockCategory::StartingSpecies;
        def.cost = (name == "RABBITS") ? 0 : 150;
        mCatalog.append(def);
    }

    // --- Permanent stat bonuses ---------------------------------------
    // Three tiers each for metabolism efficiency, spawning energy, and
    // mutation rate, each tier requiring the previous one. statBonusValue
    // is a fractional modifier (e.g. 0.05 == +5%); how it's applied to an
    // actual Species is left to the run-setup code that consumes
    // metabolismBonus()/spawningEnergyBonus()/mutationRateBonus().
    struct StatTierSpec { QString target; QString label; int cost; double value; };
    const StatTierSpec statTiers[] = {
        {"metabolism", "Metabolism", 100, 0.05},
        {"metabolism", "Metabolism", 250, 0.05},
        {"metabolism", "Metabolism", 500, 0.05},
        {"spawningEnergy", "Spawning Energy", 100, 0.05},
        {"spawningEnergy", "Spawning Energy", 250, 0.05},
        {"spawningEnergy", "Spawning Energy", 500, 0.05},
        {"mutationRate", "Mutation Rate", 100, 0.05},
        {"mutationRate", "Mutation Rate", 250, 0.05},
        {"mutationRate", "Mutation Rate", 500, 0.05},
    };
    QHash<QString, int> tierIndex;
    for (const auto & tier : statTiers)
    {
        const int index = ++tierIndex[tier.target];
        UnlockDefinition def;
        def.id = QStringLiteral("stat.%1.%2").arg(tier.target).arg(index);
        def.displayName = QStringLiteral("%1 Tier %2").arg(tier.label).arg(index);
        def.description = QStringLiteral("Permanent +%1% %2 bonus.").arg(tier.value * 100.0).arg(tier.label);
        def.category = UnlockCategory::StatBonus;
        def.cost = tier.cost;
        def.statTarget = tier.target;
        def.statBonusValue = tier.value;
        if (index > 1)
        {
            def.prerequisites << QStringLiteral("stat.%1.%2").arg(tier.target).arg(index - 1);
        }
        mCatalog.append(def);
    }

    // --- Biome / plant pattern options ---------------------------------
    // plantPatternOneGroup mirrors Laboratory's default and is free;
    // the others are purchasable variety/difficulty options.
    {
        UnlockDefinition oneGroup;
        oneGroup.id = "biome.oneGroup";
        oneGroup.displayName = "Single Cluster";
        oneGroup.description = "Plants grow in a single dense cluster.";
        oneGroup.category = UnlockCategory::Biome;
        oneGroup.cost = 0;
        mCatalog.append(oneGroup);

        UnlockDefinition twoGroups;
        twoGroups.id = "biome.twoGroups";
        twoGroups.displayName = "Twin Clusters";
        twoGroups.description = "Plants grow in two separated clusters.";
        twoGroups.category = UnlockCategory::Biome;
        twoGroups.cost = 75;
        mCatalog.append(twoGroups);

        UnlockDefinition random;
        random.id = "biome.random";
        random.displayName = "Scattered Growth";
        random.description = "Plants grow scattered randomly across the map.";
        random.category = UnlockCategory::Biome;
        random.cost = 150;
        mCatalog.append(random);
    }

    // --- Cosmetic colors -------------------------------------------------
    struct ColorSpec { QString id; QString name; int cost; };
    const ColorSpec colors[] = {
        {"cosmetic.color.crimson", "Crimson", 50},
        {"cosmetic.color.azure", "Azure", 50},
        {"cosmetic.color.gold", "Gold", 75},
        {"cosmetic.color.violet", "Violet", 75},
        {"cosmetic.color.obsidian", "Obsidian", 100},
    };
    for (const auto & color : colors)
    {
        UnlockDefinition def;
        def.id = color.id;
        def.displayName = color.name;
        def.description = QStringLiteral("Unlocks the %1 cosmetic species color.").arg(color.name);
        def.category = UnlockCategory::Cosmetic;
        def.cost = color.cost;
        mCatalog.append(def);
    }

    // --- Consumable charges ---------------------------------------------
    {
        UnlockDefinition revive;
        revive.id = "charge.revive";
        revive.displayName = "Revive Charge";
        revive.description = "Spend a charge to survive a run-ending loss once.";
        revive.category = UnlockCategory::Charge;
        revive.cost = 200;
        revive.maxCharges = 3;
        mCatalog.append(revive);

        UnlockDefinition reroll;
        reroll.id = "charge.reroll";
        reroll.displayName = "Reroll Charge";
        reroll.description = "Spend a charge to reroll an epoch's encounter/mutation options.";
        reroll.category = UnlockCategory::Charge;
        reroll.cost = 100;
        reroll.maxCharges = 5;
        mCatalog.append(reroll);
    }
}

const QVector<UnlockDefinition> & MetaProgression::catalog() const
{
    return mCatalog;
}

const UnlockDefinition * MetaProgression::findDefinition(const QString & id) const
{
    auto it = std::find_if(mCatalog.begin(), mCatalog.end(),
        [&id](const UnlockDefinition & def) { return def.id == id; });
    return it == mCatalog.end() ? nullptr : &(*it);
}

int MetaProgression::evolutionPoints() const
{
    return mEvolutionPoints;
}

void MetaProgression::awardEP(int amount)
{
    if (amount == 0)
    {
        return;
    }
    mEvolutionPoints = std::max(0, mEvolutionPoints + amount);
    save();
    emit evolutionPointsChanged(mEvolutionPoints);
}

int MetaProgression::applyRunResult(const RunResult & result, const EvolutionPointsWeights & weights)
{
    const int earned = evolutionPointsEarned(result, weights);
    if (earned != 0)
    {
        awardEP(earned);
    }
    return earned;
}

bool MetaProgression::isUnlocked(const QString & id) const
{
    if (mUnlocked.contains(id))
    {
        return true;
    }

    // Zero-cost, prerequisite-free entries (e.g. the default starting
    // species and biome) are available out of the box rather than
    // requiring a no-op purchase.
    const UnlockDefinition * def = findDefinition(id);
    return def != nullptr
        && def->category != UnlockCategory::Charge
        && def->cost == 0
        && def->prerequisites.isEmpty();
}

bool MetaProgression::canAfford(const QString & id) const
{
    const UnlockDefinition * def = findDefinition(id);
    return def != nullptr && mEvolutionPoints >= def->cost;
}

bool MetaProgression::canUnlock(const QString & id) const
{
    const UnlockDefinition * def = findDefinition(id);
    if (def == nullptr)
    {
        return false;
    }

    if (def->category == UnlockCategory::Charge)
    {
        if (chargesRemaining(id) >= def->maxCharges)
        {
            return false;
        }
    }
    else if (isUnlocked(id))
    {
        return false;
    }

    for (const QString & prereq : def->prerequisites)
    {
        if (!isUnlocked(prereq))
        {
            return false;
        }
    }

    return canAfford(id);
}

bool MetaProgression::purchaseUnlock(const QString & id)
{
    if (!canUnlock(id))
    {
        return false;
    }

    const UnlockDefinition * def = findDefinition(id);
    mEvolutionPoints -= def->cost;

    if (def->category == UnlockCategory::Charge)
    {
        const int newCount = chargesRemaining(id) + 1;
        mCharges[id] = newCount;
        save();
        emit evolutionPointsChanged(mEvolutionPoints);
        emit chargesChanged(id, newCount);
        emit unlockPurchased(id);
    }
    else
    {
        mUnlocked.append(id);
        save();
        emit evolutionPointsChanged(mEvolutionPoints);
        emit unlockPurchased(id);
    }

    return true;
}

QStringList MetaProgression::unlockedIds() const
{
    QStringList ids = mUnlocked;
    for (const UnlockDefinition & def : mCatalog)
    {
        if (def.category != UnlockCategory::Charge && !ids.contains(def.id) && isUnlocked(def.id))
        {
            ids.append(def.id);
        }
    }
    return ids;
}

double MetaProgression::statBonusTotal(const QString & statTarget) const
{
    double total = 0.0;
    for (const UnlockDefinition & def : mCatalog)
    {
        if (def.category == UnlockCategory::StatBonus
            && def.statTarget == statTarget
            && isUnlocked(def.id))
        {
            total += def.statBonusValue;
        }
    }
    return total;
}

double MetaProgression::metabolismBonus() const
{
    return statBonusTotal("metabolism");
}

double MetaProgression::spawningEnergyBonus() const
{
    return statBonusTotal("spawningEnergy");
}

double MetaProgression::mutationRateBonus() const
{
    return statBonusTotal("mutationRate");
}

int MetaProgression::chargesRemaining(const QString & chargeId) const
{
    return mCharges.value(chargeId, 0);
}

bool MetaProgression::consumeCharge(const QString & chargeId)
{
    const int remaining = chargesRemaining(chargeId);
    if (remaining <= 0)
    {
        return false;
    }
    mCharges[chargeId] = remaining - 1;
    save();
    emit chargesChanged(chargeId, remaining - 1);
    return true;
}

void MetaProgression::load()
{
    QSettings settings(kSettingsOrg, kSettingsApp);
    settings.beginGroup(mSettingsGroup);

    mEvolutionPoints = settings.value("EvolutionPoints", 0).toInt();
    mUnlocked = settings.value("Unlocked", QStringList()).toStringList();

    mCharges.clear();
    settings.beginGroup("Charges");
    for (const QString & key : settings.childKeys())
    {
        mCharges[key] = settings.value(key, 0).toInt();
    }
    settings.endGroup();

    settings.endGroup();
}

void MetaProgression::save() const
{
    QSettings settings(kSettingsOrg, kSettingsApp);
    settings.beginGroup(mSettingsGroup);

    settings.setValue("EvolutionPoints", mEvolutionPoints);
    settings.setValue("Unlocked", mUnlocked);

    settings.beginGroup("Charges");
    settings.remove(""); // Clear stale keys before rewriting current stock.
    for (auto it = mCharges.constBegin(); it != mCharges.constEnd(); ++it)
    {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();

    settings.endGroup();
}

void MetaProgression::resetProgress()
{
    mEvolutionPoints = 0;
    mUnlocked.clear();
    mCharges.clear();

    QSettings settings(kSettingsOrg, kSettingsApp);
    settings.beginGroup(mSettingsGroup);
    settings.remove(""); // Drop the whole group, including any stale keys.
    settings.endGroup();

    save();
    emit evolutionPointsChanged(mEvolutionPoints);
}
