#ifndef SPECIES_H
#define SPECIES_H

#include <QObject>
#include <QPointF>
#include <QList>
#include <QColor>
#include <QVector>
#include <atomic>
#include "common.h"

class Animal;

struct SpeciesUserData
{
    QString mName;

    int mMetabolism = 100;
    int mMetabolismMutation = 0;

    //[Friends][Enemies]
    Movements mMovements;
    int mMovementMutation = 0;

    int mSpawningEnergy = 1000;
    int mSpawningEnergyMutation = 0;

    int mUnknown = 0;

    QColor mColor = Qt::white;
};

class Species : public QObject
{
    Q_OBJECT

public:
    enum SpeciesType { typePlant, typeAnimal };

    Species(SpeciesType type, QObject * parent = nullptr);
    ~Species();

    static bool load(const QString & filename, Species & species);
    static Species * plantSpecies();

    void clear();
    void initialize(int numAnimals, int maxAnimals, int initialEnergy, bool spawnInitialAnimal = true);
    void respawn(int numAnimals, int initialEnergy);

    bool isActive() const;
    void activate();
    void deactivate();

    void setPlantPattern(PlantPattern pattern) { mPlantPattern = pattern; }
    PlantPattern plantPattern() const { return mPlantPattern; }

    QString name() const;

    void toggleMovement(int friends, int enemies);

    int movementMutation() const;
    const Movements & movements() const;

    int metabolism() const;
    int metabolismMutation() const;
    int spawningEnergy() const;
    int spawningEnergyMutation() const;

    bool canSpawn() const;

    double eatWeakestPlant(int cellX, int cellY);
    double killWeakestEnemy(int cellX, int cellY);

    int friendCount(int cellX, int cellY) const;
    int enemyCount(int cellX, int cellY) const;
    int plantCount(int cellX, int cellY) const;
    Animal *firstNeighbor(int cellX, int cellY, const Animal * exclude = nullptr) const;
    void advanceCombatCycle();

    double energyLevel(QPointF pos) const;
    double energyLevel(int x, int y) const;
    double threatLevel(QPointF pos) const;

    void addAnimal(int cellX, int cellY, Animal * animal);
    void moveAnimal(int oldX, int oldY, int newX, int newY, Animal * animal);
    void killAnimal(int cellX, int cellY, Animal * animal);
    void spawnAnimal(QPointF pos, double startingEnergy, int spawningEnergy, int metabolism, const Movements & movements, const QPointF & direction, const Animal *parent);

    void setColor(QColor color);
    QColor color() const;

    QVector<Animal*> & animals();
    const QVector<Animal*> & animals() const;

    // This species' own animals occupying the given cell (see mOccupants) -
    // unlike animals(), which is every active animal in the species.
    const QVector<Animal*> & cellOccupants(int cellX, int cellY) const;

    void save(const QString & filename);

    QPixmap heatMap() const;

    SpeciesType type() const;

    QString statistics() const;

    template<typename Fn>
    void forEachNeighborCell(int cellX, int cellY, Fn && fn) const
    {
        int yMinus = cellY - 1;
        int y = cellY;
        int yPlus = cellY + 1;
        int xMinus = cellX - 1;
        int x = cellX;
        int xPlus = cellX + 1;

        wrapLabHeight(yMinus);
        wrapLabHeight(y);
        wrapLabHeight(yPlus);
        wrapLabWidth(xMinus);
        wrapLabWidth(x);
        wrapLabWidth(xPlus);

        fn(xMinus, yMinus);
        fn(x, yMinus);
        fn(xPlus, yMinus);
        fn(xMinus, y);
        fn(x, y);
        fn(xPlus, y);
        fn(xMinus, yPlus);
        fn(x, yPlus);
        fn(xPlus, yPlus);
    }

public slots:
    void setName(const QString & name);
    void setMovement(int friends, int enemies, MovementDirections direction);
    void setMovementMutation(int movementMutation);
    void setMetabolism(int metabolism);
    void setMetabolismMutation(int mutation);
    void setSpawningEnergy(int spawningEnergy);
    void setSpawningEnergyMutation(int mutation);

signals:
    void colorChanged(QColor color);
    void nameChanged(QString name);

private:
    void rebuildCaches();
    void removeAnimal(int cellX, int cellY, Animal * animal);
    void removeFromCell(int cellX, int cellY, Animal * animal);
    Animal *weakestInNeighborhood(int cellX, int cellY) const;
    bool tryClaimCombatCell(int cellX, int cellY);
    const QVector<Animal*> *occupants(int cellX, int cellY) const;
    QVector<Animal*> *occupants(int cellX, int cellY);
    QPointF jitteredPosition(QPointF pos) const;
    QPointF emptyNearbyCell(QPointF pos) const;

private:
    static const uint SPECIES_MAGIC_NUMBER = 0x0E7017E0;
    static const uint SPECIES_VERSION_NUMBER = 1;

private:
    SpeciesUserData mUserData;

    int mMaximumAnimals;
    int mInitialAnimalCount = 1;
    int mSpeciesIndex;
    SpeciesType mType;
    PlantPattern mPlantPattern = plantPatternOneGroup;
    bool mActive = true;

    static QList<Species*> mSpeciesList;
    static Species *mPlantSpecies;
    QVector<Species*> mEnemySpecies;
    int mCombatCycle = 0;
    std::atomic<int> mCellCombatCycle[LABORATORY_HEIGHT][LABORATORY_WIDTH]{};
    int mFriendCounts[LABORATORY_HEIGHT][LABORATORY_WIDTH]{};
    QVector<Animal *> mOccupants[LABORATORY_HEIGHT][LABORATORY_WIDTH];
    QVector<Animal *> mInactiveAnimals;
    QVector<Animal *> mAnimals;
};

#endif // SPECIES_H
