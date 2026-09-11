#include "species.h"
#include "animal.h"
#include <QDebug>
#include <QFile>
#include <QDataStream>
#include <QFileInfo>
#include <QTextStream>

#include <QImage>
#include <QPainter>
#include <cstring>
#include <utility>

QList<Species *> Species::mSpeciesList;// List Definition
Species * Species::mPlantSpecies = nullptr;

Species::Species(SpeciesType type, QObject * parent) : QObject(parent),
    mType(type)
{
    mSpeciesList.append(this);
    mSpeciesIndex = mSpeciesList.size() - 1;
    mMaximumAnimals = 250;
    rebuildCaches();
}

Species::~Species()
{
    clear();
    const qsizetype index = mSpeciesList.indexOf(this);
    if (index < 0) {
        return;
    }
    mSpeciesList.removeAt(index);
    for (qsizetype i = index; i < mSpeciesList.size(); ++i) {
        mSpeciesList[i]->mSpeciesIndex = static_cast<int>(i);
    }
    rebuildCaches();
}

Species * Species::plantSpecies()
{
    return mPlantSpecies;
}

void Species::rebuildCaches()
{
    mPlantSpecies = nullptr;
    for (Species * species : mSpeciesList)
    {
        species->mEnemySpecies.clear();
        if (species->type() == typePlant)
        {
            mPlantSpecies = species;
        }
    }
    for (Species * species : mSpeciesList)
    {
        if (species->type() != typeAnimal)
        {
            continue;
        }
        for (Species * other : mSpeciesList)
        {
            if (other != species && other->type() == typeAnimal)
            {
                species->mEnemySpecies.append(other);
            }
        }
    }
}

void Species::initialize(int numAnimals, int maxAnimals, int initialEnergy)
{
    mMaximumAnimals = maxAnimals;
    if ( numAnimals > maxAnimals )
    {
        numAnimals = maxAnimals;
    }

    {
        const int x = randIntInclusive(5, LABORATORY_WIDTH - 5);
        const int y = randIntInclusive(5, LABORATORY_HEIGHT - 5);
        QPointF pos(x, y);
        auto * animal = new Animal(pos, this, initialEnergy, mUserData.mSpawningEnergy, mUserData.mMetabolism, mUserData.mMovements);
        addAnimal(animal->cellX(), animal->cellY(), animal);
    }

    mInactiveAnimals.reserve(maxAnimals - numAnimals);
    for ( int index = 0; index < (maxAnimals - numAnimals); ++index )
    {
        mInactiveAnimals.append(new Animal());
    }
}

void Species::respawn(int numAnimals, int initialEnergy)
{
    if ( mType != typePlant )
    {
        return;
    }

    const int numToSpawn = qMin(numAnimals, int(mInactiveAnimals.size()));
    for ( int index = 0; index < numToSpawn; ++index )
    {
        const int x = randIntInclusive(0, LABORATORY_WIDTH - 1);
        const int y = randIntInclusive(0, LABORATORY_HEIGHT - 1);
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
            mOccupants[y][x].clear();
        }
    }
    memset(mFriendCounts, 0, sizeof(mFriendCounts));
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
            int friends = friendCount(x, y);
            int enemies = enemyCount(x, y);

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

const QVector<Animal*> *Species::occupants(int cellX, int cellY) const
{
    return &mOccupants[cellY][cellX];
}

QVector<Animal*> *Species::occupants(int cellX, int cellY)
{
    return &mOccupants[cellY][cellX];
}

Animal *Species::weakestInNeighborhood(int cellX, int cellY) const
{
    Animal * weakest = nullptr;
    forEachNeighborCell(cellX, cellY, [this, &weakest](int x, int y) {
        const QVector<Animal*> * cell = occupants(x, y);
        if (!cell)
        {
            return;
        }
        for (Animal * animal : *cell)
        {
            if (!animal || animal->isEaten())
            {
                continue;
            }
            if (!weakest || weakest->energy() > animal->energy())
            {
                weakest = animal;
            }
        }
    });
    return weakest;
}

void Species::advanceCombatCycle()
{
    ++mCombatCycle;
}

bool Species::tryClaimCombatCell(int cellX, int cellY)
{
    int observed = mCellCombatCycle[cellY][cellX].load(std::memory_order_relaxed);
    while (observed != mCombatCycle)
    {
        if (mCellCombatCycle[cellY][cellX].compare_exchange_weak(observed, mCombatCycle, std::memory_order_relaxed))
        {
            return true;
        }
    }
    return false;
}

double Species::eatWeakestPlant(int cellX, int cellY)
{
    Species * plants = mPlantSpecies;
    if (!plants)
    {
        return 0;
    }
    Animal * weakestPlant = plants->weakestInNeighborhood(cellX, cellY);
    if (!weakestPlant)
    {
        return 0;
    }
    if (!plants->tryClaimCombatCell(weakestPlant->cellX(), weakestPlant->cellY()))
    {
        return 0;
    }
    if (!weakestPlant->tryClaimEaten())
    {
        return 0;
    }
    return weakestPlant->energy();
}

