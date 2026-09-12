#ifndef ANIMAL_H
#define ANIMAL_H

#include <QPoint>
#include <QPointF>
#include <QColor>
#include <atomic>
#include "common.h"

class Species;

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
    Animal() = default;
    Animal(QPointF pos, Species * species, double energy, int spawningEnergy, int metabolism, const Movements & movements);
    ~Animal() = default;
    void initialize(QPointF pos, Species * species, double energy, int spawningEnergy, int metabolism, const Movements & movements, QPointF direction, const Animal *parent);

    QPointF movement(unsigned int friends, unsigned int enemies, const QPointF & curDirection) const;
    void calculateMovement();
    void executeMovement();

    bool tryClaimEaten();
    bool isEaten() const;
    void markAsEaten();

    void setPos(QPointF pos);
    const QPointF & pos() const;
    int cellX() const { return mCellX; }
    int cellY() const { return mCellY; }

    const QColor & color() const;
    const Movements & movements() const;

    double energy() const;
    void setEnergy(double energy);

    int metabolism() const;
    void setMetabolism(int metabolism);

    Species * species() const;

    int spawningEnergy() const;
    void setSpawningEnergy(int spawningEnergy);

    const Statistics & statistics() const;

    int listIndex() const { return mListIndex; }
    void setListIndex(int index) { mListIndex = index; }
    int cellSlot() const { return mCellSlot; }
    void setCellSlot(int slot) { mCellSlot = slot; }

protected:
    void killSelf();
    void spawnSelf();
    void syncCellFromPos();

private:
    Statistics mStatistics;
    QPointF mNextPos;
    QPointF mNextMovement;
    QPointF mPos;
    QPointF mDirection;
    QColor mColor;
    double mNextEnergyDiff = 0.0;
    std::atomic<bool> mNextEaten{false};
    double mEnergy = 0.0;
    int mMetabolism = 0;
    int mSpawningEnergy = 0;
    int mCellX = 0;
    int mCellY = 0;
    int mListIndex = -1;
    int mCellSlot = -1;
    uint mLastSpawnAge = 0;
    int mLastPlantEatAge = -ANIMAL_MINIMUM_PLANT_RATE;

    Species * mSpecies = nullptr;
    //[Friends][Enemies]
    Movements mMovements;
};

#endif // ANIMAL_H
