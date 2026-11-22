#pragma once

#include <engine/AlicaClock.h>

namespace alica::sim
{

class MatchTime
{
public:
    AlicaTime now() const { return _now; }
    void set(AlicaTime t) { _now = t; }

private:
    AlicaTime _now;
};

class SimClock : public AlicaClock
{
public:
    explicit SimClock(const MatchTime& time)
            : _time(time)
    {
    }
    AlicaTime now() const override { return _time.now(); }
    void sleep(const AlicaTime&) const override {}

private:
    const MatchTime& _time;
};

} // namespace alica::sim
