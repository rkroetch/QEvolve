#include "species.h"
#include "animal.h"
#include <QDebug>
#include <QFile>
#include <QDataStream>
#include <QFileInfo>

#include <QImage>
#include <QPainter>
#include <utility>

QList<Species *> Species::mSpeciesList;// List Definition

Species::Species(SpeciesType type, QObject * parent) : QObject(parent),
    mRdGen(mRd()),
    mType(type)
{
    mSpeciesList.append(this);
    mSpeciesIndex = mSpeciesList.size() - 1;
    mMaximumAnimals = 250;

    memset(reinterpret_cast<int*>(mFriendCounts), 0, LABORATORY_HEIGHT * LABORATORY_WIDTH);
}

Species::~Species()
{
    mSpeciesList.removeAt(mSpeciesIndex);
}

void Species::initialize(int numAnimals, int maxAnimals, int initialEnergy)
{
    mMaximumAnimals = maxAnimals;
    if ( numAnimals > maxAnimals )
    {
        numAnimals = maxAnimals;
    }

    std::uniform_int_distribution<> randXPos(5, LABORATORY_WIDTH - 5);
    std::uniform_int_distribution<> randYPos(5, LABORATORY_HEIGHT - 5);
    {
        int x = randXPos(mRdGen);
        int y = randYPos(mRdGen);
        QPointF pos(x, y);
        auto * animal = new Animal(pos, this, initialEnergy, mUserData.mSpawningEnergy, mUserData.mMetabolism, mUserData.mMovements);
        if ( animal )
        {
            addAnimal(pos, animal);
        }
        else
        {
            qWarning() << "Failed to create animal!";
        }
    }

    for ( int index = 0; index < (maxAnimals - numAnimals); ++index )
    {
        auto * animal = new Animal();
        mInactiveAnimals.append(animal);
    }
}

void Species::respawn(int numAnimals, int initialEnergy)
{
    if ( mType != typePlant )
    {
        return;
    }

    int numToSpawn = qMin(numAnimals, mInactiveAnimals.size() );

    //Single Square
//    std::uniform_int_distribution<> randXPos(((LABORATORY_WIDTH / 2)), ((LABORATORY_WIDTH / 4) + (LABORATORY_WIDTH / 2)));
//    std::uniform_int_distribution<> randYPos(((LABORATORY_HEIGHT / 2)), ((LABORATORY_HEIGHT / 4) + (LABORATORY_HEIGHT / 2)));
//    for ( int index = 0; index < numToSpawn; ++index )
//    {
//        int x = randXPos(mRdGen);
//        int y = randYPos(mRdGen);
//        spawnAnimal(QPointF(x, y), initialEnergy, spawningEnergy(), metabolism(), movements(), QPointF(0,0), nullptr);
//    }

    std::uniform_int_distribution<> randXPos(0, LABORATORY_WIDTH);
    std::uniform_int_distribution<> randYPos(0, LABORATORY_HEIGHT);
    for ( int index = 0; index < numToSpawn; ++index )
    {
        int x = randXPos(mRdGen);
        int y = randYPos(mRdGen);
        spawnAnimal(QPointF(x, y), initialEnergy, spawningEnergy(), metabolism(), movements(), QPointF(0,0), nullptr);
    }
}

