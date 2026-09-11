#include <gtest/gtest.h>

#include <QVector>
#include <atomic>

#include "cycleconcurrent.h"

TEST(CycleConcurrent, ThreadPoolHasAtLeastOneWorker)
{
    EXPECT_GE(CycleConcurrent::threadLocalPool().threadCount(), 1);
}

TEST(CycleConcurrent, BlockingMapAppliesFunctionToEveryElement)
{
    QVector<int> items(200);
    for (int i = 0; i < items.size(); ++i)
    {
        items[i] = i;
    }

    CycleConcurrent::blockingMap(items, [](int & value) { value *= 2; });

    for (int i = 0; i < items.size(); ++i)
    {
        EXPECT_EQ(items[i], i * 2);
    }
}

TEST(CycleConcurrent, BlockingMapVisitsEveryElementExactlyOnce)
{
    const int n = 500;
    QVector<int> items(n, 0);
    std::atomic<int> visitCount{0};

    CycleConcurrent::blockingMap(items, [&visitCount](int &) { visitCount.fetch_add(1, std::memory_order_relaxed); });

    EXPECT_EQ(visitCount.load(), n);
}

TEST(CycleConcurrent, BlockingMapHandlesEmptySequence)
{
    QVector<int> items;
    EXPECT_NO_FATAL_FAILURE(CycleConcurrent::blockingMap(items, [](int & value) { value = -1; }));
}

TEST(CycleConcurrent, BlockingMapIsUsableRepeatedlyOnTheSameThread)
{
    QVector<int> items(50, 1);
    for (int iteration = 0; iteration < 10; ++iteration)
    {
        CycleConcurrent::blockingMap(items, [](int & value) { ++value; });
    }
    for (int value : items)
    {
        EXPECT_EQ(value, 11);
    }
}
