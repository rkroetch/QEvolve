#include "animal.h"
#include <QGraphicsScene>
#include <QList>
#include <QDebug>
#include <QBrush>

Animal::Animal(QPointF pos, Species * species, double energy, int spawningEnergy, int metabolism, const Movements & movements) :
    mRdGen(mRd())
{
    std::uniform_int_distribution<> randDirection(-1, 1);
    QPointF direction(randDirection(mRdGen), randDirection(mRdGen));

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
    mSpecies = species;

    mPos = pos;

    mColor = species->color();
    mEnergy = energy;
    mStatistics.mEnergy = mEnergy;

    std::uniform_int_distribution<> randOffset(1, 100);
    if ( (mSpecies->metabolismMutation()) > 0 && (randOffset(mRdGen) <= mSpecies->metabolismMutation()) )
    {
        std::uniform_int_distribution<> randMetabolism(-5, 5);
        metabolism += randMetabolism(mRdGen);
        ++mStatistics.mNumMutationsMetabolism;
    }
    setMetabolism(metabolism);

    if ( (mSpecies->spawningEnergyMutation()) > 0 && (randOffset(mRdGen) <= mSpecies->spawningEnergyMutation()) )
    {
        std::uniform_int_distribution<> randEnergy(-100, 100);
        spawningEnergy += randEnergy(mRdGen);
        ++mStatistics.mNumMutationsSpawningEnergy;
    }
    setSpawningEnergy(spawningEnergy);

    //Copy over our movements from our parent
    mMovements = movements;
    if ( (mSpecies->movementMutation()) > 0 && (randOffset(mRdGen) <= mSpecies->movementMutation()) )
    {
        std::uniform_int_distribution<> randMove(0, 2);
        std::uniform_int_distribution<> randDirection(0, MoveMax-1);
        mMovements.setMovement(randMove(mRdGen), randMove(mRdGen), static_cast<MovementDirections>(randDirection(mRdGen)));
        ++mStatistics.mNumMutationsMovement;
    }

    mDirection =  direction;
    mNextEnergyDiff = 0;
    mNextEaten = false;
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
        return QPointF(qreal((rand() % 3) - 1.0) / 2.0, qreal((rand() % 3) - 1.0) / 2.0);
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
        const QVector<Animal*> & friends = mSpecies->friends(mPos);
        if ( friends.isEmpty() )
        {
            return curDirection;
        }
        return friends.first()->mDirection;
    }
    case MoveSplit:
    {
        const QVector<Animal*> & friends = mSpecies->friends(mPos);
        if ( friends.isEmpty() )
        {
            return curDirection;
        }
        return QPointF(qreal((rand() % 3) - 1.0) / 2.0, qreal((rand() % 3) - 1.0) / 2.0);
    }
    default:
        return QPointF(0.0, 0.0);
    }
}

void Animal::setPos(QPointF pos)
{
    mPos = pos;
}

const QPointF & Animal::pos() const
{
    return mPos;
}

const QColor & Animal::color() const
{
    return mColor;
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

    //Dont include ourselves in the count
    int friends = mSpecies->friendCount(mPos) - 1;
    int enemies = mSpecies->enemyCount(mPos);
    int plants = mSpecies->plantCount(mPos);

    mStatistics.mNumFriends = static_cast<uint>(friends);
    mStatistics.mNumEnemies = static_cast<uint>(enemies);

    if ( plants > 0 )
    {
        Animal * weakestPlant = nullptr;
        double energy = mSpecies->eatWeakestPlant(mPos, weakestPlant);
        if ( weakestPlant )
        {
            mNextEnergyDiff += (energy / (enemies + friends + 1));
        }
    }

    if ( enemies > 0 && friends >= 3)
    {
        Animal * weakestAnimal = nullptr;
        double energy = mSpecies->killWeakestEnemy(mPos, weakestAnimal);
        if ( weakestAnimal )
        {
            mNextEnergyDiff += (energy / friends);
        }
    }

    mDirection = movement(friends, enemies, mDirection);
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

    //If we were eaten last cycle, kill ourselves
    if ( mNextEaten )
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
        mEnergy += mMetabolism / 100;
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
    if ( mEnergy >= mSpawningEnergy )
    {
        spawnSelf();
    }

    if ( species()->type() == Species::typeAnimal )
    {
        mSpecies->moveAnimal(mPos, mNextPos, this);
        mPos = mNextPos;
    }
}

void Animal::markAsEaten()
{
    mNextEaten = true;
}

void Animal::killSelf()
{
    mSpecies->killAnimal(mPos, this);
}

void Animal::spawnSelf()
{
    mSpecies->spawnAnimal(mPos, mEnergy / 2, mSpawningEnergy, mMetabolism, mMovements, mDirection, this);
    mEnergy = mEnergy / 2;
    mStatistics.mEnergy = mEnergy;
    ++mStatistics.mNumChildren;
}



