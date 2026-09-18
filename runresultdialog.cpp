#include "runresultdialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

RunResultDialog::RunResultDialog(const RunResult & result, QWidget * parent) :
    QDialog(parent)
{
    setWindowTitle(result.outcome == RunOutcome::Won ? tr("Victory!") : tr("Defeat"));
    setModal(true);
    buildUi(result);
}

void RunResultDialog::buildUi(const RunResult & result)
{
    auto * rootLayout = new QVBoxLayout(this);

    const bool won = (result.outcome == RunOutcome::Won);

    auto * headline = new QLabel(won ? tr("VICTORY") : tr("EXTINCTION"), this);
    QFont headlineFont = headline->font();
    headlineFont.setBold(true);
    headlineFont.setPointSize(headlineFont.pointSize() + 6);
    headline->setFont(headlineFont);
    // DB16 palette (see resources/theme.qss): #6daa2c green for a win,
    // #d04648 red for a loss - same swatches the run HUD's banner and
    // Laboratory::colorForIndex()'s plant color use, so "victory" and
    // "danger" read consistently across the whole app.
    headline->setStyleSheet(won ? QStringLiteral("color: #6daa2c;") : QStringLiteral("color: #d04648;"));
    headline->setAlignment(Qt::AlignCenter);
    rootLayout->addWidget(headline);

    auto * summary = new QLabel(
        tr("Epochs cleared: %1\nTicks survived: %2")
            .arg(result.epochsCleared)
            .arg(result.ticksSurvived),
        this);
    rootLayout->addWidget(summary);

    auto * table = new QTableWidget(result.speciesStats.size(), 4, this);
    table->setHorizontalHeaderLabels({tr("Species"), tr("Population"), tr("Top Gen"), tr("Children")});
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (int i = 0; i < result.speciesStats.size(); ++i)
    {
        const SpeciesRunStats & stats = result.speciesStats.at(i);
        table->setItem(i, 0, new QTableWidgetItem(stats.name));
        table->setItem(i, 1, new QTableWidgetItem(QString::number(stats.finalPopulation)));
        table->setItem(i, 2, new QTableWidgetItem(QString::number(stats.highestGeneration)));
        table->setItem(i, 3, new QTableWidgetItem(QString::number(stats.totalChildren)));
    }
    rootLayout->addWidget(table);

    auto * buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    rootLayout->addWidget(buttonBox);
}
