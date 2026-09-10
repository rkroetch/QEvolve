#ifndef SPECIESBUTTONLAYOUTWIDGET_H
#define SPECIESBUTTONLAYOUTWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QSignalMapper>
#include <QList>
#include "species.h"

namespace Ui {
    class SpeciesButtonLayoutWidget;
}

class SpeciesButtonLayoutWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpeciesButtonLayoutWidget(QWidget *parent = 0);
    ~SpeciesButtonLayoutWidget();

    void setSpecies(QList<Species *> species);

private slots:
    void handleSpeciesClicked(int speciesIdx);

private:
    void updateLayout();

private:
    Ui::SpeciesButtonLayoutWidget *ui;
    QSignalMapper mSignalMapper;
    QList<Species *> mSpecies;
};

#endif // SPECIESBUTTONLAYOUTWIDGET_H
