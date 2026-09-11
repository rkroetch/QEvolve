#ifndef CYCLECONCURRENT_H
#define CYCLECONCURRENT_H

// An alternative to QtConcurrent::blockingMap() for hot loops that call it
// repeatedly with no gap in between - e.g. CalculationThread::run() in
// laboratory.cpp at speed == 0 (see mSpeed/usleep there: 0 is the default
// and means "no delay between cycles"). QtConcurrent::blockingMap()
// re-submits to Qt's shared global thread pool on every call (QFuture +
// task-queue machinery per call), which is cheap once per paced frame but
// dominates wall time when called twice a cycle, hundreds of thousands of
// times back to back - most of the wall clock ends up spent in dispatch,
// not the mapped work itself.
//
// CycleConcurrent::blockingMap() keeps a small pool of worker threads alive
// for the life of the calling thread instead: one CyclePhaseBarrierPool per
// caller, lazily created on first use and reused via thread_local storage
// (mirroring QtConcurrent's own implicit global pool, just scoped to the
// calling thread rather than the whole process), synchronized by a
// generation-counter barrier. Per-call cost becomes "flip an atomic and
// wait on your own dedicated threads" instead of "enqueue tasks on a
// shared pool and block" - see CyclePhaseBarrierPool::runPhase().
//
// blockingMap(sequence, fn) matches QtConcurrent::blockingMap(sequence,
// fn)'s observable contract: fn(item) is invoked once for every item in
// `sequence`, across worker threads, and the call blocks until all of them
// are done. That contract match is what makes switching backends a
// namespace swap - see ActiveConcurrent in laboratory.cpp.
//
// Measured trade-off (benchmarks/species_benchmark.cpp,
// BM_LargeScaleSimulation vs BM_LargeScaleSimulationPersistentPool, 10,000
// animals x 100,000 cycles): wall time roughly 2.5x lower, but total
// CPU-seconds burned across all cores roughly 4x higher. A spinning worker
// isn't truly idle the way a parked QThreadPool worker is, so this trades
// wall-clock latency for CPU utilization - worth it when nothing else is
// competing for the machine's cores, worth reconsidering if QEvolve ever
// needs to share the box.
//
// A CyclePhaseBarrierPool's worker threads are ordinary std::thread's, so
// they're torn down automatically (via thread_local destruction) when the
// owning thread itself exits - no explicit shutdown call is needed at
// CalculationThread::stop() time. That teardown detaches the workers rather
// than joining them: thread_local destructors run as part of the owning
// thread's own OS-level thread-exit sequence (on Windows, while its
// DLL_THREAD_DETACH notification holds the loader lock), and join()ing
// another thread from inside that sequence deadlocks, since the joined
// thread can't complete its own thread-exit sequence without that same
// lock. See ~CyclePhaseBarrierPool().

#include <QThread>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class CyclePhaseBarrierPool
{
public:
    explicit CyclePhaseBarrierPool(int threadCount)
        : mThreadCount(qMax(1, threadCount))
    {
        mThreads.reserve(mThreadCount);
        for (int i = 0; i < mThreadCount; ++i)
        {
            mThreads.emplace_back([this, i] { workerLoop(i); });
        }
    }

    ~CyclePhaseBarrierPool()
    {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStop.store(true, std::memory_order_relaxed);
            mGeneration.fetch_add(1, std::memory_order_release);
        }
        mCv.notify_all();

        // Wait for every worker to observe the stop signal and finish
        // touching this pool's members, via our own atomic counter rather
        // than std::thread::join(). This destructor runs as part of the
        // owning thread's thread_local teardown (see threadLocalPool()
        // below), which on Windows executes while that thread is inside its
        // own thread-exit sequence (DLL_THREAD_DETACH) holding the loader
        // lock. join()ing another thread from there deadlocks: the joined
        // thread cannot complete its own thread-exit sequence - needed
        // before join() would return - without acquiring that same loader
        // lock. Detaching instead, once mExited confirms each worker is done
        // touching `this`, lets every worker finish its own OS-level
        // teardown independently, with no thread here waiting on it.
        int spins = 0;
        while (mExited.load(std::memory_order_acquire) != mThreadCount)
        {
            if (spins++ < kSpinLimit)
            {
                std::this_thread::yield();
            }
            else
            {
                std::unique_lock<std::mutex> lock(mMutex);
                mCv.wait_for(lock, std::chrono::microseconds(50), [this] {
                    return mExited.load(std::memory_order_acquire) == mThreadCount;
                });
            }
        }

        for (std::thread & t : mThreads)
        {
            t.detach();
        }
    }

    int threadCount() const { return mThreadCount; }

    // Invokes fn(workerIndex) exactly once on each of the pool's threads and
    // blocks the caller until every worker has finished this phase. fn only
    // needs to stay valid for the duration of this call - runPhase() doesn't
    // return until every worker is done with it.
    void runPhase(const std::function<void(int)> & fn)
    {
        mPhaseFn = &fn;
        mCompleted.store(0, std::memory_order_relaxed);
        {
            // Hold the mutex across the generation bump so a worker that is
            // right on the boundary of parking can't miss this wakeup
            // (classic lost-wakeup race between the predicate check and
            // condition_variable::wait()).
            std::lock_guard<std::mutex> lock(mMutex);
            mGeneration.fetch_add(1, std::memory_order_release);
        }
        mCv.notify_all();

        int spins = 0;
        while (mCompleted.load(std::memory_order_acquire) != mThreadCount)
        {
            if (spins++ < kSpinLimit)
            {
                std::this_thread::yield();
            }
            else
            {
                std::unique_lock<std::mutex> lock(mMutex);
                mCv.wait_for(lock, std::chrono::microseconds(50), [this] {
                    return mCompleted.load(std::memory_order_acquire) == mThreadCount;
                });
            }
        }
    }

