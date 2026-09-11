#include "animal.h"
#include "species.h"
#include "deatheffects.h"

Animal::Animal(QPointF pos, Species * species, double energy, int spawningEnergy, int metabolism, const Movements & movements)
{
    const QPointF direction(randIntInclusive(-1, 1), randIntInclusive(-1, 1));
    initialize(pos, species, energy, spawningEnergy, metabolism, movements, direction, nullptr);
}

void Animal::initialize(QPointF pos, Species * species, double energy, int spawningEnergy, int metabolism, const Movements & movements, QPointF direction, const Animal * parent)
{
    if ( parent )
    {
        mStatistics = parent->mStatistics;
        ++mStatistics.mGeneration;
        mStatistics.mNumChildren = 0;
        mStatistics.mAge = 0;
    }
    else
    {
        mStatistics = Statistics{};
    }
    mSpecies = species;

    mPos = pos;
    syncCellFromPos();

    mColor = species->color();
    mEnergy = energy;
    mStatistics.mEnergy = mEnergy;

    if ( (mSpecies->metabolismMutation()) > 0 && (randIntInclusive(1, 100) <= mSpecies->metabolismMutation()) )
    {
        metabolism += randIntInclusive(-5, 5);
        ++mStatistics.mNumMutationsMetabolism;
    }
    setMetabolism(metabolism);

    if ( (mSpecies->spawningEnergyMutation()) > 0 && (randIntInclusive(1, 100) <= mSpecies->spawningEnergyMutation()) )
    {
        spawningEnergy += randIntInclusive(-100, 100);
        ++mStatistics.mNumMutationsSpawningEnergy;
    }
    setSpawningEnergy(spawningEnergy);

    mMovements = movements;
    if ( (mSpecies->movementMutation()) > 0 && (randIntInclusive(1, 100) <= mSpecies->movementMutation()) )
    {
        mMovements.setMovement(randIntInclusive(0, 2), randIntInclusive(0, 2),
                               static_cast<MovementDirections>(randIntInclusive(0, MoveMax - 1)));
        ++mStatistics.mNumMutationsMovement;
    }

    mDirection =  direction;
    mNextEnergyDiff = 0;
    mNextEaten.store(false, std::memory_order_relaxed);
    mLastSpawnAge = 0;
}

QPointF Animal::movement(unsigned int friends, unsigned int enemies, const QPointF &curDirection) const
{
    switch ( mMovements.getMovement(friends, enemies) )
    {
    case MoveUp:
        return QPointF(0.0, -1.0);
    case MoveUpRight:
        return QPointF(0.5, -0.5);
    case MoveRight:
        return QPointF(1.0, 0.0);
    case MoveDownRight:
        return QPointF(0.5, 0.5);
    case MoveDown:
        return QPointF(0.0, 1.0);
    case MoveDownLeft:
        return QPointF(-0.5, 0.5);
    case MoveLeft:
        return QPointF(-1.0, 0.0);
    case MoveUpLeft:
        return QPointF(-0.5, -0.5);
    case MoveRandom:
        return QPointF(qreal(randIntInclusive(-1, 1)) / 2.0, qreal(randIntInclusive(-1, 1)) / 2.0);
    case MoveStop:
        return QPointF(0.0, 0.0);
    case MoveGo:
        return curDirection;
    case MoveTurnAround:
        return QPointF(-curDirection.x(), -curDirection.y());
    case MoveTurnRight:
        return QPointF(curDirection.y(), -curDirection.x());
    case MoveTurnLeft:
        return QPointF(-curDirection.y(), curDirection.x());
    case MoveMerge:
    {
        const Animal * neighbor = mSpecies->firstNeighbor(mCellX, mCellY);
        if ( !neighbor )
        {
            return curDirection;
        }
        return neighbor->mDirection;
    }
    case MoveSplit:
    {
        const Animal * neighbor = mSpecies->firstNeighbor(mCellX, mCellY);
        if ( !neighbor )
        {
            return curDirection;
        }
        return QPointF(qreal(randIntInclusive(-1, 1)) / 2.0, qreal(randIntInclusive(-1, 1)) / 2.0);
    }
    default:
        return QPointF(0.0, 0.0);
    }
}

void Animal::setPos(QPointF pos)
{
    mPos = pos;
    syncCellFromPos();
}

void Animal::syncCellFromPos()
{
    mCellX = clampCellX(int(mPos.x()));
    mCellY = clampCellY(int(mPos.y()));
}

const QPointF & Animal::pos() const
{
    return mPos;
}

const QColor & Animal::color() const
{
    return mColor;
}

const Movements & Animal::movements() const
{
    return mMovements;
}

double Animal::energy() const
{
    return mEnergy;
}

void Animal::setEnergy(double energy)
{
    mEnergy = energy;
    mStatistics.mEnergy = mEnergy;
}

