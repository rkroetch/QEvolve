#ifndef MUTATIONCHOICEDIALOG_H
#define MUTATIONCHOICEDIALOG_H

#include <QDialog>
#include <QVector>

#include "mutationchoice.h"

// Player-facing epoch mutation-choice picker (redesign plan section 2a,
// "Agent 4 - UI/UX & Game-Feel Engineer"): presents 2-3 choice-limited
// genome tweaks (see mutationchoice.h) and lets the player pick at most one
// by clicking its "Choose" button, which immediately accepts the dialog.
// Reuses movementIcon()/movementToolTip() (see movementicons.h) for
// MovementTweak choices - the same icon set EditSpeciesDialog uses for its
// freeform 3x3 action-gene table, just choice-limited here instead of
// freeform.
//
// Deliberately dumb: it never touches Species itself. The caller reads
// selectedIndex() after exec() and applies the chosen MutationChoice to
// Laboratory::playerSpecies() via that class's public setters (see
// MainWindow::onEpochAdvanced()).
class MutationChoiceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MutationChoiceDialog(const QVector<MutationChoice> & choices, QWidget * parent = nullptr);

    // -1 if the dialog was closed/rejected (e.g. Skip, Esc, window X)
    // without picking anything.
    int selectedIndex() const { return mSelectedIndex; }

private:
    void buildUi(const QVector<MutationChoice> & choices);
    void choose(int index);

    int mSelectedIndex = -1;
};

#endif // MUTATIONCHOICEDIALOG_H
