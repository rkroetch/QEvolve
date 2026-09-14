#include "mutationchoice.h"

#include <algorithm>
#include <array>
#include <random>

namespace {

QString directionName(MovementDirections direction)
{
    // Kept as a local, string-only copy of movementicons.h's tooltip text
    // (rather than depending on it) so this file has no Qt-widget
    // dependency and stays trivially unit-testable.
    switch (direction)
    {
    case MoveUp: return QStringLiteral("Up");
    case MoveUpRight: return QStringLiteral("Up-Right");
    case MoveRight: return QStringLiteral("Right");
    case MoveDownRight: return QStringLiteral("Down-Right");
    case MoveDown: return QStringLiteral("Down");
    case MoveDownLeft: return QStringLiteral("Down-Left");
    case MoveLeft: return QStringLiteral("Left");
    case MoveUpLeft: return QStringLiteral("Up-Left");
    case MoveRandom: return QStringLiteral("Random");
    case MoveStop: return QStringLiteral("Stop");
    case MoveGo: return QStringLiteral("Go");
    case MoveTurnAround: return QStringLiteral("U-Turn");
    case MoveTurnRight: return QStringLiteral("Turn Right");
    case MoveTurnLeft: return QStringLiteral("Turn Left");
    case MoveMerge: return QStringLiteral("Merge");
    case MoveSplit: return QStringLiteral("Split");
    default: return QStringLiteral("Unknown");
    }
}

QString countLabel(int count)
{
    return count >= 2 ? QStringLiteral("2+") : QString::number(count);
}

MutationChoice makeMovementTweak(const MutationChoiceContext & context, std::mt19937 & rng)
{
    std::uniform_int_distribution<int> cellDist(0, 2);
    const int friends = cellDist(rng);
    const int enemies = cellDist(rng);
    const MovementDirections current = context.movements.getMovement(friends, enemies);

    std::uniform_int_distribution<int> dirDist(0, int(MoveMax) - 1);
    MovementDirections next = current;
    // A handful of retries is enough to almost always land on a different
    // direction (1-in-16 odds per try of colliding with `current`); if
    // every retry collides anyway, offering the same direction back is a
    // harmless no-op choice rather than a bug.
    for (int attempt = 0; attempt < 8 && next == current; ++attempt)
    {
        next = MovementDirections(dirDist(rng));
    }

    MutationChoice choice;
    choice.kind = MutationChoiceKind::MovementTweak;
    choice.moveFriends = friends;
    choice.moveEnemies = enemies;
    choice.moveDirection = next;
    choice.title = QStringLiteral("Instinct Shift");
    choice.description = QStringLiteral("With %1 friend(s) and %2 foe(s) nearby, switch action to %3.")
        .arg(countLabel(friends), countLabel(enemies), directionName(next));
    return choice;
}

MutationChoice makeMetabolismShift(std::mt19937 & rng)
{
    std::uniform_int_distribution<int> pctDist(8, 20);
    const int percent = pctDist(rng);

    MutationChoice choice;
    choice.kind = MutationChoiceKind::MetabolismShift;
    choice.statDeltaFraction = -(percent / 100.0);
    choice.title = QStringLiteral("Leaner Metabolism");
    choice.description = QStringLiteral("Reduce metabolism by %1%% - spend less energy on every move.").arg(percent);
    return choice;
}

MutationChoice makeSpawningEnergyShift(std::mt19937 & rng)
{
    std::uniform_int_distribution<int> pctDist(8, 20);
    const int percent = pctDist(rng);

    MutationChoice choice;
    choice.kind = MutationChoiceKind::SpawningEnergyShift;
    choice.statDeltaFraction = -(percent / 100.0);
    choice.title = QStringLiteral("Efficient Reproduction");
    choice.description = QStringLiteral("Reduce the spawning-energy threshold by %1%% - reproduce sooner.").arg(percent);
    return choice;
}

} // namespace

QVector<MutationChoice> generateMutationChoices(const MutationChoiceContext & context, int count, quint32 seed)
{
    count = qBound(1, count, 3);

    std::mt19937 rng(seed);

    std::array<MutationChoiceKind, 3> kinds = {
        MutationChoiceKind::MovementTweak,
        MutationChoiceKind::MetabolismShift,
        MutationChoiceKind::SpawningEnergyShift
    };
    std::shuffle(kinds.begin(), kinds.end(), rng);

    QVector<MutationChoice> choices;
    choices.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        switch (kinds[std::size_t(i)])
        {
        case MutationChoiceKind::MovementTweak:
            choices.append(makeMovementTweak(context, rng));
            break;
        case MutationChoiceKind::MetabolismShift:
            choices.append(makeMetabolismShift(rng));
            break;
        case MutationChoiceKind::SpawningEnergyShift:
            choices.append(makeSpawningEnergyShift(rng));
            break;
        }
    }
    return choices;
}
