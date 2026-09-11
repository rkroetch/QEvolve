// Google Benchmark micro-benchmarks for the Species/Animal simulation core.
//
// These exercise exactly the operations CalculationThread::run() performs
// every simulation cycle (see laboratory.cpp), just without the Qt
// threading/locking/paint plumbing around them, so the numbers here track
// the app's own "calculations / second" metric.
//
// Build (from repo root, in an already-configured build dir):
//   cmake -S . -B build -DQEVOLVE_BUILD_BENCHMARKS=ON
//   cmake --build build --config Release --target QEvolveBenchmarks
//   build/Release/QEvolveBenchmarks.exe
//
// Use this to compare before/after a change to Species' occupancy storage,
// combat-claim logic, or Animal's hot-path fields: build once on each side
// of the change (e.g. via `git stash`) and diff the reported ns/op.
//
// BM_LargeScaleSimulation is a long-running soak test (up to 10,000 animals
// x 10,000,000 cycles) rather than a quick benchmark - see its comment
// below before running the full-size variant.
//
// BM_LargeScaleSimulationPersistentPool runs the identical workload through
// CycleConcurrent (cycleconcurrent.h) - the persistent-worker-pool
// threading backend laboratory.cpp's CalculationThread::run() can opt into
// via the QEVOLVE_USE_PERSISTENT_THREAD_POOL CMake option - instead of
// QtConcurrent::blockingMap. Compare it against BM_LargeScaleSimulation
// directly - same Args, same population/cycle counts.

#include <benchmark/benchmark.h>

#include <QAtomicInteger>
#include <QPair>
#include <QThread>
#include <QtConcurrent/QtConcurrentMap>

#include <algorithm>
#include <cstdio>
#include <memory>

#include "animal.h"
#include "cycleconcurrent.h"
#include "species.h"

