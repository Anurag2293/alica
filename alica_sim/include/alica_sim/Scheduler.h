#pragma once

#include "alica_sim/SimClock.h"

#include <engine/AlicaClock.h>
#include <engine/IAlicaTimer.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace alica::sim
{

class Scheduler
{
public:
    using Callback = std::function<void()>;

    explicit Scheduler(MatchTime& time)
            : _time(time)
    {
    }

    uint64_t add(Callback cb, AlicaTime period, int owner);
    void cancel(uint64_t id);
    void runUntil(AlicaTime end);
    // Runs action right after the current callback returns, or now when no callback is running.
    void defer(std::function<void()> action);

    bool dispatching() const { return _dispatching; }
    size_t size() const { return _entries.size(); }

private:
    struct Entry
    {
        std::shared_ptr<Callback> cb;
        AlicaTime period;
        int owner;
    };
    using Key = std::tuple<int64_t, int, uint64_t>;

    MatchTime& _time;
    uint64_t _nextId = 1;
    std::unordered_map<uint64_t, Entry> _entries;
    std::priority_queue<Key, std::vector<Key>, std::greater<Key>> _queue;
    std::vector<std::function<void()>> _deferred;
    bool _dispatching = false;
};

class SimTimer : public IAlicaTimer
{
public:
    SimTimer(Scheduler& scheduler, uint64_t id)
            : _scheduler(scheduler)
            , _id(id)
    {
    }
    ~SimTimer() override { _scheduler.cancel(_id); }

private:
    Scheduler& _scheduler;
    uint64_t _id;
};

class SimTimerFactory : public IAlicaTimerFactory
{
public:
    SimTimerFactory(Scheduler& scheduler, int owner, std::string agentName = "")
            : _scheduler(scheduler)
            , _owner(owner)
            , _agentName(std::move(agentName))
    {
    }
    std::unique_ptr<IAlicaTimer> createTimer(IAlicaTimer::TimerCb cb, AlicaTime period) const override;

private:
    Scheduler& _scheduler;
    int _owner;
    std::string _agentName;
};

} // namespace alica::sim
