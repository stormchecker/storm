#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/storage/Scheduler.h"

TEST(SchedulerTest, TotalDeterministicMemorylessScheduler) {
    storm::storage::Scheduler<double> scheduler(4);

    ASSERT_NO_THROW(scheduler.setChoice(1, 0));
    ASSERT_NO_THROW(scheduler.setChoice(3, 1));
    ASSERT_NO_THROW(scheduler.setChoice(5, 2));
    ASSERT_NO_THROW(scheduler.setChoice(4, 3));

    ASSERT_FALSE(scheduler.isPartialScheduler());
    ASSERT_TRUE(scheduler.isMemorylessScheduler());
    ASSERT_TRUE(scheduler.isDeterministicScheduler());

    ASSERT_TRUE(scheduler.getChoice(0).isDefined());
    ASSERT_EQ(1ul, scheduler.getChoice(0).getDeterministicChoice());

    ASSERT_TRUE(scheduler.getChoice(1).isDefined());
    ASSERT_EQ(3ul, scheduler.getChoice(1).getDeterministicChoice());

    ASSERT_TRUE(scheduler.getChoice(2).isDefined());
    ASSERT_EQ(5ul, scheduler.getChoice(2).getDeterministicChoice());

    ASSERT_TRUE(scheduler.getChoice(3).isDefined());
    ASSERT_EQ(4ul, scheduler.getChoice(3).getDeterministicChoice());
}

TEST(SchedulerTest, PartialDeterministicMemorylessScheduler) {
    storm::storage::Scheduler<double> scheduler(4);

    ASSERT_NO_THROW(scheduler.setChoice(1, 0));
    ASSERT_NO_THROW(scheduler.setChoice(3, 0));
    ASSERT_NO_THROW(scheduler.setChoice(5, 2));
    ASSERT_NO_THROW(scheduler.setChoice(4, 3));
    ASSERT_NO_THROW(scheduler.clearChoice(2));

    ASSERT_TRUE(scheduler.isPartialScheduler());
    ASSERT_TRUE(scheduler.isMemorylessScheduler());
    ASSERT_TRUE(scheduler.isDeterministicScheduler());

    ASSERT_TRUE(scheduler.getChoice(0).isDefined());
    ASSERT_EQ(3ul, scheduler.getChoice(0).getDeterministicChoice());

    ASSERT_TRUE(scheduler.getChoice(3).isDefined());
    ASSERT_EQ(4ul, scheduler.getChoice(3).getDeterministicChoice());

    ASSERT_FALSE(scheduler.getChoice(1).isDefined());
    ASSERT_FALSE(scheduler.getChoice(2).isDefined());
}