namespace {

Movements randomWalkMovements()
{
    // Every friends/enemies bucket wanders randomly, which is the worst
    // case (highest churn) for the per-cell occupancy structure - animals
    // change cell almost every cycle instead of sitting still.
    Movements movements;
    for (int friends = 0; friends <= 2; ++friends)
    {
        for (int enemies = 0; enemies <= 2; ++enemies)
        {
            movements.setMovement(friends, enemies, MoveRandom);
        }
    }
    return movements;
}

// Spawns `count` additional active animals scattered across the grid using
// the same public spawnAnimal() path the real simulation uses for
// reproduction, so the resulting per-cell occupancy pattern is realistic.
// `species` must already have been initialize()'d with enough inactive
// capacity (at least `count`).
void scatterPopulation(Species * species, int count, double energy, int spawningEnergy,
                        int metabolism, const Movements & movements)
{
    for (int i = 0; i < count; ++i)
    {
        const QPointF pos(randIntInclusive(0, LABORATORY_WIDTH - 1), randIntInclusive(0, LABORATORY_HEIGHT - 1));
        species->spawnAnimal(pos, energy, spawningEnergy, metabolism, movements, QPointF(0, 0), nullptr);
    }
}

// Runs exactly one CalculationThread::run() cycle (advanceCombatCycle ->
// calculateMovement -> executeMovement -> respawn) over `allSpecies`,
// single-threaded, and returns the number of animals that went through
// executeMovement(), for throughput reporting. Used by
// BM_FullSimulationCycle, where determinism/low variance for isolating a
// specific hot-path cost matters more than matching the real app's
// threading - see runSimulationCycleThreaded() below for the version that
// actually reproduces CalculationThread::run()'s concurrency (used by
// BM_LargeScaleSimulation).
qint64 runSimulationCycle(const QVector<Species *> & allSpecies)
{
    for (Species * species : allSpecies)
    {
        species->advanceCombatCycle();
    }

    // Sequential stand-in for the read-locked "calculate" pass, which in
    // laboratory.cpp is fanned out across worker threads via
    // QtConcurrent::blockingMap (see runSimulationCycleThreaded()).
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

    // Mirrors the write-locked "execute" pass. Snapshot first: an animal's
    // executeMovement() can kill or spawn, mutating the very list we're
    // iterating - exactly why laboratory.cpp's executeSpecies() takes a
    // copy before iterating too.
    qint64 itemsProcessed = 0;
    for (Species * species : allSpecies)
    {
        const QVector<Animal *> snapshot = species->animals();
        for (Animal * animal : snapshot)
        {
            animal->executeMovement();
        }
        itemsProcessed += snapshot.size();
    }

    for (Species * species : allSpecies)
    {
        species->respawn(1, PLANT_INITIAL_ENERGY);
    }

    return itemsProcessed;
}

// Verbatim copy of laboratory.cpp's calculateRange() free function: applies
// calculateMovement() across one contiguous slice of the flattened animal
// pointer list. Kept as a free function (not a lambda) specifically so it
// can be diffed against laboratory.cpp's copy to catch drift.
void calculateRange(const QPair<Animal * const *, int> & range)
{
    Animal * const * ptrs = range.first;
    const int count = range.second;
    for (int i = 0; i < count; ++i)
    {
        ptrs[i]->calculateMovement();
    }
}

// Persistent-pool counterpart to runSimulationCycleThreaded(): identical
// per-cycle logic and partitioning (advanceCombatCycle -> calculateMovement
// over calculateRange() ranges -> executeMovement() one species per worker
// -> respawn), but fanned out through CycleConcurrent::blockingMap
// (cycleconcurrent.h) instead of QtConcurrent::blockingMap - the exact same
// shared header laboratory.cpp's ActiveConcurrent alias uses when
// QEVOLVE_USE_PERSISTENT_THREAD_POOL is defined, so this benchmark
// exercises the real production code path, not a reimplementation of it.
// See cycleconcurrent.h for why this backend exists and its measured
// trade-off (BM_LargeScaleSimulation vs BM_LargeScaleSimulationPersistentPool
// below are the numbers referenced there).
qint64 runSimulationCyclePersistentPool(const QVector<Species *> & allSpecies, QVector<Animal *> & calcScratch)
{
    for (Species * species : allSpecies)
    {
        species->advanceCombatCycle();
    }

    calcScratch.clear();
    for (Species * species : allSpecies)
    {
        if (species->type() == Species::typeAnimal)
        {
            calcScratch += species->animals();
        }
    }

    if (!calcScratch.isEmpty())
    {
        const int n = int(calcScratch.size());
        const int threadCount = qMax(1, qMin(n, CycleConcurrent::threadLocalPool().threadCount()));
        QVector<QPair<Animal * const *, int>> ranges;
        ranges.reserve(threadCount);
        for (int t = 0; t < threadCount; ++t)
        {
            const int begin = t * n / threadCount;
            const int end = (t + 1) * n / threadCount;
            if (begin < end)
            {
                ranges.append(qMakePair(calcScratch.constData() + begin, end - begin));
            }
        }
        CycleConcurrent::blockingMap(ranges, calculateRange);
    }

    QAtomicInteger<qint64> itemsProcessed{0};
    CycleConcurrent::blockingMap(allSpecies, [&itemsProcessed](Species * species) {
        const QVector<Animal *> snapshot = species->animals();
        for (Animal * animal : snapshot)
        {
            animal->executeMovement();
        }
        itemsProcessed.fetchAndAddRelaxed(qint64(snapshot.size()));
    });

    for (Species * species : allSpecies)
    {
        species->respawn(1, PLANT_INITIAL_ENERGY);
    }

    return itemsProcessed.loadRelaxed();
}

// Threaded counterpart to runSimulationCycle(): reproduces
// CalculationThread::run()'s actual concurrency, not just its sequence of
// steps. The "calculate" pass flattens every animal species into
// `calcScratch` (a scratch buffer reused cycle to cycle, exactly like
// CalculationThread::mCalcAnimals) and fans calculateMovement() out across
// QThread::idealThreadCount() contiguous ranges via
// QtConcurrent::blockingMap - the same partitioning arithmetic as
// laboratory.cpp. This is what actually puts Animal::tryClaimEaten() and
// Species::tryClaimCombatCell() under real cross-thread contention, which
// runSimulationCycle()'s single-threaded version never exercises.
//
// The "execute" pass fans executeMovement() out one thread per species
// (matching laboratory.cpp's executeSpecies()), snapshotting each species'
// animals first since executeMovement() can kill/spawn into the very list
// being iterated. A QAtomicInteger tallies the processed count across
// those concurrent per-species tasks for throughput reporting.
qint64 runSimulationCycleThreaded(const QVector<Species *> & allSpecies, QVector<Animal *> & calcScratch)
{
    for (Species * species : allSpecies)
    {
        species->advanceCombatCycle();
    }

    calcScratch.clear();
    for (Species * species : allSpecies)
    {
        if (species->type() == Species::typeAnimal)
        {
            calcScratch += species->animals();
        }
    }

    if (!calcScratch.isEmpty())
    {
        const int n = int(calcScratch.size());
        const int threadCount = qMax(1, qMin(n, QThread::idealThreadCount()));
        QVector<QPair<Animal * const *, int>> ranges;
        ranges.reserve(threadCount);
        for (int t = 0; t < threadCount; ++t)
        {
            const int begin = t * n / threadCount;
            const int end = (t + 1) * n / threadCount;
            if (begin < end)
            {
                ranges.append(qMakePair(calcScratch.constData() + begin, end - begin));
            }
        }
        QtConcurrent::blockingMap(ranges, calculateRange);
    }

    QAtomicInteger<qint64> itemsProcessed{0};
    QtConcurrent::blockingMap(allSpecies, [&itemsProcessed](Species * species) {
        const QVector<Animal *> snapshot = species->animals();
        for (Animal * animal : snapshot)
        {
            animal->executeMovement();
        }
        itemsProcessed.fetchAndAddRelaxed(qint64(snapshot.size()));
    });

    for (Species * species : allSpecies)
    {
        species->respawn(1, PLANT_INITIAL_ENERGY);
    }

    return itemsProcessed.loadRelaxed();
}

} // namespace

