# alica_sim: design

Status: agreed design for Sim v0. See [architecture.md](architecture.md) for where it fits.

`alica_sim` runs a match: several teams of ALICA agents in one process, against a world supplied
as a plugin, on a simulated clock. It knows nothing about football.

## Goals

- **Deterministic.** The same teams, world, world parameters and seed produce byte-identical
  replays, at any speed, on the same build.
- **Faster than real time.** Nothing waits on wall-clock time unless the runner is asked to pace
  the match.
- **One thread.** Engines, behaviours, the message bus and the world all run on the match thread
  in a fixed order.
- **World-agnostic.** The world, its perception and command types, and its replay frames belong
  to the plugin.

## How the engine is driven

The engine reads time only through `AlicaClock` (`now()`, `sleep()`) and schedules work only
through two `IAlicaTimerFactory`s: one for its main loop (`setEngineTimerFactory`) and one for
plan and behaviour `run()` calls (`setTimerFactory`). Behaviour and plan `init` and `terminate`
run inside the engine loop. `alica_sim` replaces the clock and both factories, so every engine
call happens when the match's scheduler fires a timer.

The engine runs with `stepEngine = false`. Its loop sleeps the rest of its period through the
clock, and `SimClock::sleep` does nothing. `AlicaContext::stepEngine()` is not used: it waits on
a condition variable with a wall-clock timeout.

One engine change is needed. At the end of each iteration `PlanBase::run()` waits on
`_fpEventWait` for the rest of the loop period in wall-clock time, to pick up fast-path events
(behaviours finishing early), whatever the clock. On a sim clock that wait blocks the match
thread for a full engine period per agent per iteration. A new engine option,
`Alica.SimulatedTime` (default `false`), makes the loop skip that wait and the fast-path drain
after it. The fast path only shortens the reaction to a behaviour finishing between engine
iterations; on the sim clock nothing happens between iterations except behaviour runs, and
the next iteration's normal rule-book visit sees their results (the queued events are cleared
at the start of that iteration as before). Every agent therefore reacts to a finished
behaviour at its next engine iteration.

`alica_dummy_proxy` is not used. It delivers through process-wide queues on its own thread, so
delivery order varies between runs and every engine in the process hears every other one.

## Components

Namespace `alica::sim`, package `alica_sim` (library `alica_sim`, runner `alica_sim_run`).

### SimClock

```cpp
class MatchTime {                            // one per match, owned by the Match
public:
    AlicaTime now() const;
    void set(AlicaTime t);
};

class SimClock : public alica::AlicaClock {  // one per agent, a view of the match time
public:
    explicit SimClock(const MatchTime&);
    AlicaTime now() const override;
    void sleep(const AlicaTime&) const override {}
};
```

`AlicaContext` owns its clock and `setClock<SimClock>(matchTime)` constructs it, so each agent
gets its own `SimClock` reading the shared `MatchTime`.

### Scheduler, SimTimer, SimTimerFactory

```cpp
class Scheduler {
public:
    using Callback = std::function<void()>;
    uint64_t add(Callback cb, AlicaTime period, int owner);   // first fire at the current event time
    void cancel(uint64_t id);                                 // safe from inside a callback
    void runUntil(AlicaTime end);  // fires every entry due before end, in order of
                                   // (due time, owner, creation sequence); due += period
};

class SimTimer : public alica::IAlicaTimer { /* cancels its entry on destruction */ };

class SimTimerFactory : public alica::IAlicaTimerFactory {
public:
    SimTimerFactory(Scheduler&, int owner);
    std::unique_ptr<IAlicaTimer> createTimer(TimerCb, AlicaTime period) const override;
};
```

- An entry that is due several times within one `runUntil` fires once per due time, in order.
- While a callback runs, `MatchTime` reads that entry's due time, so the engine sees exact
  event times inside a tick.
- A new timer first fires at the current event time: the due time of the callback that created
  it, or the match time outside a pass. This matches `AlicaSystemTimer`, which calls its callback
  before its first sleep, and keeps a timer created by an early event from being overtaken by
  later events of the same pass. It fires in the same pass in (due time, owner, creation
  sequence) order, and cannot fire twice at one due time because the period is positive.
