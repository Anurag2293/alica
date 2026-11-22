# Roadmap

This repository is the library for a hosted ALICA platform: invited players write plans for a
team, run matches against other teams and watch them. `alica-football` is the first
application built on it. See [design/architecture.md](design/architecture.md).

| #   | Step                          | Scope                                                                                                                      | Status |
| --- | ----------------------------- | -------------------------------------------------------------------------------------------------------------------------- | ------ |
| 0   | Fork cleanup and CMake build  | Plain CMake build and tests on macOS and Ubuntu, CI for both                                                               | Done   |
| 1   | Plan tooling v0               | `alicaplan`: YAML to engine JSON, checker, decompiler, engine load test ([alicaplan.md](alicaplan.md))                     | Done   |
| 2   | Sim v0                        | `alica_sim` kernel and runner, replays ([design](design/sim.md)); football world plugin and demo teams in `alica-football` | Next   |
| 3   | Platform skeleton             | GCP, Convex, Clerk and Vercel projects; TanStack Start app with invite-only Google sign-in                                 |        |
| 4   | Behaviour library, conditions | Football behaviours players' plans use; conditions as expressions over blackboard keys                                     |        |
| 5   | Match viewer                  | Viewer frame package and football renderer; replays play in the browser                                                    |        |
| 6   | Hosted matches                | Compile service and match worker on Cloud Run; match queue, results and replays in Convex                                  |        |
| 7   | Plan designer D0 to D2        | Viewer, data flow and key tracing, then editing in the browser ([design](design/plan-designer.md))                         |        |
| 8   | Perception and message rules  | Partial perception and teammate message delivery rules, as world rules versions                                            |        |
| 9   | Tournaments and stats         | Batch matches, rankings, leaderboard                                                                                       |        |
| 10  | Live plan view (designer D3)  | Each robot's plan state and task allocation in the viewer                                                                  |        |

`alica-football` may later add an rcssserver adapter as its own plugin.
