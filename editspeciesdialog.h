#ifndef EDITSPECIESDIALOG_H
#define EDITSPECIESDIALOG_H

#include <QDialog>
#include <QSignalMapper>
#include "species.h"

namespace Ui {
    class EditSpeciesDialog;
}

class EditSpeciesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditSpeciesDialog(Species * species, QWidget *parent = 0);
    ~EditSpeciesDialog();

private slots:
    void handleColorClicked();
    void handleMovementClicked(int movement);
    void handleSaveClicked();
    void updateMovements();

private:
    void updateMovement(int friends, int enemies);
    QPixmap movementIcon(MovementDirections direction) const;
    QString movementToolTip(MovementDirections direction) const;

private:
    QSignalMapper mSignalMapper;
    Ui::EditSpeciesDialog *ui;
    Species * mSpecies;
    QPixmap mColorPixmap;
};

#endif // EDITSPECIESDIALOG_H
