#include "animalinfodialog.h"
#include "ui_animalinfodialog.h"

#include "movementicons.h"
#include "species.h"

AnimalInfoDialog::AnimalInfoDialog(const QVector<AnimalSnapshot> & snapshots, QWidget * parent) :
    QDialog(parent),
    ui(new Ui::AnimalInfoDialog),
    mSnapshots(snapshots),
    mColorPixmap(32, 32)
{
    ui->setupUi(this);

    connect(ui->prevButton, &QToolButton::clicked, this, &AnimalInfoDialog::showPrevious);
    connect(ui->nextButton, &QToolButton::clicked, this, &AnimalInfoDialog::showNext);
    connect(ui->showBaseSpeciesCheckBox, &QCheckBox::toggled, this, &AnimalInfoDialog::setShowBaseSpecies);

    refreshDisplay();
}

AnimalInfoDialog::~AnimalInfoDialog()
{
    delete ui;
}

void AnimalInfoDialog::showPrevious()
{
    if ( mSnapshots.isEmpty() )
    {
        return;
    }
    mCurrentIndex = (mCurrentIndex - 1 + mSnapshots.size()) % mSnapshots.size();
    refreshDisplay();
}

void AnimalInfoDialog::showNext()
{
    if ( mSnapshots.isEmpty() )
    {
        return;
    }
    mCurrentIndex = (mCurrentIndex + 1) % mSnapshots.size();
    refreshDisplay();
}

void AnimalInfoDialog::setShowBaseSpecies(bool showBaseSpecies)
{
    mShowBaseSpecies = showBaseSpecies;
    refreshDisplay();
}

void AnimalInfoDialog::refreshDisplay()
{
    if ( mSnapshots.isEmpty() )
    {
        return;
    }

    const AnimalSnapshot & snapshot = mSnapshots.at(mCurrentIndex);
    const Species * species = snapshot.species;

    ui->prevButton->setEnabled(mSnapshots.size() > 1);
    ui->nextButton->setEnabled(mSnapshots.size() > 1);
    ui->indexLabel->setText(QString("%1 / %2").arg(mCurrentIndex + 1).arg(mSnapshots.size()));

    mColorPixmap.fill(snapshot.color);
    ui->colorIcon->setPixmap(mColorPixmap);

    ui->speciesName->setText(snapshot.speciesName);

    // The genome fields (movement grid, metabolism, spawn threshold) swap
    // between this animal's own, possibly-mutated values and its species'
    // unmutated baseline, so the user can compare a mutation to its origin.
    // Everything else below (Animal Info) is inherently per-animal and
    // always reflects the currently selected occupant regardless of the
    // toggle.
    if ( mShowBaseSpecies && species )
    {
        ui->metabolismValue->setText(QString("%1%").arg(species->metabolism()));
        ui->spawnThresholdValue->setText(QString::number(species->spawningEnergy()));
        updateMovement(0, 0, species->movements());
        updateMovement(0, 1, species->movements());
        updateMovement(0, 2, species->movements());
        updateMovement(1, 0, species->movements());
        updateMovement(1, 1, species->movements());
        updateMovement(1, 2, species->movements());
        updateMovement(2, 0, species->movements());
        updateMovement(2, 1, species->movements());
        updateMovement(2, 2, species->movements());
    }
    else
    {
        ui->metabolismValue->setText(QString("%1%").arg(snapshot.stats.mMetabolism));
        ui->spawnThresholdValue->setText(QString::number(snapshot.stats.mSpawningEnergy));
        updateMovement(0, 0, snapshot.movements);
        updateMovement(0, 1, snapshot.movements);
        updateMovement(0, 2, snapshot.movements);
        updateMovement(1, 0, snapshot.movements);
        updateMovement(1, 1, snapshot.movements);
        updateMovement(1, 2, snapshot.movements);
        updateMovement(2, 0, snapshot.movements);
        updateMovement(2, 1, snapshot.movements);
        updateMovement(2, 2, snapshot.movements);
    }

    if ( species )
    {
        ui->movementMutationValue->setText(QString("%1%").arg(species->movementMutation()));
        ui->metabolismMutationValue->setText(QString("%1%").arg(species->metabolismMutation()));
        ui->spawnThresholdMutationValue->setText(QString("%1%").arg(species->spawningEnergyMutation()));
    }

    ui->ageValue->setText(QString::number(snapshot.stats.mAge));
    ui->generationValue->setText(QString::number(snapshot.stats.mGeneration));
    ui->energyValue->setText(QString::number(snapshot.stats.mEnergy, 'f', 1));
    ui->childrenValue->setText(QString::number(snapshot.stats.mNumChildren));
    ui->friendsValue->setText(QString::number(snapshot.stats.mNumFriends));
    ui->enemiesValue->setText(QString::number(snapshot.stats.mNumEnemies));
    ui->movementMutationsValue->setText(QString::number(snapshot.stats.mNumMutationsMovement));
    ui->metabolismMutationsValue->setText(QString::number(snapshot.stats.mNumMutationsMetabolism));
    ui->spawnThresholdMutationsValue->setText(QString::number(snapshot.stats.mNumMutationsSpawningEnergy));
    ui->positionValue->setText(QString("(%1, %2)").arg(int(snapshot.pos.x())).arg(int(snapshot.pos.y())));
}

void AnimalInfoDialog::updateMovement(int friends, int enemies, const Movements & movements)
{
    QPushButton * button = nullptr;

    switch ( friends )
    {
    case 0:
        switch ( enemies )
        {
        case 0:
            button = ui->movement00;
            break;
        case 1:
            button = ui->movement01;
            break;
        case 2:
        default:
            button = ui->movement02;
            break;
        }
        break;
    case 1:
        switch ( enemies )
        {
        case 0:
            button = ui->movement10;
            break;
        case 1:
            button = ui->movement11;
            break;
        case 2:
        default:
            button = ui->movement12;
            break;
        }
        break;
    case 2:
    default:
        switch ( enemies )
        {
        case 0:
            button = ui->movement20;
            break;
        case 1:
            button = ui->movement21;
            break;
        case 2:
        default:
            button = ui->movement22;
            break;
        }
        break;
    }

    MovementDirections direction = movements.getMovement(friends, enemies);
    button->setIcon(movementIcon(direction));
    button->setToolTip(movementToolTip(direction));
}
