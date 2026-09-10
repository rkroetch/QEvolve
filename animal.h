#ifndef ANIMAL_H
#define ANIMAL_H

#include <QGraphicsRectItem>
#include <QPoint>
#include <QRect>
#include <QPointF>
#include <random>
#include "laboratory.h"
#include "species.h"
#include "common.h"

class Animal
{
public:
    constexpr static const double BASE_METABOLISM = 0.1;

    struct Statistics
    {
        uint mAge = 0;
        uint mGeneration = 0;
        uint mNumChildren = 0;
        uint mNumMutationsMovement = 0;
        uint mNumMutationsSpawningEnergy = 0;
        uint mNumMutationsMetabolism = 0;
        uint mNumFriends = 0;
        uint mNumEnemies = 0;
        double mEnergy = 0.0;
        int mMetabolism = 0;
        int mSpawningEnergy = 0;
    };

public:
    explicit Animal() = default;
    explicit Animal(QPointF pos, Species * species, double energy, int spawningEnergy, int metabolism, const Movements & movements);
    ~Animal() = default;
    void initialize(QPointF pos, Species * species, double energy, int spawningEnergy, int metabolism, const Movements & movements, QPointF direction, const Animal *parent);

    QPointF movement(unsigned int friends, unsigned int enemies, const QPointF & curDirection) const;
    void calculateMovement();
    void executeMovement();

    void markAsEaten();

    void setPos(QPointF pos);
    const QPointF & pos() const;

    const QColor & color() const;

    QPointF movement(unsigned int friends, unsigned int enemies) const;

    double energy() const;
    void setEnergy(double energy);

    int metabolism() const;
    void setMetabolism(int metabolism);

    Species * species() const;

    int spawningEnergy() const;
    void setSpawningEnergy(int spawningEnergy);

    const Statistics & statistics() const;

protected:
    void killSelf();
    void spawnSelf();

private:
    Statistics mStatistics;
    std::random_device mRd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 mRdGen; //Standard mersenne_twister_engine seeded with rd()
    QPointF mNextPos;
    QPointF mNextMovement;
    QPointF mPos;
    QPointF mDirection;
    QColor mColor;
    double mNextEnergyDiff = 0.0;
    bool   mNextEaten = false;
    double mEnergy = 0.0;
    int mMetabolism = 0;
    int mSpawningEnergy = 0;

    Species * mSpecies;
    //[Friends][Enemies]
    Movements mMovements;
};

#endif // ANIMAL_H
