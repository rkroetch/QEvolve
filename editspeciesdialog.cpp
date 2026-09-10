#include "editspeciesdialog.h"
#include "ui_editspeciesdialog.h"
#include <QColorDialog>
#include <QFileDialog>
#include <QtGlobal>

#include "common.h"

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

QPixmap EditSpeciesDialog::movementIcon(MovementDirections direction) const
{
    QPixmap pixmap;
    switch (direction)
    {
    case MoveUp:
        pixmap = QPixmap(":actions/resources/actions/arrow_up.png");
        break;
    case MoveUpRight:
        pixmap = QPixmap(":actions/resources/actions/arrow_right_up.png");
        break;
    case MoveRight:
        pixmap = QPixmap(":actions/resources/actions/arrow_right.png");
        break;
    case MoveDownRight:
        pixmap = QPixmap(":actions/resources/actions/arrow_right_down.png");
        break;
    case MoveDown:
        pixmap = QPixmap(":actions/resources/actions/arrow_down.png");
        break;
    case MoveDownLeft:
        pixmap = QPixmap(":actions/resources/actions/arrow_left_down.png");
        break;
    case MoveLeft:
        pixmap = QPixmap(":actions/resources/actions/arrow_left.png");
        break;
    case MoveUpLeft:
        pixmap = QPixmap(":actions/resources/actions/arrow_left_up.png");
        break;
    case MoveRandom:
        pixmap = QPixmap(":actions/resources/actions/arrow_refresh_small.png");
        break;
    case MoveStop:
        pixmap = QPixmap(":actions/resources/actions/stop.png");
        break;
    case MoveGo:
        pixmap = QPixmap(":actions/resources/actions/arrow_out.png");
        break;
    case MoveTurnAround:
        pixmap = QPixmap(":actions/resources/actions/arrow_undo.png");
        break;
    case MoveTurnRight:
        pixmap = QPixmap(":actions/resources/actions/arrow_turn_right.png");
        break;
    case MoveTurnLeft:
        pixmap = QPixmap(":actions/resources/actions/arrow_turn_left.png");
        break;
    case MoveMerge:
        pixmap = QPixmap(":actions/resources/actions/arrow_merge.png");
        break;
    case MoveSplit:
        pixmap = QPixmap(":actions/resources/actions/arrow_branch.png");
        break;
    default:
        pixmap = QPixmap();
        break;
    }

    return pixmap;
}

QString EditSpeciesDialog::movementToolTip(MovementDirections direction) const
{
    QString toolTip;
    switch (direction)
    {
    case MoveMerge:
        toolTip = QString("Merge");
        break;
    case MoveSplit:
        toolTip = QString("Split");
        break;
    case MoveUp:
        toolTip = QString("Up");
        break;
    case MoveUpRight:
        toolTip = QString("Up Right");
        break;
    case MoveRight:
        toolTip = QString("Right");
        break;
    case MoveDownRight:
        toolTip = QString("Down Right");
        break;
    case MoveDown:
        toolTip = QString("Down");
        break;
    case MoveDownLeft:
        toolTip = QString("Down Left");
        break;
    case MoveLeft:
        toolTip = QString("Left");
        break;
    case MoveUpLeft:
        toolTip = QString("Up Left");
        break;
    case MoveRandom:
        toolTip = QString("Random");
        break;
    case MoveStop:
        toolTip = QString("Stop");
        break;
    case MoveGo:
        toolTip = QString("Go");
        break;
    case MoveTurnAround:
        toolTip = QString("Turn Around");
        break;
    case MoveTurnRight:
        toolTip = QString("Turn Right");
        break;
    case MoveTurnLeft:
        toolTip = QString("Turn Left");
        break;
    default:
        toolTip = QString("");
        break;
    }

    return toolTip;
}