- A cancelled entry never fires again, even if it was already collected for this pass.
- A period of zero is rejected.

### MessageBus, SimCommunication

```cpp
using AlicaMessage = std::variant<AllocationAuthorityInfo, AlicaEngineInfo, PlanTreeInfo,
        RoleSwitch, SyncReady, SyncTalk, SolverResult, AgentQuery, AgentAnnouncement>;

class MessageBus {               // one per team
public:
    int join(const AlicaCommunicationHandlers&);   // returns the member index
    void leave(int member);
    void post(int sender, AlicaMessage msg);       // copies into the queue
    void deliver();              // delivers everything queued so far, in send order, to every
                                 // member except the sender; messages posted during delivery
                                 // wait for the next call
};

class SimCommunication : public alica::IAlicaCommunication {
    // send* posts to the bus; startCommunication joins, stopCommunication leaves; tick() is empty
};
```

`sendRoleSwitch` goes to the bus like the rest; members ignore what the engine ignores. The bus
is the place for later delivery rules (delay, loss, reordering), which belong to the world's
rules version.

### World plugin

```cpp
struct AgentSpec { std::string team; int teamIndex; std::string name; int64_t id; int indexInTeam; YAML::Node params; };

class World {
public:
    virtual ~World() = default;
    virtual std::string name() const = 0;
    virtual std::string rulesVersion() const = 0;
    virtual void configure(const YAML::Node& params, uint64_t seed) = 0;
    virtual std::any addAgent(const AgentSpec&) = 0;    // the agent's I/O handle
    virtual void sense(AlicaTime now) = 0;               // write every agent's perception
    virtual void step(AlicaTime now, AlicaTime dt) = 0;  // apply commands, advance the world
    virtual bool finished() const = 0;
    virtual void writeHeader(JsonWriter&) const = 0;     // static description for the viewer
    virtual void writeFrame(JsonWriter&) const = 0;      // the state at the current time
    virtual void writeResult(JsonWriter&) const = 0;     // final result (score, winner)
};

extern "C" alica::sim::World* alica_sim_create_world();  // exported by a plugin library
```

- The handle returned by `addAgent` is stored in the agent's global blackboard under the key
  `io` (type `std::any`) before the engine starts. Behaviours read their perception and write
  their command through it; its type is the world's.
- Commands written during a tick are applied by the next `step`, so every agent has the same
  one-tick reaction delay.
- All randomness comes from the seed passed to `configure`.
- `JsonWriter` is a small streaming writer in `alica_sim` that writes doubles with a fixed
  format so replays are byte-stable.

### Agent

Owns one `AlicaContext` with: agent name and id; the team's plan folders as config paths; the
team's role set and master plan; `stepEngine = false`; the shared `SimClock`; a `SimTimerFactory`
for both timer factories; `SimCommunication` on the team's bus; a logger. Creators come from the
team's `TeamFactory`. The agent's engine configuration is generated, never read from a
team's folders:

- Player names match `[A-Za-z0-9_-]+` and, with ids, are unique across the match, names
  compared case-insensitively; anything else is a config error.
- The runner writes `<work>/<n>/<agent>/Alica.yaml`, where `<n>` is the agent's index in the
  match, from the `alica_sim` defaults, the match's `engine` section and the player's `local`
  section (for example `Local.DefaultRole`), always with `Alica.SimulatedTime: true`. It puts
  `<work>/<n>` first on the agent's config paths, where `AlicaContext::initConfig` looks for
  `<agent>/Alica.yaml` before anything else. Each root holds only one agent's file, so the
  recursive lookup cannot find another agent's config.
- An `Alica.yaml` in a team's plan folders is a config error.
- Engine settings are match-wide. Some are process-wide statics in the engine
  (`Alica.AllowIdling`, `Alica.AssignmentProtectionTime`), so per-team values would silently
  leak between teams; only `local` differs per player.

```cpp
class TeamFactory {
public:
    virtual ~TeamFactory() = default;
    virtual AlicaCreators creators() = 0;   // called once per agent
};
```

The runner's default `TeamFactory` uses `alica_dynamic_loading`, which loads behaviour,
condition and utility libraries by the `libraryName` in the plans from the team's library path.

