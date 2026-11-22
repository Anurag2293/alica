#include <alica_sim/Scheduler.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace alica::sim
{
namespace
{

using Log = std::vector<std::pair<std::string, int64_t>>;

AlicaTime ms(int64_t n)
{
    return AlicaTime::milliseconds(n);
}

class SchedulerTest : public ::testing::Test
{
protected:
    Scheduler::Callback record(const std::string& name)
    {
        return [this, name]() { log.emplace_back(name, time.now().inNanoseconds()); };
    }

    MatchTime time;
    Scheduler scheduler{time};
    Log log;
};

TEST_F(SchedulerTest, OrdersByDueTimeThenOwnerThenCreation)
{
    scheduler.add(record("a"), ms(10), 2);
    scheduler.add(record("b"), ms(10), 1);
    scheduler.add(record("c"), ms(10), 1);
    time.set(ms(5));
    scheduler.add(record("d"), ms(10), 0);
    scheduler.runUntil(ms(10));
    EXPECT_EQ(log, (Log{{"b", 0}, {"c", 0}, {"a", 0}, {"d", ms(5).inNanoseconds()}}));
}

TEST_F(SchedulerTest, FirstFireIsAtTheCurrentMatchTime)
{
    time.set(ms(7));
    scheduler.add(record("a"), ms(100), 0);
    scheduler.runUntil(ms(7));
    EXPECT_TRUE(log.empty());
    scheduler.runUntil(ms(8));
    EXPECT_EQ(log, (Log{{"a", ms(7).inNanoseconds()}}));
}

TEST_F(SchedulerTest, FiresOncePerDueTimeWithinOnePass)
{
    scheduler.add(record("a"), ms(3), 0);
    scheduler.runUntil(ms(10));
    EXPECT_EQ(log, (Log{{"a", 0}, {"a", ms(3).inNanoseconds()}, {"a", ms(6).inNanoseconds()}, {"a", ms(9).inNanoseconds()}}));
    log.clear();
    scheduler.runUntil(ms(12));
    EXPECT_TRUE(log.empty());
    scheduler.runUntil(ms(13));
    EXPECT_EQ(log, (Log{{"a", ms(12).inNanoseconds()}}));
}

TEST_F(SchedulerTest, EndIsExclusive)
{
    scheduler.add(record("a"), ms(10), 0);
    scheduler.runUntil(ms(10));
    scheduler.runUntil(ms(10));
    EXPECT_EQ(log.size(), 1u);
    scheduler.runUntil(ms(10) + AlicaTime::nanoseconds(1));
    EXPECT_EQ(log.size(), 2u);
}

TEST_F(SchedulerTest, CancelDuringDispatch)
{
    uint64_t b = 0;
    scheduler.add(
            [&]() {
                log.emplace_back("a", time.now().inNanoseconds());
                scheduler.cancel(b);
            },
            ms(10), 0);
    b = scheduler.add(record("b"), ms(10), 1);
    uint64_t self = 0;
    self = scheduler.add(
            [&]() {
                log.emplace_back("self", time.now().inNanoseconds());
                scheduler.cancel(self);
            },
            ms(1), 2);
    scheduler.runUntil(ms(50));
    EXPECT_EQ(log, (Log{{"a", 0}, {"self", 0}, {"a", ms(10).inNanoseconds()}, {"a", ms(20).inNanoseconds()}, {"a", ms(30).inNanoseconds()},
                           {"a", ms(40).inNanoseconds()}}));
    EXPECT_EQ(scheduler.size(), 1u);
}

TEST_F(SchedulerTest, TimersCreatedByAnEarlyEventFireBeforeLaterEventsAtTheirCreatorsDueTime)
{
    time.set(ms(2));
    bool created = false;
    std::unique_ptr<IAlicaTimer> child;
    SimTimerFactory factory(scheduler, 5);
    scheduler.add(
            [&]() {
                log.emplace_back("early", time.now().inNanoseconds());
                if (!created) {
                    created = true;
                    child = factory.createTimer(record("child"), ms(4));
                }
            },
            ms(100), 0);
    time.set(ms(7));
    scheduler.add(record("late"), ms(100), 0);
    time.set(ms(0));
    scheduler.runUntil(ms(10));
    EXPECT_EQ(
            log, (Log{{"early", ms(2).inNanoseconds()}, {"child", ms(2).inNanoseconds()}, {"child", ms(6).inNanoseconds()}, {"late", ms(7).inNanoseconds()}}));
}

TEST_F(SchedulerTest, DestroyingATimerCancelsIt)
{
    SimTimerFactory factory(scheduler, 0);
    auto timer = factory.createTimer(record("a"), ms(5));
    scheduler.runUntil(ms(1));
    timer.reset();
    scheduler.runUntil(ms(100));
    EXPECT_EQ(log.size(), 1u);
    EXPECT_EQ(scheduler.size(), 0u);
}

TEST_F(SchedulerTest, DeferredActionsRunAfterTheCallbackReturns)
{
    scheduler.add(
            [&]() {
                scheduler.defer([&]() {
                    log.emplace_back("deferred", time.now().inNanoseconds());
                    EXPECT_FALSE(scheduler.dispatching());
                });
                log.emplace_back("a", time.now().inNanoseconds());
            },
            ms(10), 0);
    scheduler.add(record("b"), ms(10), 1);
    scheduler.runUntil(ms(1));
    EXPECT_EQ(log, (Log{{"a", 0}, {"deferred", 0}, {"b", 0}}));
    log.clear();
    scheduler.defer(record("now"));
    EXPECT_EQ(log, (Log{{"now", 0}}));
}

TEST_F(SchedulerTest, RejectsAZeroPeriod)
{
    EXPECT_THROW(scheduler.add(record("a"), AlicaTime::zero(), 0), std::invalid_argument);
}

TEST_F(SchedulerTest, NoDriftOverAMillionTicks)
{
    const AlicaTime period = AlicaTime::nanoseconds(33333333);
    const AlicaTime dt = ms(10);
    int64_t fires = 0;
    int64_t last = -1;
    scheduler.add(
            [&]() {
                ++fires;
                last = time.now().inNanoseconds();
            },
            period, 0);
    for (int64_t tick = 0; tick < 1000000; ++tick) {
        time.set(dt * tick);
        scheduler.runUntil(dt * (tick + 1));
    }
    const int64_t end = (dt * 1000000).inNanoseconds();
    const int64_t expected = (end + period.inNanoseconds() - 1) / period.inNanoseconds();
    EXPECT_EQ(fires, expected);
    EXPECT_EQ(last, (expected - 1) * period.inNanoseconds());
}

} // namespace
} // namespace alica::sim
