#include "alica_sim/Scheduler.h"
#include "alica_sim/SimLogger.h"

#include <stdexcept>

namespace alica::sim
{

uint64_t Scheduler::add(Callback cb, AlicaTime period, int owner)
{
    if (!cb) {
        throw std::invalid_argument("Scheduler::add: empty callback");
    }
    if (period <= AlicaTime::zero()) {
        throw std::invalid_argument("Scheduler::add: the period must be positive");
    }
    uint64_t id = _nextId++;
    _entries.emplace(id, Entry{std::make_shared<Callback>(std::move(cb)), period, owner});
    _queue.emplace(_time.now().inNanoseconds(), owner, id);
    return id;
}

void Scheduler::cancel(uint64_t id)
{
    _entries.erase(id);
}

void Scheduler::runUntil(AlicaTime end)
{
    while (!_halted && !_queue.empty() && std::get<0>(_queue.top()) < end.inNanoseconds()) {
        auto [due, owner, id] = _queue.top();
        _queue.pop();
        auto it = _entries.find(id);
        if (it == _entries.end()) {
            continue;
        }
        std::shared_ptr<Callback> cb = it->second.cb;
        int64_t next = due + it->second.period.inNanoseconds();
        _time.set(AlicaTime::nanoseconds(due));
        _dispatching = true;
        try {
            (*cb)();
        } catch (...) {
            _dispatching = false;
            _deferred.clear();
            throw;
        }
        _dispatching = false;
        while (!_deferred.empty()) {
            std::vector<std::function<void()>> actions;
            actions.swap(_deferred);
            for (auto& action : actions) {
                action();
            }
        }
        if (_entries.count(id)) {
            _queue.emplace(next, owner, id);
        }
    }
}

void Scheduler::defer(std::function<void()> action)
{
    if (_dispatching) {
        _deferred.push_back(std::move(action));
    } else {
        action();
    }
}

std::unique_ptr<IAlicaTimer> SimTimerFactory::createTimer(IAlicaTimer::TimerCb cb, AlicaTime period) const
{
    auto named = [cb = std::move(cb), name = _agentName]() {
        SimLogger::AgentScope scope(name);
        cb();
    };
    return std::make_unique<SimTimer>(_scheduler, _scheduler.add(std::move(named), period, _owner));
}

} // namespace alica::sim