// ---------------------------------------------------------------------------
// Occupancy churn: every animal moves to a neighboring cell every cycle,
// which is exactly Species::moveAnimal()'s workload from
// Animal::executeMovement(). This isolates the per-cell occupancy storage
// (flat array vs. e.g. a hash map keyed by packed cell index) from
// everything else in the cycle.
// ---------------------------------------------------------------------------
static void BM_OccupancyChurn(benchmark::State & state)
{
    const int population = static_cast<int>(state.range(0));

    auto species = std::make_unique<Species>(Species::typeAnimal);
    species->initialize(0, population + 1, 900);
    scatterPopulation(species.get(), population - 1, 900, 1000, 100, randomWalkMovements());

    const QVector<Animal *> animals = species->animals();

    for (auto _ : state)
    {
        for (Animal * animal : animals)
        {
            const int oldX = animal->cellX();
            const int oldY = animal->cellY();
            const int newX = clampCellX(oldX + randIntInclusive(-1, 1));
            const int newY = clampCellY(oldY + randIntInclusive(-1, 1));
            species->moveAnimal(oldX, oldY, newX, newY, animal);
            animal->setPos(QPointF(newX, newY));
        }
    }

    state.SetItemsProcessed(state.iterations() * animals.size());
}
BENCHMARK(BM_OccupancyChurn)->Arg(500)->Arg(2000)->Arg(8000)->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// Neighborhood queries: friendCount/enemyCount/plantCount are called
// unconditionally for every animal, every cycle, in
// Animal::calculateMovement(). Read-only, so it's safe to run indefinitely
// without the population/state drifting.
// ---------------------------------------------------------------------------
static void BM_NeighborhoodQueries(benchmark::State & state)
{
    const int population = static_cast<int>(state.range(0));

    auto plants = std::make_unique<Species>(Species::typePlant);
    plants->initialize(0, MAX_NUM_PLANTS, PLANT_INITIAL_ENERGY);
    scatterPopulation(plants.get(), MAX_NUM_PLANTS - 1, PLANT_INITIAL_ENERGY, PLANT_SPAWN_ENERGY, 100, Movements());

    auto subjects = std::make_unique<Species>(Species::typeAnimal);
    subjects->initialize(0, population + 1, 900);
    scatterPopulation(subjects.get(), population - 1, 900, 1000, 100, randomWalkMovements());

    auto rivals = std::make_unique<Species>(Species::typeAnimal);
    rivals->initialize(0, population + 1, 900);
    scatterPopulation(rivals.get(), population - 1, 900, 1000, 100, randomWalkMovements());

    const QVector<Animal *> animals = subjects->animals();

    for (auto _ : state)
    {
        for (const Animal * animal : animals)
        {
            const int x = animal->cellX();
            const int y = animal->cellY();
            benchmark::DoNotOptimize(subjects->friendCount(x, y));
            benchmark::DoNotOptimize(subjects->enemyCount(x, y));
            benchmark::DoNotOptimize(subjects->plantCount(x, y));
        }
    }

    state.SetItemsProcessed(state.iterations() * animals.size());
}
BENCHMARK(BM_NeighborhoodQueries)->Arg(200)->Arg(800)->Arg(3000)->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// Full simulation cycle: reproduces CalculationThread::run()'s per-cycle
// body (advanceCombatCycle -> calculateMovement -> executeMovement ->
// respawn) single-threaded, over a plant species and two competing animal
// species, so it also exercises eatWeakestPlant/killWeakestEnemy/
// killAnimal/spawnAnimal - the mutating paths the other two benchmarks
// deliberately avoid. This is the closest proxy to the app's own
// "calculations / second" readout.
//
// Note: population size is allowed to drift cycle to cycle exactly like it
// does in the real app (deaths/births aren't pinned back to a fixed
// count) - SetItemsProcessed() tallies the actual animals processed each
// iteration rather than assuming a constant, so throughput stays accurate
// even so.
// ---------------------------------------------------------------------------
static void BM_FullSimulationCycle(benchmark::State & state)
{
    const int animalPopulation = static_cast<int>(state.range(0));

    auto plants = std::make_unique<Species>(Species::typePlant);
    plants->initialize(0, MAX_NUM_PLANTS, PLANT_INITIAL_ENERGY);
    scatterPopulation(plants.get(), MAX_NUM_PLANTS / 2 - 1, PLANT_INITIAL_ENERGY, PLANT_SPAWN_ENERGY, 100, Movements());

    auto preyA = std::make_unique<Species>(Species::typeAnimal);
    preyA->initialize(0, animalPopulation * 4, 900);
    scatterPopulation(preyA.get(), animalPopulation - 1, 900, 1000, 100, randomWalkMovements());

    auto preyB = std::make_unique<Species>(Species::typeAnimal);
    preyB->initialize(0, animalPopulation * 4, 900);
    scatterPopulation(preyB.get(), animalPopulation - 1, 900, 1000, 100, randomWalkMovements());

    const QVector<Species *> allSpecies{plants.get(), preyA.get(), preyB.get()};

    qint64 itemsProcessed = 0;
    for (auto _ : state)
    {
        itemsProcessed += runSimulationCycle(allSpecies);
    }

    state.SetItemsProcessed(itemsProcessed);
}
BENCHMARK(BM_FullSimulationCycle)->Arg(150)->Arg(600)->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// Large-scale soak test: up to 10,000 animals (split across two competing
// species, same as BM_FullSimulationCycle) plus a plant population that
// keeps randomly (re)spawning one new plant per cycle via Species::respawn,
// run for a large, fixed number of cycles instead of letting Google
// Benchmark auto-repeat to fill its usual ~0.5s window.
//
// Unlike the other three benchmarks (which call calculateMovement()/
// executeMovement() directly, single-threaded, for a clean/deterministic
// signal on a specific hot path), this one drives the cycle through
// runSimulationCycleThreaded() - the same QtConcurrent::blockingMap
// partitioning CalculationThread::run() actually uses. That matters here
// specifically: it's the only benchmark that puts Animal::tryClaimEaten()
// and Species::tryClaimCombatCell() under genuine cross-thread contention,
// and at 10,000 animals the thread-pool/false-sharing/cache behavior is
// materially different from a serial loop - the whole point of this
// benchmark is to be the accurate real-world proxy, so it should pay for
// the concurrency exactly like the app does.
//
// This is a SOAK test, not a quick benchmark: at large N x cycle counts
// this is on the order of 10^8-10^9+ animal-updates. It's meant to be
// kicked off standalone (overnight, in a dedicated soak-test CI job,
// etc.), not run as part of a routine sweep. It logs progress to stderr
// (~20 lines) since a long run would otherwise give no feedback for a
// very long time.
//
// Two argument sets are registered:
//   BM_LargeScaleSimulation/100/2000     - quick smoke check (<1s), confirms
//                                           the threaded loop runs correctly.
//   BM_LargeScaleSimulation/10000/100000 - the real soak test. Run it on
//                                           its own:
//     QEvolveBenchmarks.exe --benchmark_filter=BM_LargeScaleSimulation/10000/100000
//
// Like BM_FullSimulationCycle, population is not pinned back to a fixed
// count - it can grow, shrink, or collapse over the run exactly as it
// would in the real app. SetItemsProcessed() tallies actual animals
// processed rather than assuming a constant, so throughput stays accurate
// even if it drifts.
// ---------------------------------------------------------------------------
static void BM_LargeScaleSimulation(benchmark::State & state)
{
    const int animalPopulation = static_cast<int>(state.range(0));
    const long long numCycles = state.range(1);
    const int perSpeciesPopulation = animalPopulation / 2;

    auto plants = std::make_unique<Species>(Species::typePlant);
    plants->initialize(0, MAX_NUM_PLANTS, PLANT_INITIAL_ENERGY);
    scatterPopulation(plants.get(), MAX_NUM_PLANTS / 2 - 1, PLANT_INITIAL_ENERGY, PLANT_SPAWN_ENERGY, 100, Movements());

    auto preyA = std::make_unique<Species>(Species::typeAnimal);
    preyA->initialize(0, perSpeciesPopulation * 4, 900);
    scatterPopulation(preyA.get(), perSpeciesPopulation - 1, 900, 1000, 100, randomWalkMovements());

    auto preyB = std::make_unique<Species>(Species::typeAnimal);
    preyB->initialize(0, perSpeciesPopulation * 4, 900);
    scatterPopulation(preyB.get(), perSpeciesPopulation - 1, 900, 1000, 100, randomWalkMovements());

    const QVector<Species *> allSpecies{plants.get(), preyA.get(), preyB.get()};
    const long long progressInterval = std::max<long long>(1, numCycles / 20);

    QVector<Animal *> calcScratch;

    for (auto _ : state)
    {
        qint64 itemsProcessed = 0;
        for (long long cycle = 0; cycle < numCycles; ++cycle)
        {
            itemsProcessed += runSimulationCycleThreaded(allSpecies, calcScratch);

            if ((cycle % progressInterval) == 0)
            {
                std::fprintf(stderr, "BM_LargeScaleSimulation: cycle %lld/%lld (plants=%d preyA=%d preyB=%d)\n",
                             cycle, numCycles, int(plants->animals().size()),
                             int(preyA->animals().size()), int(preyB->animals().size()));
            }
        }
        state.SetItemsProcessed(itemsProcessed);
    }
}
BENCHMARK(BM_LargeScaleSimulation)
    ->Args({100, 2000})
    ->Args({10000, 100000})
    ->Iterations(1)
    ->Unit(benchmark::kSecond)
    ->UseRealTime();

