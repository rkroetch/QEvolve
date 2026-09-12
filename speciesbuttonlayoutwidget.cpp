#include "speciesbuttonlayoutwidget.h"

#include <utility>
#include <QCheckBox>
#include <QHBoxLayout>
#include "ui_speciesbuttonlayoutwidget.h"

#include "editspeciesdialog.h"
#include "speciesbuttonwidget.h"

SpeciesButtonLayoutWidget::SpeciesButtonLayoutWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SpeciesButtonLayoutWidget)
{
    ui->setupUi(this);

    connect(&mSignalMapper, &QSignalMapper::mappedInt,
            this, &SpeciesButtonLayoutWidget::handleSpeciesClicked);

}

SpeciesButtonLayoutWidget::~SpeciesButtonLayoutWidget()
{
    delete ui;
}

void SpeciesButtonLayoutWidget::setSpecies(QList<Species *> species)
{
    mSpecies = std::move(species);
    updateLayout();
}

void SpeciesButtonLayoutWidget::handleSpeciesClicked(int speciesIdx)
{
    if ( speciesIdx >= 0 && speciesIdx < mSpecies.size() )
    {
        EditSpeciesDialog dialog(mSpecies.at(speciesIdx));
        dialog.exec();
    }
}

void SpeciesButtonLayoutWidget::updateLayout()
{
    //Remove and delete all existing children of the widget
    QLayoutItem * child;
    while ((child = ui->buttonLayout->takeAt(0)) != nullptr)
    {
        delete child;
    }

    for ( int index = 0; index < mSpecies.size(); ++index )
    {
        Species * species = mSpecies.at(index);
        if ( species->type() == Species::typePlant )
        {
            continue;
        }
        const QString & name = species->name();
        const QColor & color = species->color();

        auto * checkBox = new QCheckBox(this);
        checkBox->setChecked(species->isActive());
        connect(checkBox, &QCheckBox::toggled, this, [this, species](bool checked) {
            emit speciesActiveToggled(species, checked);
        });

        auto *button = new SpeciesButtonWidget(name, color, this);
        connect(species, SIGNAL(nameChanged(QString)),
                button, SLOT(setSpeciesName(QString)));
        connect(species, SIGNAL(colorChanged(QColor)),
                button, SLOT(setSpeciesColor(QColor)));
        connect(button, SIGNAL(clicked()), &mSignalMapper, SLOT(map()));
        mSignalMapper.setMapping(button, index);

        auto * rowLayout = new QHBoxLayout();
        rowLayout->addWidget(checkBox);
        rowLayout->addWidget(button);
        ui->buttonLayout->addLayout(rowLayout);
    }
}