double Species::killWeakestEnemy(int cellX, int cellY)
{
    Animal * weakestAnimal = nullptr;
    for (Species * species : mEnemySpecies)
    {
        Animal * candidate = species->weakestInNeighborhood(cellX, cellY);
        if (!candidate)
        {
            continue;
        }
        if (!weakestAnimal || weakestAnimal->energy() > candidate->energy())
        {
            weakestAnimal = candidate;
        }
    }
    if (!weakestAnimal)
    {
        return 0;
    }
    if (!weakestAnimal->species()->tryClaimCombatCell(weakestAnimal->cellX(), weakestAnimal->cellY()))
    {
        return 0;
    }
    if (!weakestAnimal->tryClaimEaten())
    {
        return 0;
    }
    return weakestAnimal->energy();
}

int Species::friendCount(int cellX, int cellY) const
{
    int count = 0;
    forEachNeighborCell(cellX, cellY, [this, &count](int x, int y) {
        count += mFriendCounts[y][x];
    });
    return count;
}

int Species::enemyCount(int cellX, int cellY) const
{
    int count = 0;
    for (const Species * species : mEnemySpecies)
    {
        count += species->friendCount(cellX, cellY);
    }
    return count;
}

int Species::plantCount(int cellX, int cellY) const
{
    return mPlantSpecies ? mPlantSpecies->friendCount(cellX, cellY) : 0;
}

Animal *Species::firstNeighbor(int cellX, int cellY) const
{
    Animal * found = nullptr;
    forEachNeighborCell(cellX, cellY, [this, &found](int x, int y) {
        if (found)
        {
            return;
        }
        const QVector<Animal*> * cell = occupants(x, y);
        if (!cell || cell->isEmpty())
        {
            return;
        }
        found = cell->first();
    });
    return found;
}

double Species::energyLevel(QPointF pos) const
{
    return energyLevel(int(pos.x()), int(pos.y()));
}

double Species::energyLevel(int x, int y) const
{
    const QVector<Animal*> * cell = occupants(x, y);
    if (!cell)
    {
        return 0;
    }
    double energy = 0;
    for (const Animal * animal : *cell)
    {
        energy += animal->energy();
    }
    return energy;
}

double Species::threatLevel(QPointF pos) const
{
    const int x = int(pos.x());
    const int y = int(pos.y());

    double threat = 0;
    for (const Species * species : mEnemySpecies)
    {
        threat += species->energyLevel(x, y);
    }
    return threat;
}

void Species::removeFromCell(int cellX, int cellY, Animal * animal)
{
    QVector<Animal*> & cell = mOccupants[cellY][cellX];
    const int slot = animal->cellSlot();
    const int last = int(cell.size()) - 1;
    if (slot < 0 || slot > last || cell.at(slot) != animal)
    {
        const qsizetype index = cell.indexOf(animal);
        if (index < 0)
        {
            return;
        }
        animal->setCellSlot(int(index));
        removeFromCell(cellX, cellY, animal);
        return;
    }
    if (slot != last)
    {
        cell[slot] = cell[last];
        cell[slot]->setCellSlot(slot);
    }
    cell.removeLast();
    mFriendCounts[cellY][cellX]--;
}

void Species::removeAnimal(int cellX, int cellY, Animal * animal)
{
    removeFromCell(cellX, cellY, animal);
    const int index = animal->listIndex();
    const int last = int(mAnimals.size()) - 1;
    if (index >= 0 && index <= last && mAnimals.at(index) == animal)
    {
        if (index != last)
        {
            mAnimals[index] = mAnimals[last];
            mAnimals[index]->setListIndex(index);
        }
        mAnimals.removeLast();
    }
    else
    {
        mAnimals.removeAll(animal);
    }
    animal->setListIndex(-1);
    animal->setCellSlot(-1);
}

void Species::addAnimal(int cellX, int cellY, Animal * animal)
{
    QVector<Animal*> & cell = mOccupants[cellY][cellX];
    animal->setCellSlot(int(cell.size()));
    cell.append(animal);
    mFriendCounts[cellY][cellX]++;
    animal->setListIndex(int(mAnimals.size()));
    mAnimals.append(animal);
}

void Species::moveAnimal(int oldX, int oldY, int newX, int newY, Animal * animal)
{
    if (oldX == newX && oldY == newY)
    {
        return;
    }
    removeFromCell(oldX, oldY, animal);
    QVector<Animal*> & cell = mOccupants[newY][newX];
    animal->setCellSlot(int(cell.size()));
    cell.append(animal);
    mFriendCounts[newY][newX]++;
}

void Species::killAnimal(int cellX, int cellY, Animal * animal)
{
    removeAnimal(cellX, cellY, animal);
    mInactiveAnimals.append(animal);
}

void Species::spawnAnimal(QPointF pos, double startingEnergy, int spawningEnergy, int metabolism, const Movements & movements, const QPointF & direction, const Animal * parent)
{
    if ( mInactiveAnimals.isEmpty() )
    {
        return;
    }
    Animal * animal = mInactiveAnimals.takeLast();

    QPointF newPos(pos.x() + randIntInclusive(-4, 4), pos.y() + randIntInclusive(-4, 4));

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
    addAnimal(animal->cellX(), animal->cellY(), animal);
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

QVector<Animal*> & Species::animals()
{
    return mAnimals;
}

const QVector<Animal*> & Species::animals() const
{
    return mAnimals;
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