//ver 1.0
//3 5 7 15 8 7 15 7 7   //Left to right - Top to bottom
//50                    //Metabolism
//120                   //Spawn Threshold / 10
//7                     //Mutation Rate (genes)
//4                     //Mutation Rate (metabolism)
//10                    //Mutation Rate (spawn)
//20                    //???
bool Species::load(const QString &filename, Species & species)
{
    QFile file(filename);
    file.open(QIODevice::ReadOnly);

    QString speciesString = file.readAll();
    QStringList lines = speciesString.split("\r\n", Qt::SkipEmptyParts);

    if ( lines.size() < 8 )
    {
        qWarning() << "Species" << filename << "failed to load." << "Not enough data.";
        return false;
    }

    if ( lines.at(0) != "ver 1.0" )
    {
        qWarning() << "Species" << filename << "failed to load." << "Invalid file version.";
        return false;
    }

    QStringList genes = lines.at(1).split(' ', Qt::SkipEmptyParts);
    if ( genes.size() != 9 )
    {
        qWarning() << "Species" << filename << "failed to load." << "Invalid number of genes.";
        return false;
    }

    QFileInfo fileInfo(filename);
    species.mUserData.mName = fileInfo.fileName().remove(".SPC");

    species.setMovement(0, 0, MovementDirections(genes.at(0).toUInt()));
    species.setMovement(0, 1, MovementDirections(genes.at(1).toUInt()));
    species.setMovement(0, 2, MovementDirections(genes.at(2).toUInt()));
    species.setMovement(1, 0, MovementDirections(genes.at(3).toUInt()));
    species.setMovement(1, 1, MovementDirections(genes.at(4).toUInt()));
    species.setMovement(1, 2, MovementDirections(genes.at(5).toUInt()));
    species.setMovement(2, 0, MovementDirections(genes.at(6).toUInt()));
    species.setMovement(2, 1, MovementDirections(genes.at(7).toUInt()));
    species.setMovement(2, 2, MovementDirections(genes.at(8).toUInt()));

    species.mUserData.mMetabolism = lines.at(2).toInt();
    species.mUserData.mSpawningEnergy = lines.at(3).toInt() * 10;
    species.mUserData.mMovementMutation = lines.at(4).toInt();
    species.mUserData.mMetabolismMutation = lines.at(5).toInt();
    species.mUserData.mSpawningEnergyMutation = lines.at(6).toInt();
    species.mUserData.mUnknown = lines.at(7).toInt();

    return true;
}

void Species::clear()
{
    qDeleteAll(mInactiveAnimals);
    mInactiveAnimals.clear();
    qDeleteAll(mAnimals);
    mAnimals.clear();
    for ( int y = 0; y < LABORATORY_HEIGHT; ++y )
    {
        for ( int x = 0; x < LABORATORY_WIDTH; ++x )
        {
            mFriends[y][x].clear();
            mFriendCounts[y][x] = 0;
        }
    }
}

//ver 1.0
//3 5 7 15 8 7 15 7 7   //Left to right - Top to bottom
//50                    //Metabolism
//120                   //Spawn Threshold / 10
//7                     //Mutation Rate (genes)
//4                     //Mutation Rate (metabolism)
//10                    //Mutation Rate (spawn)
//20                    //???

void Species::save(const QString &filename)
{
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);

    QString fileFormat(
                "ver 1.0\r\n"
                "%1 %2 %3 %4 %5 %6 %7 %8 %9\r\n"   //Left to right - Top to bottom
                "%10\r\n"                          //Metabolism
                "%11\r\n"                          //Spawn Threshold / 10
                "%12\r\n"                          //Mutation Rate (genes)
                "%13\r\n"                          //Mutation Rate (metabolism)
                "%14\r\n"                          //Mutation Rate (spawn)
                "%15\r\n");                        //???

    QString fileOutput = fileFormat;
    fileOutput = fileOutput.arg(mUserData.mMovements.getMovement(0, 0));
    fileOutput = fileOutput.arg(mUserData.mMovements.getMovement(0, 1));
    fileOutput = fileOutput.arg(mUserData.mMovements.getMovement(0, 2));
    fileOutput = fileOutput.arg(mUserData.mMovements.getMovement(1, 0));
    fileOutput = fileOutput.arg(mUserData.mMovements.getMovement(1, 1));
    fileOutput = fileOutput.arg(mUserData.mMovements.getMovement(1, 2));
    fileOutput = fileOutput.arg(mUserData.mMovements.getMovement(2, 0));
    fileOutput = fileOutput.arg(mUserData.mMovements.getMovement(2, 1));
    fileOutput = fileOutput.arg(mUserData.mMovements.getMovement(2, 2));

    fileOutput = fileOutput.arg(mUserData.mMetabolism);
    fileOutput = fileOutput.arg(mUserData.mSpawningEnergy / 10);
    fileOutput = fileOutput.arg(mUserData.mMovementMutation);
    fileOutput = fileOutput.arg(mUserData.mMetabolismMutation);
    fileOutput = fileOutput.arg(mUserData.mSpawningEnergyMutation);
    fileOutput = fileOutput.arg(mUserData.mUnknown);

    file.write(fileOutput.toLatin1());
    file.close();
}