int Animal::metabolism() const
{
    return mMetabolism;
}

void Animal::setMetabolism(int metabolism)
{
    mMetabolism = qBound(1, metabolism, 100);
    mStatistics.mMetabolism = mMetabolism;
}

int Animal::spawningEnergy() const
{
    return mSpawningEnergy;
}

void Animal::setSpawningEnergy(int spawningEnergy)
{
    mSpawningEnergy = qBound(10, spawningEnergy, 2500);
    mStatistics.mSpawningEnergy = mSpawningEnergy;

}

const Animal::Statistics &Animal::statistics() const
{
    return mStatistics;
}

Species *Animal::species() const
{
    return mSpecies;
}

void Animal::calculateMovement()
{
    if ( mSpecies->type() == Species::typePlant )
    {
        return;
    }

    const int friends = mSpecies->friendCount(mCellX, mCellY) - 1;
    const int enemies = mSpecies->enemyCount(mCellX, mCellY);
    const int plants = mSpecies->plantCount(mCellX, mCellY);

    mStatistics.mNumFriends = static_cast<uint>(friends);
    mStatistics.mNumEnemies = static_cast<uint>(enemies);

    if ( plants > 0 )
    {
        mNextEnergyDiff += mSpecies->eatWeakestPlant(mCellX, mCellY) / qMax(1, enemies + friends + 1);
    }

    if ( enemies > 0 && friends >= 3)
    {
        mNextEnergyDiff += mSpecies->killWeakestEnemy(mCellX, mCellY) / qMax(friends, 1);
    }

    mDirection = movement(static_cast<unsigned int>(qMax(friends, 0)),
                          static_cast<unsigned int>(qMax(enemies, 0)),
                          mDirection);
    mNextMovement = (mDirection * (mMetabolism / 100.0));

    mNextPos = mPos + mNextMovement;

    if ( mNextPos.x() > LABORATORY_WIDTH - 1  )
    {
        mNextPos.setX(1);
    }
    else if ( mNextPos.x() < 1 )
    {
        mNextPos.setX( LABORATORY_WIDTH - 1 );
    }
    if ( mNextPos.y() > LABORATORY_HEIGHT - 1)
    {
        mNextPos.setY(1);
    }
    else if ( mNextPos.y() < 1 )
    {
        mNextPos.setY( LABORATORY_HEIGHT - 1 );
    }
}

void Animal::executeMovement()
{
    ++mStatistics.mAge;

    if ( mNextEaten.load(std::memory_order_relaxed) )
    {
        killSelf();
        return;
    }

    if ( mNextEnergyDiff > 0 )
    {
        mEnergy += mNextEnergyDiff;
        mNextEnergyDiff = 0;
    }

    if ( species()->type() == Species::typePlant )
    {
        mEnergy += mMetabolism / 100.0;
        mStatistics.mEnergy = mEnergy;
    }
    else
    {
        mEnergy -= (mNextMovement.manhattanLength() + BASE_METABOLISM);
        mStatistics.mEnergy = mEnergy;
    }
    if ( mEnergy <= 0 )
    {
        killSelf();
        return;
    }
    if ( mEnergy >= mSpawningEnergy && mStatistics.mAge > ANIMAL_MINIMUM_SPAWN_AGE
         && (mStatistics.mAge - mLastSpawnAge) >= ANIMAL_MINIMUM_SPAWN_RATE )
    {
        spawnSelf();
    }

    if ( species()->type() == Species::typeAnimal )
    {
        const int newX = clampCellX(int(mNextPos.x()));
        const int newY = clampCellY(int(mNextPos.y()));
        mSpecies->moveAnimal(mCellX, mCellY, newX, newY, this);
        mPos = mNextPos;
        mCellX = newX;
        mCellY = newY;
    }
}

bool Animal::tryClaimEaten()
{
    bool expected = false;
    return mNextEaten.compare_exchange_strong(expected, true, std::memory_order_relaxed);
}

bool Animal::isEaten() const
{
    return mNextEaten.load(std::memory_order_relaxed);
}

void Animal::markAsEaten()
{
    mNextEaten.store(true, std::memory_order_relaxed);
}

void Animal::killSelf()
{
    if ( mSpecies->type() == Species::typeAnimal )
    {
        DeathEffects::notify(mPos, mColor);
    }
    mSpecies->killAnimal(mCellX, mCellY, this);
}

void Animal::spawnSelf()
{
    mSpecies->spawnAnimal(mPos, mEnergy / 2, mSpawningEnergy, mMetabolism, mMovements, mDirection, this);
    mEnergy = mEnergy / 2;
    mStatistics.mEnergy = mEnergy;
    ++mStatistics.mNumChildren;
    mLastSpawnAge = mStatistics.mAge;
}
