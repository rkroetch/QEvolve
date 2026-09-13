#ifndef METAPROGRESSION_H
#define METAPROGRESSION_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "evolutionpoints.h"
#include "runstate.h"

// Persistent, cross-run unlock tree and Evolution Points (EP) wallet.
//
// This is the data/logic half of the between-run "hub" (see HubDialog):
// it owns the catalog of everything that can ever be unlocked, the
// player's current EP balance, which entries are unlocked, and any
// consumable "charge" stock (revives/rerolls). State is persisted via
// QSettings under the same ("ryank", "Evolve") org/app used elsewhere in
// the codebase (see Laboratory), so it lands in the same settings store.
//
// Deliberately independent of Laboratory/Species/UI: it depends only on
// runstate.h and evolutionpoints.h, so it can be constructed and tested
// standalone.

enum class UnlockCategory
{
    StartingSpecies, // A bestiary species (species/*.SPC) usable as a run's starting preset.
    StatBonus,       // A permanent stat bonus (metabolism/spawning energy/mutation rate).
    Biome,           // A plant-pattern/biome option available at hub setup.
    Cosmetic,        // A cosmetic species color option.
    Charge,          // A consumable charge (revive/reroll) - repeatable purchase, stocked count.
};

struct UnlockDefinition
{
    QString id;                 // Stable key, e.g. "species.RAPTORS", "stat.metabolism.2".
    QString displayName;
    QString description;
    UnlockCategory category = UnlockCategory::Cosmetic;
    int cost = 0;                // EP cost per purchase.
    QStringList prerequisites;   // Ids that must already be unlocked first.

    // StatBonus only: which stat this tier modifies ("metabolism",
    // "spawningEnergy", or "mutationRate") and the bonus it contributes
    // (e.g. 0.05 == +5%), summed across every unlocked tier of that stat
    // by statBonusTotal().
    QString statTarget;
    double statBonusValue = 0.0;

    // Charge only: purchasing adds one use, up to this cap (0 means "not
    // a stocked charge" for every other category).
    int maxCharges = 0;
};

class MetaProgression : public QObject
{
    Q_OBJECT

public:
    // `settingsGroup` namespaces this instance's persisted keys under
    // QSettings("ryank", "Evolve") - the same org/app Laboratory uses, so
    // the default ("Meta") lands the real game's progress in that shared
    // store. Tests (or a second, independent save slot) can pass a
    // different group to avoid colliding with/clobbering the default
    // save.
    explicit MetaProgression(QObject * parent = nullptr, const QString & settingsGroup = QStringLiteral("Meta"));

    // The full catalog of unlockable entries (default bestiary/stat/
    // biome/cosmetic/charge set registered at construction). Exposed so
    // the hub UI can enumerate what to show without duplicating the list.
    const QVector<UnlockDefinition> & catalog() const;
    const UnlockDefinition * findDefinition(const QString & id) const;

    int evolutionPoints() const;

    // Adds EP directly (e.g. debug/testing, or a non-run source).
    void awardEP(int amount);

    // Scores a completed run with computeEvolutionPoints(), awards the
    // resulting EP, and returns the amount earned.
    int applyRunResult(const RunResult & result, const EvolutionPointsWeights & weights = EvolutionPointsWeights());

    bool isUnlocked(const QString & id) const;
    bool canAfford(const QString & id) const;

    // True if `id` exists, its prerequisites are satisfied, the player
    // can afford it, and (for non-Charge categories) it is not already
    // unlocked, or (for Charge) its stock is below maxCharges.
    bool canUnlock(const QString & id) const;

    // Attempts to purchase `id`; deducts EP and marks it unlocked (or,
    // for Charge entries, increments stock) on success. Returns false
    // and changes nothing if canUnlock(id) is false.
    bool purchaseUnlock(const QString & id);

    QStringList unlockedIds() const;

    // Sum of statBonusValue across every unlocked StatBonus definition
    // whose statTarget matches (case-sensitive), e.g. statBonusTotal("metabolism").
    double statBonusTotal(const QString & statTarget) const;
    double metabolismBonus() const;
    double spawningEnergyBonus() const;
    double mutationRateBonus() const;

    // Consumable charge stock (revive/reroll, etc).
    int chargesRemaining(const QString & chargeId) const;
    bool consumeCharge(const QString & chargeId);

    // Reloads state from QSettings, discarding any in-memory changes
    // made since the last load()/save().
    void load();

    // Persists current EP/unlocks/charges to QSettings.
    void save() const;

    // Wipes all persisted progress (EP, unlocks, charges) back to
    // defaults. Intended for tests and a "reset save data" UI action.
    void resetProgress();

signals:
    void evolutionPointsChanged(int newBalance);
    void unlockPurchased(QString id);
    void chargesChanged(QString chargeId, int remaining);

private:
    void registerDefaultCatalog();

    QString mSettingsGroup;
    QVector<UnlockDefinition> mCatalog;
    int mEvolutionPoints = 0;
    QStringList mUnlocked;
    QHash<QString, int> mCharges;
};

#endif // METAPROGRESSION_H
