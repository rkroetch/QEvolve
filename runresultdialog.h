#ifndef RUNRESULTDIALOG_H
#define RUNRESULTDIALOG_H

#include <QDialog>

#include "runstate.h"

// End-of-run victory/defeat screen (redesign plan section 2a, "Agent 4 -
// UI/UX & Game-Feel Engineer": "victory/defeat screens ... showing win/
// loss, epochs cleared, ticks survived, per-species stats from
// result.speciesStats"). Purely a display of a finished RunResult - it
// doesn't touch MetaProgression/Laboratory itself. The caller feeds the
// result into MetaProgression (via HubDialog::applyRunResult()) and
// reopens the hub after this dialog closes (see MainWindow::onRunEnded()).
class RunResultDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RunResultDialog(const RunResult & result, QWidget * parent = nullptr);

private:
    void buildUi(const RunResult & result);
};

#endif // RUNRESULTDIALOG_H