private:
    void workerLoop(int index)
    {
        std::uint64_t seen = 0;
        for (;;)
        {
            std::uint64_t gen;
            int spins = 0;
            while ((gen = mGeneration.load(std::memory_order_acquire)) == seen)
            {
                if (mStop.load(std::memory_order_relaxed))
                {
                    signalExited();
                    return;
                }
                if (spins++ < kSpinLimit)
                {
                    std::this_thread::yield();
                }
                else
                {
                    std::unique_lock<std::mutex> lock(mMutex);
                    mCv.wait_for(lock, std::chrono::microseconds(50), [this, seen] {
                        return mGeneration.load(std::memory_order_acquire) != seen
                            || mStop.load(std::memory_order_relaxed);
                    });
                }
            }
            seen = gen;
            if (mStop.load(std::memory_order_relaxed))
            {
                signalExited();
                return;
            }

            (*mPhaseFn)(index);

            if (mCompleted.fetch_add(1, std::memory_order_acq_rel) + 1 == mThreadCount)
            {
                mCv.notify_all();
            }
        }
    }

    // Called by a worker right before it returns from workerLoop(), so the
    // destructor knows it's safe to detach (see ~CyclePhaseBarrierPool()).
    void signalExited()
    {
        if (mExited.fetch_add(1, std::memory_order_acq_rel) + 1 == mThreadCount)
        {
            mCv.notify_all();
        }
    }

    static constexpr int kSpinLimit = 4000;

    const int mThreadCount;
    std::vector<std::thread> mThreads;

    std::mutex mMutex;
    std::condition_variable mCv;

    std::atomic<std::uint64_t> mGeneration{0};
    std::atomic<int> mCompleted{0};
    std::atomic<int> mExited{0};
    std::atomic<bool> mStop{false};
    const std::function<void(int)> * mPhaseFn = nullptr;
};

namespace CycleConcurrent {

// One persistent pool per calling thread, built lazily the first time that
// thread calls blockingMap() and torn down when the thread exits.
inline CyclePhaseBarrierPool & threadLocalPool()
{
    thread_local CyclePhaseBarrierPool pool(QThread::idealThreadCount());
    return pool;
}

// Applies fn(item) to every item of `sequence` across the calling thread's
// persistent worker pool and blocks until all of them are done - same
// contract as QtConcurrent::blockingMap(sequence, fn). `sequence` must
// support size() and operator[]; both QVector and QList qualify.
template <typename Sequence, typename MapFunction>
void blockingMap(Sequence & sequence, MapFunction fn)
{
    const int n = int(sequence.size());
    if (n == 0)
    {
        return;
    }

    CyclePhaseBarrierPool & pool = threadLocalPool();
    const int activeThreads = qMax(1, qMin(n, pool.threadCount()));

    pool.runPhase([&sequence, &fn, n, activeThreads](int index) {
        if (index >= activeThreads)
        {
            return;
        }
        const int begin = index * n / activeThreads;
        const int end = (index + 1) * n / activeThreads;
        for (int i = begin; i < end; ++i)
        {
            fn(sequence[i]);
        }
    });
}

} // namespace CycleConcurrent

#endif // CYCLECONCURRENT_H
