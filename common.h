#ifndef COMMON_H
#define COMMON_H

#include <QPointF>
#include <QDebug>
#include <string.h>
#include <random>

constexpr int LABORATORY_WIDTH = 360;
constexpr int LABORATORY_HEIGHT = 360;
constexpr int MAX_NUM_PLANTS = 2000;
constexpr int PLANT_INITIAL_ENERGY = 500;
constexpr int PLANT_SPAWN_ENERGY = 1000;

inline void wrapLabWidth(int &x)
{
    if (x > LABORATORY_WIDTH - 1)
    {
        x = 1;
    }
    else if (x < 1)
    {
        x = LABORATORY_WIDTH - 1;
    }
}

inline void wrapLabHeight(int &y)
{
    if (y > LABORATORY_HEIGHT - 1)
    {
        y = 1;
    }
    else if (y < 1)
    {
        y = LABORATORY_HEIGHT - 1;
    }
}

inline int clampCellX(int x)
{
    if (x < 0)
    {
        return 0;
    }
    if (x > LABORATORY_WIDTH - 1)
    {
        return LABORATORY_WIDTH - 1;
    }
    return x;
}

inline int clampCellY(int y)
{
    if (y < 0)
    {
        return 0;
    }
    if (y > LABORATORY_HEIGHT - 1)
    {
        return LABORATORY_HEIGHT - 1;
    }
    return y;
}

inline std::mt19937 &threadRng()
{
    thread_local std::mt19937 gen{std::random_device{}()};
    return gen;
}

inline int randIntInclusive(int lo, int hi)
{
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(threadRng());
}

//GENE KEY:
//0 - up
//1 - up right
//2 - right
//3 - down right
//4 - down
//5 - down left
//6 - left
//7 - up left
//8 - stop
//9 - random
//10 - turn left
//11 - turn right
//12 - u-turn
//13 - merge
//14 - split
//15 - go
enum MovementDirections
{
    MoveUp,
    MoveUpRight,
    MoveRight,
    MoveDownRight,
    MoveDown,
    MoveDownLeft,
    MoveLeft,
    MoveUpLeft,
    MoveStop,
    MoveRandom,
    MoveTurnLeft,
    MoveTurnRight,
    MoveTurnAround,
    MoveMerge,
    MoveSplit,
    MoveGo,
    MoveMax
};

constexpr const char * movementDirectionString(MovementDirections direction)
{
    switch ( direction )
    {
    case MoveUp:
        return "MoveUp";
    case MoveUpRight:
        return "MoveUpRight";
    case MoveRight:
        return "MoveRight";
    case MoveDownRight:
        return "MoveDownRight";
    case MoveDown:
        return "MoveDown";
    case MoveDownLeft:
        return "MoveDownLeft";
    case MoveLeft:
        return "MoveLeft";
    case MoveUpLeft:
        return "MoveUpLeft";
    case MoveStop:
        return "MoveStop";
    case MoveRandom:
        return "MoveRandom";
    case MoveTurnLeft:
        return "MoveTurnLeft";
    case MoveTurnRight:
        return "MoveTurnRight";
    case MoveTurnAround:
        return "MoveTurnAround";
    case MoveMerge:
        return "MoveMerge";
    case MoveSplit:
        return "MoveSplit";
    case MoveGo:
        return "MoveGo";
    case MoveMax:
        return "MoveMax";
    }
    return "MoveUnknown";
}

class Movements
{
public:
    Movements() = default;
//    Movements & operator = (const Movements & other) = default;

    void dump() const
    {
        qWarning() << movementDirectionString(mMovements[0][0]) << movementDirectionString(mMovements[1][0]) << movementDirectionString(mMovements[2][0]);
        qWarning() << movementDirectionString(mMovements[0][1]) << movementDirectionString(mMovements[1][1]) << movementDirectionString(mMovements[2][1]);
        qWarning() << movementDirectionString(mMovements[0][2]) << movementDirectionString(mMovements[1][2]) << movementDirectionString(mMovements[2][2]);
    }

    constexpr void setMovement(int friends, int enemies, MovementDirections direction)
    {
        if ( friends < 0 )
        {
            friends = 0;
        }
        if ( friends > 2)
        {
            friends = 2;
        }
        if ( enemies < 0)
        {
            enemies = 0;
        }
        if ( enemies > 2)
        {
            enemies = 2;
        }

        mMovements[friends][enemies] = direction;
    }

    constexpr void toggleMovement(int friends, int enemies)
    {
        if ( friends < 0 )
        {
            friends = 0;
        }
        if ( friends > 2)
        {
            friends = 2;
        }
        if ( enemies < 0)
        {
            enemies = 0;
        }
        if ( enemies > 2)
        {
            enemies = 2;
        }

        mMovements[friends][enemies] = MovementDirections((mMovements[friends][enemies] + 1) % MoveMax);
    }

    constexpr MovementDirections getMovement(int friends, int enemies) const
    {
        if ( friends < 0 )
        {
            friends = 0;
        }
        if ( friends > 2)
        {
            friends = 2;
        }
        if ( enemies < 0)
        {
            enemies = 0;
        }
        if ( enemies > 2)
        {
            enemies = 2;
        }

        return mMovements[friends][enemies];
    }

private:
    MovementDirections mMovements[3][3] = {{MoveStop, MoveStop, MoveStop}, {MoveStop, MoveStop, MoveStop}, {MoveStop, MoveStop, MoveStop}};
};

#endif // COMMON_H
