#include "speciesbuttonlayoutwidget.h"

#include <utility>
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
        if ( mSpecies.at(index)->type() == Species::typePlant )
        {
            continue;
        }
        const QString & name = mSpecies.at(index)->name();
        const QColor & color = mSpecies.at(index)->color();

        auto *button = new SpeciesButtonWidget(name, color, this);
        connect(mSpecies.at(index), SIGNAL(nameChanged(QString)),
                button, SLOT(setSpeciesName(QString)));
        connect(mSpecies.at(index), SIGNAL(colorChanged(QColor)),
                button, SLOT(setSpeciesColor(QColor)));
        connect(button, SIGNAL(clicked()), &mSignalMapper, SLOT(map()));
        mSignalMapper.setMapping(button, index);
        ui->buttonLayout->addWidget(button);
    }
}