### Match

```cpp
class Match {
public:
    Match(MatchConfig, std::unique_ptr<World>, std::function<std::unique_ptr<TeamFactory>(const TeamConfig&)>);
    void start();            // configure the world, create and start every agent
    void tick();             // one fixed step, see below
    bool finished() const;   // world finished or duration reached
    void stop();             // terminate every agent
};
```

`tick()`:

A tick covers the half-open interval `[t, t + dt)`. Inputs become visible at its start and no
callback runs before them, so nothing consumes an input earlier than its time:

1. `matchTime.set(t)`.
2. Every team bus: `deliver()`, in team order. Messages are stamped `t`.
3. `world.sense(t)`.
4. `scheduler.runUntil(t + dt)`: engine loops and behaviour runs due in `[t, t + dt)`, each at
   its own due time. Messages they send are delivered at the next tick's start, never earlier
   than they were sent.
5. `world.step(t, dt)`, then `t += dt` and `matchTime.set(t)`, so anything after the tick
   (recording, `stop()` and termination hooks) sees the tick's end time.
6. Record a frame if `tick % recordEvery == 0`.

Agents are created and started in config order: teams in order, players in order.

### Replay

JSON Lines, one object per line:

1. Header: `{"format": "alica-sim-replay", "version": 1, "match": <the resolved match config>, "provenance": {...}, "world": {"name", "rulesVersion", ...writeHeader}}`.
2. One line per recorded frame: `{"t": <seconds>, "tick": <n>, ...writeFrame}`.
3. Last line: `{"result": {...writeResult}, "ticks": <n>}`.

`match` is the whole match config after defaults and path resolution: world plugin and params,
seed, dt, duration, recordEvery, engine options, and every team with its role set, master plan
and players with their params. `provenance` identifies everything else the result depends on:
the `alica_sim` version and build id, and a SHA-256 of each team's compiled plan files (sorted
by path) and of every library the match loaded (the world plugin and each behaviour, condition
and utility library), keyed by team and library name, with the resolved path. Each agent's
generated `Alica.yaml` is included under `match` as `effectiveConfig`.

### Match config

```yaml
world:
  plugin: alica_football_world # library name, found on the plugin path
  params: { ... } # passed to configure
seed: 42
dt: 0.01 # seconds
duration: 300 # seconds of match time
recordEvery: 3 # ticks
engine: # match-wide engine options
  Alica.EngineFrequency: 30
teams:
  - name: Blue
    plans: teams/blue/out # compiled plan folder(s)
    libraries: [build/lib] # where behaviour libraries are found
    roleSet: RoleSet
    masterPlan: Match
    players:
      - { name: blue1, id: 1, params: { ... }, local: { DefaultRole: Striker } }
```

Relative paths are resolved against the config file's folder.

### Runner

`alica_sim_run MATCH.yaml [--out replay.jsonl] [--plugin-path DIR]... [--speed X] [--seed N]`

- `--speed 0` (default) runs as fast as possible; `--speed 1` paces ticks to wall-clock time.
- Prints the result line to stdout and exits 0 when the match completes; exits non-zero on a
  config, load or engine error.

## Tests

- Scheduler: ordering, multiple fires per pass, cancel during dispatch, create during dispatch,
  no drift over 10^6 ticks.
- MessageBus: no self-delivery, send order, messages posted during delivery wait, teams isolated.
- An engine on the sim clock: a test world and the `alica_tests` behaviours; plan transitions
  happen at the expected match times.
- Simulated time: a behaviour that succeeds in its first `run()` triggers its transition at the
  agent's next engine iteration, and a match of four agents at 30 Hz runs at least 50 times
  faster than real time.
- Timers created by an early event in a pass fire before later events of that pass, at their
  creator's due time.
- Determinism: one match run twice, and at speed 0 and 1, gives identical replays.
- Threads: the engine starts no thread during a match (checked by counting threads on Linux and
  macOS before and after start).

## Later

- Live view: the runner serves the replay frames over a local WebSocket as they are produced.
- Partial perception and message delivery rules, as world rules versions.
- Behaviours as WebAssembly with a per-tick budget.
