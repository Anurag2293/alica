# Roadmap

The fork is used to build a small football team with ALICA: tooling to write plans, a simulator
to play matches, and views to see what the agents are doing. Everything lives in this repo for
now.

| #   | Step                          | Scope                                                                                                     | Status |
| --- | ----------------------------- | --------------------------------------------------------------------------------------------------------- | ------ |
| 0   | Fork cleanup and CMake build  | Plain CMake build and tests on macOS and Ubuntu, CI for both                                              | Done   |
| 1   | Plan tooling v0               | `alicaplan`: YAML to engine JSON, checker, decompiler, engine load test ([alicaplan.md](alicaplan.md))    | Done   |
| 2   | Sim v0                        | One process, 2v2 or 3v3, dummy proxy, fixed time step, WebSocket browser view                             | Next   |
| 3   | Plan viewer (designer D0)     | Read-only hierarchy, plan canvas, inspector and problems, live reload ([design](design/plan-designer.md)) |        |
| 4   | Football plans, with D1       | Passes, keeper, set pieces, formations; the designer's data flow view and key tracing are built alongside |        |
| 5   | Plan editing (designer D2)    | Editing in the designer, every edit checked                                                               |        |
| 6   | Batch runner and stats        | Run many matches headless and collect results                                                             |        |
| 7   | UDP proxy and fault injector  | Agents talk over real sockets, with dropped, delayed and reordered messages                               |        |
| 8   | Live plan and allocation view | Designer D3: the plan canvas attached to running engines, showing each agent's state and task             |        |
| 9   | rcssserver adapter            | Players connect to the RoboCup soccer server through the same interface as the sim                        |        |

## Order

- The viewer comes before the football plans because those are the first plans too big to follow
  as files. It does not depend on the sim.
- Key tracing (D1) is built while the football plans' blackboards and key mappings are written,
  so it is tested on real plans.
- Editing (D2) waits until real plans show which edits matter; until then agents write the YAML.
- The live view is the designer's canvas attached to running engines, not a separate tool.

## Sim v0: open decisions

- Write the sim in C++ next to the engine, as one deterministic process. The alternative is a
  Python or TypeScript sim talking to C++ players over sockets.
- Drive the engines from the sim's clock, not wall-clock time, so matches repeat exactly and can
  run faster than real time. The engine supports step mode and custom clock and timer factories.
- Keep the sim behind an interface so the rcssserver adapter can plug in later.