QPixmap Species::heatMap() const
{
    QImage image(LABORATORY_WIDTH, LABORATORY_HEIGHT, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    for ( int y = 0; y < LABORATORY_HEIGHT; ++y )
    {
        for ( int x = 0; x < LABORATORY_WIDTH; ++x )
        {
            int friends = friendCount(QPointF(x, y));
            int enemies = enemyCount(QPointF(x, y));

            int friendAlpha = static_cast<int>(qMin(friends / 5.0, 1.0) * 255.0);
            int enemyAlpha = static_cast<int>(qMin(enemies / 5.0, 1.0) * 255.0);

            QColor friendColor(0, 255, 0, friendAlpha);
            QColor enemyColor(255, 0, 0, enemyAlpha);

            p.fillRect(x, y, 1, 1, friendColor);
            p.fillRect(x, y, 1, 1, enemyColor);
        }
    }

    return QPixmap::fromImage(image);
}

QString Species::name() const
{
    return mUserData.mName;
}

void Species::setName(const QString &name)
{
    mUserData.mName = name;
    emit nameChanged(mUserData.mName);
}

const Movements & Species::movements() const
{
    return mUserData.mMovements;
}

void Species::toggleMovement(int friends, int enemies)
{
    mUserData.mMovements.toggleMovement(friends, enemies);
}

void Species::setMovement(int friends, int enemies, MovementDirections direction)
{
    mUserData.mMovements.setMovement(friends, enemies, direction);
}

int Species::movementMutation() const
{
    return mUserData.mMovementMutation;
}

void Species::setMovementMutation(int movementMutation)
{
    mUserData.mMovementMutation = movementMutation;
}

int Species::metabolism() const
{
    return mUserData.mMetabolism;
}

void Species::setMetabolism(int metabolism)
{
    mUserData.mMetabolism = metabolism;
}

int Species::metabolismMutation() const
{
    return mUserData.mMetabolismMutation;
}

void Species::setMetabolismMutation(int mutation)
{
    mUserData.mMetabolismMutation = mutation;
}

int Species::spawningEnergy() const
{
    return mUserData.mSpawningEnergy;
}

void Species::setSpawningEnergy(int spawningEnergy)
{
    mUserData.mSpawningEnergy = spawningEnergy;
}

int Species::spawningEnergyMutation() const
{
    return mUserData.mSpawningEnergyMutation;
}

void Species::setSpawningEnergyMutation(int mutation)
{
    mUserData.mSpawningEnergyMutation = mutation;
}

bool Species::canSpawn() const
{
    return ( mAnimals.size() < mMaximumAnimals );
}

double Species::eatWeakestPlant(const QPointF & pos, Animal *& weakestPlant)
{
    double energy = 0;

    weakestPlant = nullptr;
    foreach ( Animal * plant, plants(pos) )
    {
        if ( !weakestPlant )
        {
            weakestPlant = plant;
        }
        else if ( weakestPlant->energy() > plant->energy() )
        {
            weakestPlant = plant;
        }
    }

    //    Die
    if ( weakestPlant )
    {
        energy = weakestPlant->energy();
        weakestPlant->markAsEaten();
    }

    return energy;
}

double Species::killWeakestEnemy(const QPointF & pos, Animal *& weakestAnimal)
{
    double energy = 0;

    weakestAnimal = nullptr;
    foreach ( Animal * animal, enemies(pos) )
    {
        if ( animal->species()->type() == Species::typePlant )
        {
            continue;
        }
        if ( !weakestAnimal )
        {
            weakestAnimal = animal;
        }
        else if ( weakestAnimal->energy() > animal->energy() )
        {
            weakestAnimal = animal;
        }
    }


//    foreach ( Animal * animal, friends(pos) )
//    {
//        if ( animal->species()->type() == Species::typePlant )
//        {
//            continue;
//        }
//        if ( !weakestAnimal )
//        {
//            weakestAnimal = animal;
//        }
//        else if ( weakestAnimal->energy() > animal->energy() )
//        {
//            weakestAnimal = animal;
//        }
//    }

    //    Die
    if ( weakestAnimal )
    {
        energy = weakestAnimal->energy();
        weakestAnimal->markAsEaten();
    }

    return energy;
}

int Species::friendCount(QPointF pos) const
{
    int yMinus = pos.y() - 1;
    int y = pos.y();
    int yPlus = pos.y() + 1;
    int xMinus = pos.x() - 1;
    int x = pos.x();
    int xPlus = pos.x() + 1;

    wrapHeight(yMinus);
    wrapHeight(y);
    wrapHeight(yPlus);

    wrapWidth(xMinus);
    wrapWidth(x);
    wrapWidth(xPlus);

    return (mFriendCounts[yMinus][xMinus] +
            mFriendCounts[yMinus][x] +
            mFriendCounts[yMinus][xPlus] +
            mFriendCounts[y][xMinus] +
            mFriendCounts[y][x] +
            mFriendCounts[y][xPlus] +
            mFriendCounts[yPlus][xMinus] +
            mFriendCounts[yPlus][x] +
            mFriendCounts[yPlus][xPlus]);
}

int Species::enemyCount(QPointF pos) const
{
    int count = 0;
    for( int index = 0; index < mSpeciesList.size(); ++index )
    {
        if ( index != mSpeciesIndex )
        {
            if ( mSpeciesList.at(index)->type() == typeAnimal )
            {
                count += mSpeciesList.at(index)->friendCount(pos);
            }
        }
    }
    return count;
}

int Species::plantCount(QPointF pos) const
{
    int count = 0;
    for( int index = 0; index < mSpeciesList.size(); ++index )
    {
        if ( index != mSpeciesIndex )
        {
            if ( mSpeciesList.at(index)->type() == typePlant )
            {
                count += mSpeciesList.at(index)->friendCount(pos);
            }
        }
    }
    return count;
}

QVector<Animal*> Species::friends(QPointF pos) const
{
    int yMinus = pos.y() - 1;
    int y = pos.y();
    int yPlus = pos.y() + 1;
    int xMinus = pos.x() - 1;
    int x = pos.x();
    int xPlus = pos.x() + 1;

    wrapHeight(yMinus);
    wrapHeight(y);
    wrapHeight(yPlus);

    wrapWidth(xMinus);
    wrapWidth(x);
    wrapWidth(xPlus);

    QVector<Animal *> localAnimals;
    localAnimals << mFriends[yMinus][xMinus];
    localAnimals << mFriends[yMinus][x];
    localAnimals << mFriends[yMinus][xPlus];
    localAnimals << mFriends[y][xMinus];
    localAnimals << mFriends[y][x];
    localAnimals << mFriends[y][xPlus];
    localAnimals << mFriends[yPlus][xMinus];
    localAnimals << mFriends[yPlus][x];
    localAnimals << mFriends[yPlus][xPlus];
    return localAnimals;

}

QVector<Animal *> Species::enemies(QPointF pos) const
{
    QVector<Animal*> animals;
    for( int index = 0; index < mSpeciesList.size(); ++index )
    {
        if ( index != mSpeciesIndex )
        {
            const Species  * species = mSpeciesList.at(index);
            if ( species->type() == typeAnimal )
            {
                animals += species->friends(pos);
            }
        }
    }
    return animals;
}

QVector<Animal *> Species::plants(QPointF pos) const
{
    QVector<Animal*> animals;
    for( int index = 0; index < mSpeciesList.size(); ++index )
    {
        if ( index != mSpeciesIndex )
        {
            const Species  * species = mSpeciesList.at(index);
            if ( species->type() == typePlant )
            {
                animals += species->friends(pos);
            }
        }
    }
    return animals;
}

double Species::energyLevel(QPointF pos) const
{
    return energyLevel(int(pos.x()), int(pos.y()));
}

double Species::energyLevel(int x, int y) const
{
    double energy = 0;
    foreach ( const Animal * animal, mFriends[y][x] )
    {
        energy += animal->energy();
    }
    return energy;
}

double Species::threatLevel(QPointF pos) const
{
    const int & x = pos.x();
    const int & y = pos.y();

    double threat = 0;
    for( int index = 0; index < mSpeciesList.size(); ++index )
    {
        if ( index != mSpeciesIndex )
        {
            if ( mSpeciesList.at(index)->type() == typeAnimal )
            {
                threat += mSpeciesList.at(index)->energyLevel(x,y);
            }
        }
    }
    return threat;
}

void Species::removeAnimal(QPointF pos, Animal * animal)
{
    const int & x = pos.x();
    const int & y = pos.y();

    if ( !mFriends[y][x].removeOne(animal) )
    {
        qCritical() << "Tried to remove animal:" << animal << ". This animal doesn't exist here.";
    }
    mFriendCounts[y][x]--;
    mAnimals.removeOne(animal);
}

void Species::addAnimal(QPointF pos, Animal * animal)
{
    const int & x = pos.x();
    const int & y = pos.y();
    mFriends[y][x].append(animal);
    mFriendCounts[y][x]++;
    mAnimals.append(animal);
}

void Species::moveAnimal(QPointF oldPos, QPointF newPos, Animal * animal)
{
    const int & oldX = oldPos.x();
    const int & oldY = oldPos.y();
    if ( !mFriends[oldY][oldX].removeOne(animal) )
    {
        qCritical() << "Tried to move animal:" << animal << ". This animal doesn't exist here.";
    }
    mFriendCounts[oldY][oldX]--;

    const int & newX = newPos.x();
    const int & newY = newPos.y();
    mFriends[newY][newX].append(animal);
    mFriendCounts[newY][newX]++;
}

void Species::killAnimal(QPointF pos, Animal * animal)
{
    removeAnimal(pos, animal);
    mInactiveAnimals.append(animal);
}

void Species::spawnAnimal(QPointF pos, double startingEnergy, int spawningEnergy, int metabolism, const Movements & movements, const QPointF & direction, const Animal * parent)
{
    std::uniform_int_distribution<> randPosOffset(-4, 4);
    if ( !mInactiveAnimals.isEmpty() )
    {
        Animal * animal = mInactiveAnimals.takeLast();

        QPointF newPos(pos.x() + randPosOffset(mRdGen), pos.y() + randPosOffset(mRdGen));

        if ( newPos.x() > LABORATORY_WIDTH - 1  )
        {
            newPos.setX(1);
        }
        else if ( newPos.x() < 1 )
        {
            newPos.setX( LABORATORY_WIDTH - 1 );
        }
        if ( newPos.y() > LABORATORY_HEIGHT - 1)
        {
            newPos.setY(1);
        }
        else if ( newPos.y() < 1 )
        {
            newPos.setY( LABORATORY_HEIGHT - 1 );
        }

        animal->initialize(newPos, this, startingEnergy, spawningEnergy, metabolism, movements, direction, parent);
        addAnimal(newPos, animal);
    }
}

void Species::setColor(QColor color)
{
    mUserData.mColor = std::move(color);
    emit colorChanged(mUserData.mColor);
}

QColor Species::color() const
{
    return mUserData.mColor;
}

QList<Animal*> & Species::animals()
{
    return mAnimals;
}

void Species::wrapHeight(int & y) const
{
    if ( y > LABORATORY_HEIGHT - 1)
    {
        y = 1;
    }
    else if ( y < 1 )
    {
        y = LABORATORY_HEIGHT - 1;
    }
}

void Species::wrapWidth(int & x) const
{
    if ( x > LABORATORY_WIDTH - 1  )
    {
        x = 1;
    }
    else if ( x < 1 )
    {
        x = LABORATORY_WIDTH - 1;
    }
}

Species::SpeciesType Species::type() const
{
    return mType;
}

QString Species::statistics() const
{
    if ( mAnimals.isEmpty() )
    {
        QString output;
        QTextStream os(&output);
        os << mUserData.mName << Qt::endl;
        os << "Empty" << Qt::endl;
        os << Qt::endl;
        return output;
    }

    Animal::Statistics minStats = mAnimals.first()->statistics();
    Animal::Statistics maxStats;
    Animal::Statistics sumStats;
    Animal::Statistics meanStats;


    for ( const Animal * animal : mAnimals )
    {
        const Animal::Statistics & stats = animal->statistics();
        //Min
        if ( minStats.mAge > stats.mAge )
        {
            minStats.mAge = stats.mAge;
        }
        if ( minStats.mGeneration > stats.mGeneration )
        {
            minStats.mGeneration = stats.mGeneration;
        }
        if ( minStats.mNumChildren > stats.mNumChildren )
        {
            minStats.mNumChildren = stats.mNumChildren;
        }
        if ( minStats.mNumMutationsMovement > stats.mNumMutationsMovement )
        {
            minStats.mNumMutationsMovement = stats.mNumMutationsMovement;
        }
        if ( minStats.mNumMutationsSpawningEnergy > stats.mNumMutationsSpawningEnergy )
        {
            minStats.mNumMutationsSpawningEnergy = stats.mNumMutationsSpawningEnergy;
        }
        if ( minStats.mNumMutationsMetabolism > stats.mNumMutationsMetabolism )
        {
            minStats.mNumMutationsMetabolism = stats.mNumMutationsMetabolism;
        }
        if ( minStats.mEnergy > stats.mEnergy )
        {
            minStats.mEnergy = stats.mEnergy;
        }
        if ( minStats.mMetabolism > stats.mMetabolism )
        {
            minStats.mMetabolism = stats.mMetabolism;
        }
        if ( minStats.mSpawningEnergy > stats.mSpawningEnergy )
        {
            minStats.mSpawningEnergy = stats.mSpawningEnergy;
        }
        if ( minStats.mNumFriends > stats.mNumFriends )
        {
            minStats.mNumFriends = stats.mNumFriends;
        }
        if ( minStats.mNumEnemies > stats.mNumEnemies )
        {
            minStats.mNumEnemies = stats.mNumEnemies;
        }
        //Max
        if ( maxStats.mAge < stats.mAge )
        {
            maxStats.mAge = stats.mAge;
        }
        if ( maxStats.mGeneration < stats.mGeneration )
        {
            maxStats.mGeneration = stats.mGeneration;
        }
        if ( maxStats.mNumChildren < stats.mNumChildren )
        {
            maxStats.mNumChildren = stats.mNumChildren;
        }
        if ( maxStats.mNumMutationsMovement < stats.mNumMutationsMovement )
        {
            maxStats.mNumMutationsMovement = stats.mNumMutationsMovement;
        }
        if ( maxStats.mNumMutationsSpawningEnergy < stats.mNumMutationsSpawningEnergy )
        {
            maxStats.mNumMutationsSpawningEnergy = stats.mNumMutationsSpawningEnergy;
        }
        if ( maxStats.mNumMutationsMetabolism < stats.mNumMutationsMetabolism )
        {
            maxStats.mNumMutationsMetabolism = stats.mNumMutationsMetabolism;
        }
        if ( maxStats.mEnergy < stats.mEnergy )
        {
            maxStats.mEnergy = stats.mEnergy;
        }
        if ( maxStats.mMetabolism < stats.mMetabolism )
        {
            maxStats.mMetabolism = stats.mMetabolism;
        }
        if ( maxStats.mSpawningEnergy < stats.mSpawningEnergy )
        {
            maxStats.mSpawningEnergy = stats.mSpawningEnergy;
        }
        if ( maxStats.mNumFriends < stats.mNumFriends )
        {
            maxStats.mNumFriends = stats.mNumFriends;
        }
        if ( maxStats.mNumEnemies < stats.mNumEnemies )
        {
            maxStats.mNumEnemies = stats.mNumEnemies;
        }
        //Mean
        sumStats.mAge += stats.mAge;
        sumStats.mGeneration += stats.mGeneration;
        sumStats.mNumChildren += stats.mNumChildren;
        sumStats.mNumMutationsMovement += stats.mNumMutationsMovement;
        sumStats.mNumMutationsSpawningEnergy += stats.mNumMutationsSpawningEnergy;
        sumStats.mNumMutationsMetabolism += stats.mNumMutationsMetabolism;
        sumStats.mEnergy += stats.mEnergy;
        sumStats.mMetabolism += stats.mMetabolism;
        sumStats.mSpawningEnergy += stats.mSpawningEnergy;
        sumStats.mNumFriends += stats.mNumFriends;
        sumStats.mNumEnemies += stats.mNumEnemies;
    }

    meanStats.mAge = sumStats.mAge / mAnimals.size();
    meanStats.mGeneration = sumStats.mGeneration / mAnimals.size();
    meanStats.mNumChildren = sumStats.mNumChildren / mAnimals.size();
    meanStats.mNumMutationsMovement = sumStats.mNumMutationsMovement / mAnimals.size();
    meanStats.mNumMutationsSpawningEnergy = sumStats.mNumMutationsSpawningEnergy / mAnimals.size();
    meanStats.mNumMutationsMetabolism = sumStats.mNumMutationsMetabolism / mAnimals.size();
    meanStats.mEnergy = sumStats.mEnergy / mAnimals.size();
    meanStats.mMetabolism = sumStats.mMetabolism / mAnimals.size();
    meanStats.mSpawningEnergy = sumStats.mSpawningEnergy / mAnimals.size();
    meanStats.mNumFriends = sumStats.mNumFriends / mAnimals.size();
    meanStats.mNumEnemies = sumStats.mNumEnemies / mAnimals.size();


    QString output;
    QTextStream os(&output);
    os << mUserData.mName << " (" << mAnimals.size() << ")" << Qt::endl;
    os << qSetFieldWidth(16);
    os << "" << "Min" << "Mean" << "Max" << Qt::endl;
    os << "Age:" << minStats.mAge << meanStats.mAge << maxStats.mAge << Qt::endl;
    os << "Generation:" << minStats.mGeneration << meanStats.mGeneration << maxStats.mGeneration << Qt::endl;
    os << "Children:" << minStats.mNumChildren << meanStats.mNumChildren << maxStats.mNumChildren << Qt::endl;
    os << "Energy:" << minStats.mEnergy << meanStats.mEnergy << maxStats.mEnergy << Qt::endl;
    os << "Metabolism:" << minStats.mMetabolism << meanStats.mMetabolism << maxStats.mMetabolism << Qt::endl;
    os << "Spawning Energy:" << minStats.mSpawningEnergy << meanStats.mSpawningEnergy << maxStats.mSpawningEnergy << Qt::endl;
    os << "Num Friends:" << minStats.mNumFriends << meanStats.mNumFriends << maxStats.mNumFriends << Qt::endl;
    os << "Num Enemies:" << minStats.mNumEnemies << meanStats.mNumEnemies << maxStats.mNumEnemies << Qt::endl;

    os << "Mut. Movement:" << minStats.mNumMutationsMovement << meanStats.mNumMutationsMovement << maxStats.mNumMutationsMovement << Qt::endl;
    os << "Mut. Sp. Energy:" << minStats.mNumMutationsSpawningEnergy << meanStats.mNumMutationsSpawningEnergy << maxStats.mNumMutationsSpawningEnergy << Qt::endl;
    os << "Mut. Metabolism:" << minStats.mNumMutationsMetabolism << meanStats.mNumMutationsMetabolism << maxStats.mNumMutationsMetabolism << Qt::endl;
    os << Qt::endl;
    return output;
}

