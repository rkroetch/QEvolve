#include "editspeciesdialog.h"
#include "ui_editspeciesdialog.h"
#include <QColorDialog>
#include <QFileDialog>
#include <QtGlobal>

#include "common.h"
#include "movementicons.h"

EditSpeciesDialog::EditSpeciesDialog(Species * species, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditSpeciesDialog),
    mColorPixmap(32, 32)
{
    ui->setupUi(this);
    mSpecies = species;

    mColorPixmap.fill(mSpecies->color());
    ui->colorIcon->setPixmap(mColorPixmap);

    ui->speciesName->setText(species->name());
    connect(ui->speciesName, &QLineEdit::textChanged,
            species, &Species::setName);

    ui->metabolism->setValue(species->metabolism());
    connect(ui->metabolism, QOverload<int>::of(&QSpinBox::valueChanged),
          [=](int d){ species->setMetabolism(d); });

    ui->metabolismMutation->setValue(species->metabolismMutation());
    connect(ui->metabolismMutation, QOverload<int>::of(&QSpinBox::valueChanged),
          [=](int d){ species->setMetabolismMutation(d); });

    ui->spawnThreshold->setValue(species->spawningEnergy());
    connect(ui->spawnThreshold, QOverload<int>::of(&QSpinBox::valueChanged),
          [=](int d){ species->setSpawningEnergy(d); });

    ui->spawnThresholdMutation->setValue(species->spawningEnergyMutation());
    connect(ui->spawnThresholdMutation, QOverload<int>::of(&QSpinBox::valueChanged),
          [=](int d){ species->setSpawningEnergyMutation(d); });

    ui->movementMutation->setValue(species->movementMutation());
    connect(ui->movementMutation, QOverload<int>::of(&QSpinBox::valueChanged),
          [=](int d){ species->setMovementMutation(d); });

    connect(ui->movement00, SIGNAL(clicked()), &mSignalMapper, SLOT(map()));
    mSignalMapper.setMapping(ui->movement00, 00);
    connect(ui->movement01, SIGNAL(clicked()), &mSignalMapper, SLOT(map()));
    mSignalMapper.setMapping(ui->movement01, 01);
    connect(ui->movement02, SIGNAL(clicked()), &mSignalMapper, SLOT(map()));
    mSignalMapper.setMapping(ui->movement02, 02);

    connect(ui->movement10, SIGNAL(clicked()), &mSignalMapper, SLOT(map()));
    mSignalMapper.setMapping(ui->movement10, 10);
    connect(ui->movement11, SIGNAL(clicked()), &mSignalMapper, SLOT(map()));
    mSignalMapper.setMapping(ui->movement11, 11);
    connect(ui->movement12, SIGNAL(clicked()), &mSignalMapper, SLOT(map()));
    mSignalMapper.setMapping(ui->movement12, 12);

    connect(ui->movement20, SIGNAL(clicked()), &mSignalMapper, SLOT(map()));
    mSignalMapper.setMapping(ui->movement20, 20);
    connect(ui->movement21, SIGNAL(clicked()), &mSignalMapper, SLOT(map()));
    mSignalMapper.setMapping(ui->movement21, 21);
    connect(ui->movement22, SIGNAL(clicked()), &mSignalMapper, SLOT(map()));
    mSignalMapper.setMapping(ui->movement22, 22);

    connect(&mSignalMapper, SIGNAL(mappedInt(int)),
            this, SLOT(handleMovementClicked(int)));

    updateMovements();

    connect(ui->colorPickerButton, SIGNAL(clicked()),
            this, SLOT(handleColorClicked()));
    connect(ui->saveButton, SIGNAL(clicked()),
            this, SLOT(handleSaveClicked()));
}

EditSpeciesDialog::~EditSpeciesDialog()
{
    delete ui;
}

void EditSpeciesDialog::handleColorClicked()
{
    if ( mSpecies )
    {
        QColor newColor = QColorDialog::getColor(mSpecies->color());
        if ( newColor.isValid() )
        {
            mSpecies->setColor(newColor);

            mColorPixmap.fill(mSpecies->color());
            ui->colorIcon->setPixmap(mColorPixmap);
        }
    }
}

void EditSpeciesDialog::handleMovementClicked(int movement)
{
    if ( mSpecies )
    {
        switch ( movement )
        {
        case 00:
            mSpecies->toggleMovement(0, 0);
            break;
        case 01:
            mSpecies->toggleMovement(0, 1);
            break;
        case 02:
            mSpecies->toggleMovement(0, 2);
            break;

        case 10:
            mSpecies->toggleMovement(1, 0);
            break;
        case 11:
            mSpecies->toggleMovement(1, 1);
            break;
        case 12:
            mSpecies->toggleMovement(1, 2);
            break;

        case 20:
            mSpecies->toggleMovement(2, 0);
            break;
        case 21:
            mSpecies->toggleMovement(2, 1);
            break;
        case 22:
            mSpecies->toggleMovement(2, 2);
            break;
        default:
            break;
        }
        updateMovements();
    }
}

void EditSpeciesDialog::handleSaveClicked()
{
    QString initialFile("species/%1.SPC");
    initialFile = initialFile.arg(ui->speciesName->text().toLower());

    QString filename = QFileDialog::getSaveFileName(this, "Save Species", initialFile, "Species (*.SPC)");
    if ( !filename.isEmpty() )
    {
        mSpecies->save(filename);
    }
}

void EditSpeciesDialog::updateMovements()
{
    if ( mSpecies )
    {
        updateMovement(0, 0);
        updateMovement(0, 1);
        updateMovement(0, 2);

        updateMovement(1, 0);
        updateMovement(1, 1);
        updateMovement(1, 2);

        updateMovement(2, 0);
        updateMovement(2, 1);
        updateMovement(2, 2);
    }
}

void EditSpeciesDialog::updateMovement(int friends, int enemies)
{
    if ( mSpecies )
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

        MovementDirections direction = mSpecies->movements().getMovement(friends, enemies);
        button->setIcon(movementIcon(direction));
        button->setToolTip(movementToolTip(direction));
    }
}