// ---------------------------------------------------------------------------
// Same workload and Args as BM_LargeScaleSimulation, but driven through
// runSimulationCyclePersistentPool() (CycleConcurrent::blockingMap, see
// cycleconcurrent.h) instead of runSimulationCycleThreaded()
// (QtConcurrent::blockingMap), to quantify the per-cycle dispatch-overhead
// win/CPU-time cost of the QEVOLVE_USE_PERSISTENT_THREAD_POOL backend that
// CalculationThread::run() can opt into. Compare the two directly with:
//   QEvolveBenchmarks.exe --benchmark_filter=BM_LargeScale.*/10000/100000
// A smaller real-time number here than BM_LargeScaleSimulation's, for the
// same population/cycle count, is the wall-clock win; check CPU time too -
// see cycleconcurrent.h for the trade-off this backend makes.
// ---------------------------------------------------------------------------
static void BM_LargeScaleSimulationPersistentPool(benchmark::State & state)
{
    const int animalPopulation = static_cast<int>(state.range(0));
    const long long numCycles = state.range(1);
    const int perSpeciesPopulation = animalPopulation / 2;

    auto plants = std::make_unique<Species>(Species::typePlant);
    plants->initialize(0, MAX_NUM_PLANTS, PLANT_INITIAL_ENERGY);
    scatterPopulation(plants.get(), MAX_NUM_PLANTS / 2 - 1, PLANT_INITIAL_ENERGY, PLANT_SPAWN_ENERGY, 100, Movements());

    auto preyA = std::make_unique<Species>(Species::typeAnimal);
    preyA->initialize(0, perSpeciesPopulation * 4, 900);
    scatterPopulation(preyA.get(), perSpeciesPopulation - 1, 900, 1000, 100, randomWalkMovements());

    auto preyB = std::make_unique<Species>(Species::typeAnimal);
    preyB->initialize(0, perSpeciesPopulation * 4, 900);
    scatterPopulation(preyB.get(), perSpeciesPopulation - 1, 900, 1000, 100, randomWalkMovements());

    const QVector<Species *> allSpecies{plants.get(), preyA.get(), preyB.get()};
    const long long progressInterval = std::max<long long>(1, numCycles / 20);

    CycleConcurrent::threadLocalPool(); // warm up the persistent worker threads before timing
    QVector<Animal *> calcScratch;

    for (auto _ : state)
    {
        qint64 itemsProcessed = 0;
        for (long long cycle = 0; cycle < numCycles; ++cycle)
        {
            itemsProcessed += runSimulationCyclePersistentPool(allSpecies, calcScratch);

            if ((cycle % progressInterval) == 0)
            {
                std::fprintf(stderr, "BM_LargeScaleSimulationPersistentPool: cycle %lld/%lld (plants=%d preyA=%d preyB=%d)\n",
                             cycle, numCycles, int(plants->animals().size()),
                             int(preyA->animals().size()), int(preyB->animals().size()));
            }
        }
        state.SetItemsProcessed(itemsProcessed);
    }
}
BENCHMARK(BM_LargeScaleSimulationPersistentPool)
    ->Args({100, 2000})
    ->Args({10000, 100000})
    ->Iterations(1)
    ->Unit(benchmark::kSecond)
    ->UseRealTime();

BENCHMARK_MAIN();
