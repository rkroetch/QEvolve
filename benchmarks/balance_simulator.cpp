// Headless roguelike balance simulator for QEvolve's Phase 3 tuning pass
// (see "Claude outputs/qevolve-roguelike-redesign-plan.md" section 4).
//
// This is not a Google Benchmark/Google Test harness: it's a small console
// program that plays out full simulated "runs" (per runstate.h's RunConfig/
// RunResult contract) against the real Species/Animal simulation core -
// exactly the same load/tick/spawn code paths CalculationThread::run() and
// Laboratory use - and reports aggregate outcome distributions (win rate,
// epochs cleared, ticks survived, EP earned) across many runs per starting
// archetype. The goal is to replace by-eye guesses at ticksPerEpoch/
// targetEpochs/EP costs with actual simulated data.
//
// It deliberately mirrors:
//   - benchmarks/species_benchmark.cpp's scatterPopulation()/
//     runSimulationCycle() pattern for driving the simulation core
//     headlessly (advanceCombatCycle -> calculateMovement -> executeMovement
//     -> respawn, single-threaded, no Qt threading/locking/paint overhead).
//   - laboratory.cpp's Laboratory::applyEncounterSpec()/triggerHazard()/
//     advanceHazards() for turning an EncounterSpec into actual rival
//     spawns/plant scarcity/hazard effects. Laboratory itself is Phase 0/1
//     integration code and is intentionally not touched or linked here;
//     this file has its own copies of that logic so behavior can be
//     verified against real .SPC data without requiring Qt widgets/OpenGL.
//
// Known simplification: this harness does not model the player-facing
// mutation-choice picker (MutationChoiceDialog/mutationchoice.h) that fires
// each epoch in the real app - that's a player decision, not a Laboratory
// mechanic, and modeling it well would mean simulating player skill rather
// than measuring the underlying difficulty curve. Every simulated run here
// is therefore a "no intervention" run: the player's starting genome never
// improves over the course of the run. Treat the reported win rates as a
// conservative floor - a real player making good mutation choices should
// do at least as well, usually better.
//
// Build (from repo root, in an already-configured build dir):
//   cmake --build build --config Release --target QEvolveBalanceSim
//   build/Release/QEvolveBalanceSim.exe [runsPerArchetype] [ticksPerEpoch]
//       [targetEpochs] [metaTier] [maxTicksMultiplier] [archetype1,archetype2,...]
// All arguments are optional and positional; trailing ones can be omitted.
// Defaults: runsPerArchetype=60, ticksPerEpoch/targetEpochs/metaTier come
// from RunConfig's current defaults (runstate.h) so this always measures
// whatever the shipped defaults currently are, maxTicksMultiplier=4 (a run
// is called a stalemate/timeout if it neither wins nor goes extinct within
// ticksPerEpoch*targetEpochs*maxTicksMultiplier ticks), and the archetype
// list defaults to a representative spread across the bestiary.

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#include "animal.h"
#include "difficultycurve.h"
#include "evolutionpoints.h"
#include "runstate.h"
#include "species.h"

#ifndef QEVOLVE_SPECIES_DIR
#define QEVOLVE_SPECIES_DIR "species"
#endif

