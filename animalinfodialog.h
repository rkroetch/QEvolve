#ifndef ANIMALINFODIALOG_H
#define ANIMALINFODIALOG_H

#include <QDialog>
#include <QColor>
#include <QPixmap>
#include <QPointF>
#include <QString>
#include <QVector>
#include "animal.h"
#include "common.h"

namespace Ui {
    class AnimalInfoDialog;
}

// A read-only, point-in-time copy of the fields an AnimalInfoDialog needs.
// Captured while Laboratory holds positionLock for write (see
// Laboratory::handleCanvasClicked) so the dialog never reads an Animal's
// fields while the calculation thread is concurrently mutating them, and
// keeps working even if the underlying Animal is later killed/recycled.
struct AnimalSnapshot
{
    QString speciesName;
    // Species objects live for the lifetime of the app (unlike Animal, which
    // is pooled/recycled - see AnimalSnapshot's own comment above), so it's
    // safe to keep this pointer around for the dialog's lifetime to read the
    // species' baseline genome and mutation-rate settings, which only the
    // GUI thread ever writes.
    Species * species = nullptr;
    QColor color;
    QPointF pos;
    Movements movements;
    Animal::Statistics stats;
};

// Shows one animal (or plant) at a time from a set of snapshots - all the
// occupants Laboratory found in the clicked cell - with left/right arrows to
// step through them, and a toggle to compare an animal's current (possibly
// mutated) genome against its species' unmutated baseline.
class AnimalInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AnimalInfoDialog(const QVector<AnimalSnapshot> & snapshots, QWidget * parent = nullptr);
    ~AnimalInfoDialog();

private slots:
    void showPrevious();
    void showNext();
    void setShowBaseSpecies(bool showBaseSpecies);

private:
    void refreshDisplay();
    void updateMovement(int friends, int enemies, const Movements & movements);

private:
    Ui::AnimalInfoDialog * ui;
    QVector<AnimalSnapshot> mSnapshots;
    int mCurrentIndex = 0;
    bool mShowBaseSpecies = false;
    QPixmap mColorPixmap;
};

#endif // ANIMALINFODIALOG_H
