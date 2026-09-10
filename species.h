#ifndef SPECIES_H
#define SPECIES_H

#include <QObject>
#include <QPointF>
#include <QList>
#include <QMultiHash>
#include <QColor>
#include <QVector>
#include <random>
#include "laboratory.h"
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

    void clear();
    void initialize(int numAnimals, int maxAnimals, int initialEnergy);
    void respawn(int numAnimals, int initialEnergy);

    QString name() const;

    void toggleMovement(int friends, int enemies);

    int movementMutation() const;
    const Movements & movements() const;

    int metabolism() const;
    int metabolismMutation() const;
    int spawningEnergy() const;
    int spawningEnergyMutation() const;

    bool canSpawn() const;

    double eatWeakestPlant(const QPointF & pos, Animal *& weakestPlant);
    double killWeakestEnemy(const QPointF &pos, Animal *&weakestAnimal);

    int friendCount(QPointF pos) const;
    int enemyCount(QPointF pos) const;
    int plantCount(QPointF pos) const;
    QVector<Animal *> friends(QPointF pos) const;
    QVector<Animal*> enemies(QPointF pos) const;
    QVector<Animal *> plants(QPointF pos) const;

    double energyLevel(QPointF pos) const;
    double energyLevel(int x, int y) const;
    double threatLevel(QPointF pos) const;

    void removeAnimal(QPointF pos, Animal * animal);
    void addAnimal(QPointF pos, Animal * animal);

    void moveAnimal(QPointF oldPos, QPointF newPos, Animal * animal);
    void killAnimal(QPointF pos, Animal * animal);
    void spawnAnimal(QPointF pos, double startingEnergy, int spawningEnergy, int metabolism, const Movements & movements, const QPointF & direction, const Animal *parent);

    void setColor(QColor color);
    QColor color() const;

    QList<Animal*> & animals();

    void save(const QString & filename);

    QPixmap heatMap() const;

    SpeciesType type() const;

    QString statistics() const;

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
    void wrapHeight(int & y) const;
    void wrapWidth(int & x) const;

private:
    static const uint SPECIES_MAGIC_NUMBER = 0x0E7017E0;
    static const uint SPECIES_VERSION_NUMBER = 1;

private:
    std::random_device mRd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 mRdGen; //Standard mersenne_twister_engine seeded with rd()
    SpeciesUserData mUserData;

    int mMaximumAnimals;
    int mSpeciesIndex;
    SpeciesType mType;


    //Hash keyed on: top 16 bits Y, bottom 16 bits X
//    QMultiHash<int, Animal *> mAnimals;
//    QHash<int, int> mFriendCounts;
    static QList<Species*> mSpeciesList;
    int mFriendCounts[LABORATORY_HEIGHT][LABORATORY_WIDTH]{};
    QVector<Animal *> mFriends[LABORATORY_HEIGHT][LABORATORY_WIDTH];
    QList<Animal *> mInactiveAnimals;
    QList<Animal *> mAnimals;

};

#endif // SPECIES_H