namespace {

// ---------------------------------------------------------------------------
// Bestiary loading - mirrors Laboratory::loadSpecies() (see laboratory.cpp)
// and the Laboratory constructor's plant Species setup, minus anything
// UI-only (per-species button color assignment).
// ---------------------------------------------------------------------------
struct Bestiary
{
    std::vector<std::unique_ptr<Species>> animals;
    std::unique_ptr<Species> plants;
    QHash<QString, Species *> byName;
    QHash<Species *, int> baselineMetabolism;
};

Bestiary loadBestiary()
{
    Bestiary result;

    QDir dir(QStringLiteral(QEVOLVE_SPECIES_DIR));
    dir.setFilter(QDir::Files | QDir::Readable | QDir::NoSymLinks);
    dir.setNameFilters({"*.SPC"});

    for (const QFileInfo & fileInfo : dir.entryInfoList())
    {
        auto species = std::make_unique<Species>(Species::typeAnimal);
        if (!Species::load(fileInfo.filePath(), *species))
        {
            std::fprintf(stderr, "balance_simulator: failed to load %s\n", qPrintable(fileInfo.fileName()));
            continue;
        }
        // Give every species a stable initial-population count (10, matching
        // Laboratory::initActors()'s species->initialize(10, ...)) without
        // spawning anyone yet - simulateRun() re-clears/re-initializes every
        // species at the start of each run anyway, but activate() (used by
        // applyEncounterSpec() when a rival is introduced) reads
        // mInitialAnimalCount, which is otherwise only set inside
        // initialize(). Doing it once here keeps that count stable (10)
        // across every run in a sweep, regardless of run order.
        species->initialize(10, 10000, ANIMAL_INITIAL_ENERGY, false);
        result.baselineMetabolism.insert(species.get(), species->metabolism());
        result.byName.insert(species->name(), species.get());
        result.animals.push_back(std::move(species));
    }

    // Matches the Laboratory constructor's plant Species setup exactly
    // (color aside - irrelevant headlessly).
    auto plants = std::make_unique<Species>(Species::typePlant);
    plants->setName(QStringLiteral("Plant"));
    plants->setMetabolism(10);
    plants->setSpawningEnergy(PLANT_SPAWN_ENERGY);
    for (int friends = 0; friends <= 2; ++friends)
    {
        for (int enemies = 0; enemies <= 2; ++enemies)
        {
            plants->setMovement(friends, enemies, MoveStop);
        }
    }
    plants->setPlantPattern(plantPatternOneGroup);
    plants->initialize(10, MAX_NUM_PLANTS, PLANT_INITIAL_ENERGY);
    result.plants = std::move(plants);

    return result;
}

// ---------------------------------------------------------------------------
// One simulation tick - verbatim structure of
// benchmarks/species_benchmark.cpp's runSimulationCycle(), which itself
// mirrors CalculationThread::run()'s per-cycle body single-threaded.
// ---------------------------------------------------------------------------
void runSimulationCycle(const QVector<Species *> & allSpecies)
{
    for (Species * species : allSpecies)
    {
        species->advanceCombatCycle();
    }

    for (Species * species : allSpecies)
    {
        if (species->type() != Species::typeAnimal)
        {
            continue;
        }
        for (Animal * animal : species->animals())
        {
            animal->calculateMovement();
        }
    }

    for (Species * species : allSpecies)
    {
        const QVector<Animal *> snapshot = species->animals();
        for (Animal * animal : snapshot)
        {
            animal->executeMovement();
        }
    }

    for (Species * species : allSpecies)
    {
        species->respawn(1, 500); // matches CalculationThread::run(); a no-op for typeAnimal species.
    }
}

// ---------------------------------------------------------------------------
// EncounterSpec/hazard application - mirrors Laboratory::applyEncounterSpec()/
// triggerHazard()/advanceHazards() (see laboratory.cpp) exactly, including
// the "activate() also spawns mInitialAnimalCount baseline animals, on top
// of the explicit rival.count spawns below" behavior that a first-time
// rival introduction triggers there. positionLock is a no-op here (this
// harness is single-threaded).
// ---------------------------------------------------------------------------
struct HazardState
{
    QHash<Species *, int> metabolismBaseline;
    int epochsRemaining = 0;
};

void triggerHazard(const HazardEvent & hazard, Species * plants, const std::vector<std::unique_ptr<Species>> & animals, HazardState & hz)
{
    switch (hazard.type)
    {
    case HazardType::None:
        break;

    case HazardType::MetabolismSurge:
        if (hz.metabolismBaseline.isEmpty())
        {
            for (const auto & species : animals)
            {
                hz.metabolismBaseline.insert(species.get(), species->metabolism());
                species->setMetabolism(qMax(1, int(species->metabolism() * (1.0 + hazard.magnitude))));
            }
        }
        hz.epochsRemaining = qMax(hz.epochsRemaining, hazard.durationEpochs);
        break;

    case HazardType::PlantDieOff:
        if (plants)
        {
            const QVector<Animal *> snapshot = plants->animals();
            const int cullCount = int(snapshot.size() * qBound(0.0, hazard.magnitude, 1.0));
            for (int i = 0; i < cullCount; ++i)
            {
                plants->killAnimal(snapshot[i]->cellX(), snapshot[i]->cellY(), snapshot[i]);
            }
        }
        break;

    case HazardType::ResourceBloom:
        if (plants)
        {
            plants->respawn(int(MAX_NUM_PLANTS * qBound(0.0, hazard.magnitude, 1.0)), PLANT_INITIAL_ENERGY);
        }
        break;
    }
}

void advanceHazards(HazardState & hz)
{
    if (hz.epochsRemaining <= 0)
    {
        return;
    }
    if (--hz.epochsRemaining == 0)
    {
        for (auto it = hz.metabolismBaseline.constBegin(); it != hz.metabolismBaseline.constEnd(); ++it)
        {
            it.key()->setMetabolism(it.value());
        }
        hz.metabolismBaseline.clear();
    }
}

void applyEncounterSpec(const EncounterSpec & spec, Species * plants, const std::vector<std::unique_ptr<Species>> & animals, HazardState & hz)
{
    for (const RivalSpawn & rival : spec.rivals)
    {
        Species * target = nullptr;
        for (const auto & species : animals)
        {
            if (species->name().compare(rival.archetype, Qt::CaseInsensitive) == 0)
            {
                target = species.get();
                break;
            }
        }
        if (!target)
        {
            std::fprintf(stderr, "balance_simulator: unknown rival archetype %s\n", qPrintable(rival.archetype));
            continue;
        }

        if (!target->isActive())
        {
            target->activate();
        }

        const int spawningEnergy = qMax(1, int(target->spawningEnergy() * spec.rivalStatMultiplier));
        const int metabolism = qMax(1, int(target->metabolism() * spec.rivalStatMultiplier));
        for (int i = 0; i < rival.count; ++i)
        {
            const QPointF pos(randIntInclusive(5, LABORATORY_WIDTH - 5), randIntInclusive(5, LABORATORY_HEIGHT - 5));
            target->spawnAnimal(pos, ANIMAL_INITIAL_ENERGY, spawningEnergy, metabolism, target->movements(), QPointF(0, 0), nullptr);
        }
    }

    if (plants)
    {
        plants->setSpawningEnergy(qMax(1, int(PLANT_SPAWN_ENERGY * spec.plantSpawnEnergyMultiplier)));
    }

    if (spec.hazard.type != HazardType::None && randDouble01() < spec.hazard.chance)
    {
        triggerHazard(spec.hazard, plants, animals, hz);
    }
}

// ---------------------------------------------------------------------------
// One simulated run.
// ---------------------------------------------------------------------------
struct RunRecord
{
    RunOutcome outcome = RunOutcome::InProgress;
    bool timedOut = false; // Hit the safety tick cap without winning/losing.
    int epochsCleared = 0;
    qint64 ticksSurvived = 0;
    int finalPlayerPopulation = 0;
    int peakPlayerPopulation = 0;
    int evolutionPoints = 0;
};

RunRecord simulateRun(Bestiary & bestiary, Species * player, const RunConfig & config, qint64 maxTicks)
{
    QVector<Species *> allSpecies;
    for (const auto & species : bestiary.animals)
    {
        allSpecies.append(species.get());
    }
    allSpecies.append(bestiary.plants.get());

    // --- Per-run reset -----------------------------------------------------
    // Restore every animal species' metabolism to its .SPC baseline, in case
    // a MetabolismSurge hazard was still active when a previous run ended
    // (extinction can cut a run short before advanceHazards() reverts it).
    for (const auto & species : bestiary.animals)
    {
        species->setMetabolism(bestiary.baselineMetabolism.value(species.get()));
    }

    // Deactivate every animal species so applyEncounterSpec()'s "activate()
    // on first introduction" behavior fires correctly for whichever
    // archetypes this epoch schedule introduces - see the comment on
    // applyEncounterSpec() above.
    for (const auto & species : bestiary.animals)
    {
        if (species->isActive())
        {
            species->deactivate();
        }
    }
    if (!player->isActive())
    {
        player->activate();
    }

    for (const auto & species : bestiary.animals)
    {
        species->clear();
        species->initialize(10, 10000, ANIMAL_INITIAL_ENERGY, species.get() == player);
    }
    bestiary.plants->clear();
    bestiary.plants->setSpawningEnergy(PLANT_SPAWN_ENERGY);
    bestiary.plants->initialize(10, MAX_NUM_PLANTS, PLANT_INITIAL_ENERGY);

    // --- Tick loop -----------------------------------------------------
    HazardState hz;
    qint64 ticksSurvived = 0;
    int currentEpoch = 0;
    RunOutcome outcome = RunOutcome::InProgress;
    int peakPlayerPopulation = int(player->animals().size());

    while (outcome == RunOutcome::InProgress && ticksSurvived < maxTicks)
    {
        runSimulationCycle(allSpecies);
        ++ticksSurvived;
        peakPlayerPopulation = qMax(peakPlayerPopulation, int(player->animals().size()));

        const int epoch = computeEpoch(ticksSurvived, config.ticksPerEpoch);
        while (currentEpoch < epoch)
        {
            ++currentEpoch;
            advanceHazards(hz);
            applyEncounterSpec(computeEncounterSpec(currentEpoch, config.metaTier), bestiary.plants.get(), bestiary.animals, hz);
        }

        const bool playerExtinct = player->animals().isEmpty();
        outcome = evaluateRunOutcome(playerExtinct, currentEpoch, config.targetEpochs);
    }

    RunRecord record;
    record.timedOut = (outcome == RunOutcome::InProgress);
    record.outcome = record.timedOut ? RunOutcome::Lost : outcome; // Treat a stalemate as a loss for scoring purposes.
    record.epochsCleared = currentEpoch;
    record.ticksSurvived = ticksSurvived;
    record.finalPlayerPopulation = int(player->animals().size());
    record.peakPlayerPopulation = peakPlayerPopulation;

    RunResult result;
    result.outcome = record.outcome;
    result.epochsCleared = currentEpoch;
    result.ticksSurvived = ticksSurvived;
    for (const auto & species : bestiary.animals)
    {
        SpeciesRunStats stats;
        stats.name = species->name();
        stats.finalPopulation = int(species->animals().size());
        for (const Animal * animal : species->animals())
        {
            stats.highestGeneration = qMax(stats.highestGeneration, animal->statistics().mGeneration);
            stats.totalChildren += animal->statistics().mNumChildren;
        }
        result.speciesStats.append(stats);
    }
    record.evolutionPoints = evolutionPointsEarned(result);

    return record;
}

// ---------------------------------------------------------------------------
// Aggregation / reporting.
// ---------------------------------------------------------------------------
struct AggregateStats
{
    int runs = 0;
    int wins = 0;
    int timeouts = 0;
    double winRate = 0.0;
    double medianEpochsCleared = 0.0;
    double meanEpochsCleared = 0.0;
    double meanTicksSurvived = 0.0;
    double meanPeakPopulation = 0.0;
    double meanEP = 0.0;
    double medianEP = 0.0;
};

double median(std::vector<double> values)
{
    if (values.empty())
    {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const size_t mid = values.size() / 2;
    if (values.size() % 2 == 0)
    {
        return (values[mid - 1] + values[mid]) / 2.0;
    }
    return values[mid];
}

AggregateStats aggregate(const std::vector<RunRecord> & records)
{
    AggregateStats stats;
    stats.runs = int(records.size());
    if (records.empty())
    {
        return stats;
    }

    std::vector<double> epochs;
    std::vector<double> eps;
    double sumEpochs = 0.0;
    double sumTicks = 0.0;
    double sumPeak = 0.0;
    double sumEP = 0.0;

    for (const RunRecord & record : records)
    {
        if (record.outcome == RunOutcome::Won)
        {
            ++stats.wins;
        }
        if (record.timedOut)
        {
            ++stats.timeouts;
        }
        epochs.push_back(record.epochsCleared);
        eps.push_back(record.evolutionPoints);
        sumEpochs += record.epochsCleared;
        sumTicks += double(record.ticksSurvived);
        sumPeak += record.peakPlayerPopulation;
        sumEP += record.evolutionPoints;
    }

    stats.winRate = double(stats.wins) / double(stats.runs);
    stats.medianEpochsCleared = median(epochs);
    stats.meanEpochsCleared = sumEpochs / stats.runs;
    stats.meanTicksSurvived = sumTicks / stats.runs;
    stats.meanPeakPopulation = sumPeak / stats.runs;
    stats.meanEP = sumEP / stats.runs;
    stats.medianEP = median(eps);
    return stats;
}

QStringList defaultArchetypes()
{
    // A representative spread across the always-available bestiary (see
    // metaprogression.cpp's bestiarySpeciesNames()): the free starting kit
    // (RABBITS), two other herbivore/omnivore presets, and two of the more
    // aggressive presets, deliberately excluding APEX (capstone rival, not
    // a player starting preset) and CRUISER (identical stock stats/genome
    // to RAPTORS/MCCOY/HATFIELD/CHICKEN - all Go everywhere - so it adds no
    // new signal over RAPTORS below).
    return {"RABBITS", "CHICKEN", "SLUGS", "RAPTORS", "MCCOY", "BRACHIO"};
}

} // namespace

int main(int argc, char ** argv)
{
    const RunConfig defaultConfig;

    int runsPerArchetype = 60;
    qint64 ticksPerEpoch = defaultConfig.ticksPerEpoch;
    int targetEpochs = defaultConfig.targetEpochs;
    int metaTier = defaultConfig.metaTier;
    int maxTicksMultiplier = 4;
    QStringList archetypes = defaultArchetypes();

    if (argc > 1) runsPerArchetype = std::atoi(argv[1]);
    if (argc > 2) ticksPerEpoch = std::atoll(argv[2]);
    if (argc > 3) targetEpochs = std::atoi(argv[3]);
    if (argc > 4) metaTier = std::atoi(argv[4]);
    if (argc > 5) maxTicksMultiplier = std::atoi(argv[5]);
    if (argc > 6) archetypes = QString::fromLocal8Bit(argv[6]).split(',', Qt::SkipEmptyParts);

    RunConfig config = defaultConfig;
    config.ticksPerEpoch = ticksPerEpoch;
    config.targetEpochs = targetEpochs;
    config.metaTier = metaTier;
    const qint64 maxTicks = ticksPerEpoch * qint64(targetEpochs) * qint64(maxTicksMultiplier);

    std::printf("QEvolve balance simulator\n");
    std::printf("  runsPerArchetype=%d ticksPerEpoch=%lld targetEpochs=%d metaTier=%d maxTicks=%lld\n\n",
                 runsPerArchetype, (long long)ticksPerEpoch, targetEpochs, metaTier, (long long)maxTicks);

    Bestiary bestiary = loadBestiary();
    if (bestiary.animals.empty())
    {
        std::fprintf(stderr, "balance_simulator: no species loaded from %s - run from a directory containing species/, "
                              "or check QEVOLVE_SPECIES_DIR\n", QEVOLVE_SPECIES_DIR);
        return 1;
    }

    std::printf("%-10s %6s %8s %9s %9s %9s %9s %9s %9s\n",
                 "Species", "Runs", "WinRate", "MedEpoch", "MeanEpoch", "MeanTicks", "PeakPop", "MeanEP", "Timeout%");

    for (const QString & archetypeName : archetypes)
    {
        Species * player = bestiary.byName.value(archetypeName, nullptr);
        if (!player)
        {
            std::fprintf(stderr, "balance_simulator: unknown starting archetype '%s' - skipping\n", qPrintable(archetypeName));
            continue;
        }

        std::vector<RunRecord> records;
        records.reserve(runsPerArchetype);
        for (int i = 0; i < runsPerArchetype; ++i)
        {
            records.push_back(simulateRun(bestiary, player, config, maxTicks));
        }

        const AggregateStats stats = aggregate(records);
        std::printf("%-10s %6d %7.1f%% %9.2f %9.2f %9.1f %9.1f %9.1f %8.1f%%\n",
                     qPrintable(archetypeName), stats.runs, stats.winRate * 100.0,
                     stats.medianEpochsCleared, stats.meanEpochsCleared, stats.meanTicksSurvived,
                     stats.meanPeakPopulation, stats.meanEP, (double(stats.timeouts) / double(stats.runs)) * 100.0);
    }

    return 0;
}
