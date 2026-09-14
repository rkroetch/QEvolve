#ifndef MUTATIONCHOICE_H
#define MUTATIONCHOICE_H

#include <QString>
#include <QVector>
#include <QtGlobal>

#include "common.h" // Movements, MovementDirections

// --- Phase 2 (UI/Game-Feel Engineer) ----------------------------------------
// Pure, dependency-free generation of the 2-3 limited mutation choices
// offered to the player at each epoch boundary (see the redesign plan,
// section 2a, and MutationChoiceDialog). Mirrors runstate.h/
// difficultycurve.h's style: plain structs and free functions, no Species/
// Animal/Qt-widget dependency, so the picking logic is unit-testable in
// isolation (see test/mutationchoice_test.cpp). MutationChoiceDialog is the
// only thing that renders a QVector<MutationChoice>, and
// MainWindow::onEpochAdvanced() is the only thing that turns a chosen
// MutationChoice into actual Species::set*() calls.

enum class MutationChoiceKind
{
    MovementTweak,
    MetabolismShift,
    SpawningEnergyShift
};

// One offered choice. Only the fields relevant to `kind` are meaningful;
// the rest stay at their default.
struct MutationChoice
{
    MutationChoiceKind kind = MutationChoiceKind::MetabolismShift;
    QString title;
    QString description;

    // MovementTweak only: which (friends, enemies) cell of the 3x3 action
    // table to change, and the new direction to change it to.
    int moveFriends = 0;
    int moveEnemies = 0;
    MovementDirections moveDirection = MoveStop;

    // MetabolismShift / SpawningEnergyShift only: fractional change to
    // apply, e.g. -0.12 == a 12% reduction. Always negative (a reduction),
    // mirroring the buff direction used for MetaProgression's permanent
    // stat bonuses (see MainWindow::applyMetaBonuses()): lower metabolism
    // means less energy spent per move, lower spawning energy means
    // reproducing sooner.
    double statDeltaFraction = 0.0;
};

// Current genome snapshot the picker draws choices against - deliberately
// just the three mutable fields relevant to mutation choices, not a full
// Species/SpeciesUserData, so this stays independent of species.h.
struct MutationChoiceContext
{
    Movements movements;
    int metabolism = 100;
    int spawningEnergy = 1000;
};

// Draws `count` choices (clamped to [1, 3]), one of each kind when count
// == 3, deterministically for a given `seed` (same context + count + seed
// always produces the same result) so this is unit-testable without a live
// RNG. Callers should pass a fresh, unpredictable seed per call (e.g. from
// QRandomGenerator) to get an actually-random pick in the running app.
QVector<MutationChoice> generateMutationChoices(const MutationChoiceContext & context, int count, quint32 seed);

#endif // MUTATIONCHOICE_H
