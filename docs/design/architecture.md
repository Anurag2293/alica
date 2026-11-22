# Platform architecture

Status: agreed 2026-11-23. The sim is the real thing: there are no physical robots, so the
simulated world, its rules and its timing are the product.

The goal is a hosted platform where invited players write ALICA plans for a team, run matches
against other teams, and watch them. This repository is the library: every layer needed to build
such a platform. An application, such as `alica-football`, supplies the game: its world, its
behaviours, its viewer and its deployment.

## Layers

| Layer                    | In this repository                                                                                                                                                     | In an application (`alica-football`)                                                       |
| ------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------ |
| Distributed intelligence | The engine; `alicaplan` (YAML plans, checker); `alica_sim` (deterministic match kernel, team message bus, world plugin interface, replay format, match runner)         | The world plugin (physics, rules, perception and commands) and the behaviour library       |
| Backend                  | A Convex component for the platform: teams, plan versions, matches, results, replays; the match worker and the plan compile service, as container images for Cloud Run | The Convex app that installs the component, plus deployment config                         |
| Frontend                 | npm packages: app shell, designer, match viewer frame, team and match pages, leaderboard                                                                               | A TanStack Start app built from the packages, with the game's renderer, deployed on Vercel |

Applications integrate at build time: they install the packages and the Convex component and
build their own site. Nothing is loaded at runtime from another origin.

## Stack

| Part               | Choice                                                                                                     |
| ------------------ | ---------------------------------------------------------------------------------------------------------- |
| Frontend           | TanStack Start (React), hosted on Vercel                                                                   |
| Auth               | Clerk with Google sign-in; sign-up mode Restricted, players join by invitation; Convex verifies Clerk JWTs |
| App backend        | Convex: database, reactive queries over its WebSocket, file storage for replays, scheduled functions       |
| Plan check/compile | `alicaplan` behind a small HTTP service on Cloud Run (scales to zero), called from a Convex action         |
| Match workers      | Cloud Run jobs running the `alica_sim` runner; started by a Convex action                                  |
| Container builds   | Cloud Build into Artifact Registry                                                                         |

## Match flow

1. A player edits plans in the designer. Each save is a new plan version, stored in Convex as
   YAML text. A Convex action sends it to the compile service, which returns the checker
   findings and the compiled JSON; the version is playable only if it has no errors.
2. A match request (two team versions, a world version, a seed) goes into the match queue in
   Convex. An action starts a Cloud Run job execution with the match id.
3. The worker reads the match and both teams' compiled plans from Convex, runs the match headless
   and faster than real time, uploads the replay to Convex file storage, and writes the result.
4. Every browser watching the match list, the queue or the leaderboard updates through Convex's
   reactive queries. A viewer downloads the replay and plays it at match speed.

Matches are deterministic: the same team versions, world version and seed give the same replay.
A replay names everything it was made from, so any result can be reproduced.

Hosted matches are watched as replays, not streamed while they compute: a five-minute match
takes seconds to compute, and streaming 30 frames a second through Convex would cost one function
call per frame. Live streaming is used only locally, by the runner, for debugging.

## Players' code

Players write plans only: plan trees, roles and priorities, blackboards, key mappings and
parameters. Behaviours come from the application's behaviour library, and conditions are
expressions over blackboard keys evaluated by a generic condition creator. No player code runs
on the servers. Behaviours compiled to WebAssembly and run inside the sim with a per-tick budget
are a later option that does not change this design.

## Environments

| Resource          | Name                                                                         |
| ----------------- | ---------------------------------------------------------------------------- |
| GCP project       | `alica-platform` (billing linked; Cloud Run, Artifact Registry, Cloud Build) |
| Convex project    | `alica-football`                                                             |
| Clerk application | `alica-football`                                                             |
| Vercel project    | `alica-football`                                                             |

## Open

- Perception and communication rules: whether each robot sees the whole pitch, and how messages
  between teammates are delayed or lost. This is a world and bus rule, versioned with the world;
  Sim v0 starts with full view and perfect messages.
