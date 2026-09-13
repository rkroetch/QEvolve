# QEvolve → Roguelike: Redesign Plan & Agent Team

*Prepared for the QEvolve project. Grounded in the current codebase at [github.com/rkroetch/QEvolve](https://github.com/rkroetch/QEvolve) (commit `8540bb0`).*

## 1. What QEvolve is today

QEvolve is a Qt/C++ port of the DOS freeware "Evolve!" — an artificial-life sandbox, not a game. There's no goal, no failure state, and nothing the player manages over time. Understanding its actual mechanics matters because the roguelike redesign should grow out of them rather than bolt a game on top:

- **World**: `Laboratory` is a 512×256 toroidal grid, rendered via OpenGL, advanced by a 20ms timer (`mAdvanceTimer`) that drives a background `CalculationThread`. Speed is adjustable; the sim can be started, stopped, and reset.
- **Species** (`Species`, `SpeciesUserData`) are either `typePlant` or `typeAnimal` and carry a small genome:
  - a 3×3 **movement table** (`Movements`) keyed by (friend count, enemy count) in the surrounding 8 cells → one of 16 actions (8 directions, stop, random, turn left/right, u-turn, merge, split, go). This is the "AI" and it's fully player-editable today via `EditSpeciesDialog`.
  - `mMetabolism`, `mSpawningEnergy`, and independent **mutation rates** for movement/metabolism/spawning-energy, applied when offspring are born.
  - a color and name, saved/loaded as `.SPC`/`.spe` files. The repo ships classic "Evolve!" archetypes (`RAPTORS`, `MCCOY`, `HATFIELD`, `CRUISER`, `RABBITS`, `CHICKEN`, `SLUGS`, `PROTPLSM`, `WILDPLSM`, `SALT`, `BRACHIO`) — these read as predator/prey/scavenger archetypes and are a ready-made bestiary.
- **Animals** (`Animal`) are individuals: position, energy, and a `Statistics` block (age, generation, children, mutation counts, friends/enemies seen). Each tick: `calculateMovement()` looks up the genome move for the local friend/enemy count, eats the weakest plant or kills the weakest enemy in range (`eatWeakestPlant`/`killWeakestEnemy`) for energy; `executeMovement()` spends energy equal to `BASE_METABOLISM + distance moved`, and crossing the spawning-energy threshold triggers `spawnSelf()` (mutated offspring). Energy at zero (or being eaten) triggers `killSelf()`.
- **Plants** are a `Species` too, and *do* auto-regrow every tick via `Species::respawn()` up to `MAX_NUM_PLANTS` (2000) — this is a steady resource-regeneration mechanic, not a safety net. **Animal species have no such safety net: they can and do go permanently extinct.** That's an important, already-existing lose-condition primitive we can build directly on.
- **UI**: `MainWindow` hosts a species roster (`SpeciesButtonWidget`/`SpeciesButtonLayoutWidget`, toggle active/inactive), a speed slider, plant-pattern selection (one group / two groups / random), a live chart (currently plots calculations/second, not population), and `EditSpeciesDialog` for hand-editing a species' genome. `AnimalInfoDialog` inspects a clicked individual. `DeathEffects` gives a small death-flash animation.
- **Persistence**: `QSettings` is already used for app settings (window state, plant pattern) — a natural place to hang meta-progression later.
- **Tests/benchmarks**: `test/` (animal, species, common, cycleconcurrent) and `benchmarks/species_benchmark.cpp` already exist — a real asset for a balance-regression net once difficulty tuning starts.
- **Platform note**: `laboratory.h` includes `<Windows.h>` and `<gl/GL.h>` directly — the demo is currently Windows-only. Worth flagging even though it's out of scope for the redesign itself.

**The gap to "roguelike":** no run structure (start/end, win/lose), no player decisions during play beyond initial setup, no escalating threat, and nothing persists between sessions except window chrome. Everything below is designed to grow out of the systems above rather than replace them.

## 2. Target design

### 2a. Core loop (per-run)

The player's unit of play stops being "watch the sandbox forever" and becomes **a run**: pilot one player-controlled species (a "bloodline") through a hostile Laboratory that gets harder over time, until you're wiped out (extinction — already possible today) or you clear the run.

- **Structure**: a run is divided into **epochs** (e.g. every N sim-ticks, or every time the player's population crosses a threshold — either works with the existing tick counter in `CalculationThread`). Each epoch ends with a **mutation choice**: 2–3 offered changes to the player's genome — a movement-table edit, a metabolism/spawning-energy shift, or a new unlocked action — presented the way `EditSpeciesDialog` already edits these fields, just player-facing and choice-limited instead of freeform.
- **Escalation within a run**: at fixed epoch boundaries, spawn tougher rival species (reusing/tuning the existing `.SPC` archetypes — a `RAPTORS`-style pack, then a `MCCOY`, etc.), shrink `MAX_NUM_PLANTS`/`PLANT_SPAWN_ENERGY` to tighten the food supply, or trigger a scripted hazard (temporary metabolism penalty, a plant die-off). All of these are parameter changes to systems that already exist.
- **Win/lose**: lose = player species goes extinct (already a real state — no code currently prevents it for animal species). Win = survive N epochs, or hit a population/domination target, or outlast a boss-tier rival species introduced at the final epoch.
- **Player agency during a run**: mutation-choice picks at epoch boundaries, plus (optionally) a limited number of manual `EditSpeciesDialog`-style interventions (a resource-gated "hand of god" edit), so the player is never purely a spectator.

### 2b. Management loop (between runs)

A run's outcome converts into a persistent currency — call it **Evolution Points (EP)** — from a formula over `Statistics` already tracked per-animal and per-species (generations reached, peak population, epochs cleared, enemies killed). Between runs, a **hub screen** lets the player spend EP on persistent unlocks:

- Starting genome presets built from the existing `.SPC` bestiary (unlock `RAPTORS` or `CRUISER` as a selectable starting kit instead of the default).
- Permanent bonuses to starting metabolism, spawning energy, or mutation rates (all fields already on `SpeciesUserData`).
- New starting map/plant-pattern options (the existing one-group/two-group/random patterns, plus new ones) as unlockable "biomes."
- Cosmetic species colors.
- A limited "revive" or "extra mutation reroll" charge to use mid-run.

Persistence rides on `QSettings`, already wired into `MainWindow` — extending its schema is lower-risk than introducing a new save format.

### 2c. Increasing difficulty

Two curves, both built from parameters the codebase already exposes:

- **In-run curve**: per-epoch scaling of rival species count/aggression (movement-table sophistication), resource scarcity (`MAX_NUM_PLANTS`, `PLANT_SPAWN_ENERGY`), and hazard frequency.
- **Meta curve**: once a player clears a run, unlock a harder "tier" (a roguelike ascension/New Game+ analog) that raises the floor on rival aggression and lowers starting resources, gated behind EP spending or a clear-count flag.

## 3. The agent team

Four agents, kept small on purpose because the codebase is a single tightly-coupled C++/Qt project (`Species`/`Animal`/`Laboratory` already reference each other directly) — more agents would mean more merge conflicts, not more throughput. Each agent owns a slice with a clear file boundary and a explicit contract with its neighbors so they can work in parallel after Phase 0 lands.

### Agent 1 — Run-Loop Engineer
**Mandate**: turn the free-running sandbox into a bounded run: epoch/tick tracking, win/lose detection, run-start/run-end hooks.
**Owns**: `laboratory.h/.cpp`, `common.h` (new run-state enums/constants), light hooks into `species.h/.cpp` and `animal.h/.cpp` for extinction/epoch signals.
**Key deliverable**: a `RunState`/`RunResult` contract (epoch number, elapsed ticks, per-species `Statistics` snapshot, win/lose flag) that Agents 2–4 consume. This is the foundation everyone else builds on — it should land first.

### Agent 2 — Meta-Progression & Economy Engineer
**Mandate**: the between-run game: EP calculation from `RunResult`, the unlock tree, persistence, and the run-start loadout screen (choose starting species/genome preset from the `.SPC` bestiary and unlocked biomes).
**Owns**: new hub/menu UI, the `QSettings` schema extension, `.SPC` loading path for starting-kit selection.
**Depends on**: Agent 1's `RunResult`.

### Agent 3 — Difficulty & Encounter Designer
**Mandate**: the actual numbers — per-epoch scaling curves, rival species archetypes (tuning/adding `.SPC` files as "encounters"), hazard events, and the meta-tier/ascension curve. Uses `test/` and `benchmarks/species_benchmark.cpp` as a balance-regression net so tuning changes don't silently break existing behavior.
**Owns**: `species/*.SPC` content, a new `DifficultyCurve`/`EncounterTable` module, additions to `test/` and `benchmarks/`.
**Depends on**: Agent 1's epoch hook to know when to apply scaling; feeds encounter data back into Agent 1's spawn logic.

### Agent 4 — UI/UX & Game-Feel Engineer
**Mandate**: make the above visible and satisfying — the mutation-choice picker (reusing `EditSpeciesDialog`'s field-editing patterns but choice-limited), an epoch/threat HUD (replacing or augmenting the calculations/second chart with something player-facing, e.g. population + epoch + threat level), victory/defeat screens, and reusing `DeathEffects`/`MovementIcons` for feedback on kills, extinctions, and mutations.
**Owns**: `mainwindow.h/.cpp`, `editspeciesdialog.h/.cpp`, `deatheffects.h/.cpp`, `movementicons.h/.cpp`, new dialogs.
**Depends on**: Agent 1's `RunState` (what to display), Agent 2's unlock data (what to show in the hub), Agent 3's encounter data (what to telegraph as threat).

## 4. Suggested phasing

1. **Phase 0** — Agent 1 lands `RunState`/`RunResult` and the extinction-based lose condition; everything else blocks on this.
2. **Phase 1 (parallel)** — Agent 3 builds the difficulty/encounter data model against Agent 1's epoch hook; Agent 2 builds the EP formula and hub skeleton against `RunResult`.
3. **Phase 2** — Agent 4 wires the mutation-choice picker, HUD, and hub screens to the now-real data from Agents 1–3.
4. **Phase 3** — balancing pass using Agent 3's test/benchmark additions; iterate on epoch pacing and EP costs.

## 5. Open questions for you

- **Scope of "run"**: should an epoch be tick-based, population-threshold-based, or wall-clock-based? Tick-based is simplest given the existing `CalculationThread` counter.
- **Manual intervention budget**: should the player ever get a freeform `EditSpeciesDialog`-style edit mid-run, or should all changes be picker-driven for balance's sake?
- **Windows-only build** (`Windows.h`/`gl/GL.h` in `laboratory.h`): not blocking for this redesign, but worth a ticket if cross-platform ever matters.
- **Single-player only** is assumed throughout; flag if that's not the intent.

---

### Appendix: ready-to-use subagent definitions

If you want to actually run these as Claude Code subagents against the repo (rather than just as a planning framework), drop the following into `.claude/agents/` in the QEvolve repo — one file per agent, e.g. `.claude/agents/run-loop-engineer.md`:

```markdown
---
name: run-loop-engineer
description: Owns turning QEvolve's free-running simulation into a bounded roguelike run (epochs, win/lose, RunState/RunResult contract). Use for any change to laboratory.h/.cpp's run structure or extinction/epoch hooks in species.h/.cpp and animal.h/.cpp.
---
You own the QEvolve run-loop redesign: Laboratory, and the epoch/win-lose hooks into
Species and Animal. Your deliverable is a RunState/RunResult contract (epoch number,
elapsed ticks, per-species Statistics snapshot, win/lose flag) that other agents
(meta-progression, difficulty, UI) consume — treat that contract as a stable public
API once you publish it. Preserve existing sandbox behavior (free-run/edit mode)
where reasonable; don't break the existing test/ and benchmarks/ suites. Prefer
small, reviewable diffs over large rewrites given how tightly Species/Animal/
Laboratory are already coupled.
```

```markdown
---
name: meta-progression-engineer
description: Owns QEvolve's between-run game — Evolution Points, the unlock tree, QSettings-backed persistence, and the run-start loadout/hub screen. Use for hub UI, unlock logic, or save-data schema changes.
---
You own the QEvolve meta-progression layer: converting a RunResult (from the
run-loop-engineer's contract) into Evolution Points, the persistent unlock tree,
and the hub/loadout screen where players pick a starting species preset (from the
species/*.SPC bestiary) and spend EP. Extend the existing QSettings schema already
used in mainwindow.cpp rather than inventing a new save format unless you hit a
hard limitation. Coordinate with the difficulty-encounter-designer on what's
unlockable (biomes, tiers) and the ui-game-feel-engineer on what the hub needs to
display.
```

```markdown
---
name: difficulty-encounter-designer
description: Owns QEvolve's difficulty curves and enemy encounters — per-epoch scaling, rival species (.SPC) tuning, hazard events, and the meta ascension/tier curve. Use for balance numbers, encounter tables, or species/*.SPC changes.
---
You own QEvolve's difficulty design: per-epoch in-run scaling (rival species count/
aggression, MAX_NUM_PLANTS/PLANT_SPAWN_ENERGY scarcity, hazard events) and the
cross-run meta/ascension curve. Tune and add species/*.SPC archetypes as
"encounters." Every balance change should be checked against test/ and
benchmarks/species_benchmark.cpp — add regression cases there for new encounters
or curves rather than tuning by feel alone. Feed your encounter/curve data through
the run-loop-engineer's epoch hook; don't reach into Laboratory's internals directly.
```

```markdown
---
name: ui-game-feel-engineer
description: Owns QEvolve's player-facing presentation — the mutation-choice picker, epoch/threat HUD, victory/defeat screens, and hub UI polish. Use for mainwindow.*, editspeciesdialog.*, deatheffects.*, movementicons.* changes.
---
You own QEvolve's game-facing UI: the mutation-choice picker (reuse
EditSpeciesDialog's field-editing patterns but choice-limited to what the
difficulty-encounter-designer offers), an epoch/threat HUD replacing or
augmenting the calculations/second chart, victory/defeat screens, and feedback
via DeathEffects/MovementIcons on kills, extinctions, and mutations. Consume
RunState from the run-loop-engineer and unlock data from the
meta-progression-engineer as read-only inputs — don't duplicate their state.
```
```
