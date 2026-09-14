#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QAction>
#include <QFont>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QVBoxLayout>

#include "difficultycurve.h"
#include "hubdialog.h"
#include "mutationchoice.h"
#include "mutationchoicedialog.h"
#include "runresultdialog.h"
#include "species.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    mSettings("ryank", "Evolve", this),
    mMeta(this)
{
    ui->setupUi(this);

    connect(ui->speedSlider, &QSlider::valueChanged,
            ui->laboratory, &Laboratory::setSpeed);

    connect(ui->actionStart, &QAction::triggered,
            this, &MainWindow::toggleStart);
    connect(ui->actionReset, &QAction::triggered,
            this, &MainWindow::reset);

    auto plantPatternSignalMapper = new QSignalMapper(this);
    connect(ui->actionPlantOneGroup, &QAction::triggered, plantPatternSignalMapper, qOverload<>(&QSignalMapper::map));
    plantPatternSignalMapper->setMapping(ui->actionPlantOneGroup, plantPatternOneGroup);
    connect(ui->actionPlantTwoGroups, &QAction::triggered, plantPatternSignalMapper, qOverload<>(&QSignalMapper::map));
    plantPatternSignalMapper->setMapping(ui->actionPlantTwoGroups, plantPatternTwoGroups);
    connect(ui->actionPlantRandom, &QAction::triggered, plantPatternSignalMapper, qOverload<>(&QSignalMapper::map));
    plantPatternSignalMapper->setMapping(ui->actionPlantRandom, plantPatternRandom);
    connect(plantPatternSignalMapper, &QSignalMapper::mappedInt, ui->laboratory, &Laboratory::setPlantPattern);
    connect(ui->laboratory, &Laboratory::plantPatternChanged, this, &MainWindow::onPlantPatternChanged);


    auto plantPatternGroup = new QActionGroup(this);
    plantPatternGroup->addAction(ui->actionPlantOneGroup);
    plantPatternGroup->addAction(ui->actionPlantTwoGroups);
    plantPatternGroup->addAction(ui->actionPlantRandom);
    plantPatternGroup->setExclusive(true);

    ui->statusBar->addPermanentWidget(&mNumAnimalsLabel);

    mNumAnimalsTimer.setInterval(250);
    mNumAnimalsTimer.setSingleShot(false);
    connect(&mNumAnimalsTimer, SIGNAL(timeout()),
            this, SLOT(updateStatusBar()));
    mNumAnimalsTimer.start();

    ui->textBrowser->setVisible(ui->actionShowStatistics->isChecked());
    ui->chartView->setVisible(ui->actionShowGraph->isChecked());
    connect(ui->actionShowStatistics, &QAction::toggled, ui->textBrowser, &QWidget::setVisible);
    connect(ui->actionShowGraph, &QAction::toggled, ui->chartView, &QWidget::setVisible);

    ui->laboratory->setSpeed(ui->speedSlider->value());

    ui->speciesButtons->setSpecies(ui->laboratory->species());
    connect(ui->speciesButtons, &SpeciesButtonLayoutWidget::speciesActiveToggled,
            ui->laboratory, &Laboratory::setSpeciesActive);

    mChart.legend()->hide();
    mChart.addSeries(&mSeries);
    mChart.createDefaultAxes();
    mChart.setTitle("calculations / second");

    ui->chartView->setRenderHints(QPainter::Antialiasing);
    ui->chartView->setChart(&mChart);

    // --- Roguelike run wiring (Phase 2: UI/Game-Feel Engineer) -----------
    mHubDialog = new HubDialog(&mMeta, this);
    connect(mHubDialog, &HubDialog::startRunRequested, this, &MainWindow::onStartRunRequested);
    connect(mHubDialog, &HubDialog::closedWithoutStartingRun, this, &MainWindow::onHubClosedWithoutRun);

    connect(ui->laboratory, &Laboratory::epochAdvanced, this, &MainWindow::onEpochAdvanced);
    connect(ui->laboratory, &Laboratory::runEnded, this, &MainWindow::onRunEnded);

    mActionNewRun = new QAction(tr("New Run..."), this);
    mActionNewRun->setToolTip(tr("Open the Evolution Hub to spend Evolution Points and start a bounded run"));
    connect(mActionNewRun, &QAction::triggered, this, &MainWindow::showHub);
    ui->menuRun->insertAction(ui->actionStart, mActionNewRun);
    ui->menuRun->insertSeparator(ui->actionStart);
    ui->mainToolBar->addAction(mActionNewRun);

    mRunHudLabel = new QLabel(this);
    mRunHudLabel->setVisible(false);
    mRunHudLabel->setAlignment(Qt::AlignCenter);
    QFont hudFont = mRunHudLabel->font();
    hudFont.setBold(true);
    hudFont.setPointSize(hudFont.pointSize() + 1);
    mRunHudLabel->setFont(hudFont);
    mRunHudLabel->setStyleSheet(QStringLiteral("padding: 4px; background-color: rgba(0, 0, 0, 40);"));
    if (auto * rootLayout = qobject_cast<QVBoxLayout *>(ui->centralWidget->layout()))
    {
        rootLayout->insertWidget(0, mRunHudLabel);
    }

    updateRunHud();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateStatusBar()
{
    const int old_scrollbar_value = ui->textBrowser->verticalScrollBar()->value();

    const int numAnimals = ui->laboratory->numAnimals();
    qreal calculationsPerSecond = numAnimals * ui->laboratory->cyclesPerSecond();
    mNumAnimalsLabel.setText(QString::number(numAnimals) + "-" + QString::number(calculationsPerSecond, 'f', 2));
    ui->textBrowser->setText(ui->laboratory->statistics());
    ui->textBrowser->verticalScrollBar()->setValue(old_scrollbar_value);

    static qreal x = 0.0;
    static qreal maxY = 1000.0;
    mSeries.append(x, calculationsPerSecond);
    x += 0.25;
    maxY = qMax(maxY, calculationsPerSecond);

    for ( auto axis : mSeries.attachedAxes() )
    {
        if ( axis->orientation() == Qt::Horizontal )
        {
            axis->setRange(0.0, (x - fmod(x, 60)) + 60.0);
        }
        else if ( axis->orientation() == Qt::Vertical )
        {
            axis->setRange(0.0, (maxY - fmod(maxY, 1000)) + 1000.0);
        }
    }

    updateRunHud();
}

void MainWindow::toggleStart()
{
    ui->laboratory->toggleStart();
}

void MainWindow::reset()
{
    ui->laboratory->reset();
    updateRunHud();
}

void MainWindow::onPlantPatternChanged(PlantPattern pattern)
{
    switch (pattern)
    {
    case plantPatternOneGroup:
        ui->actionPlantOneGroup->setChecked(true);
        break;
    case plantPatternTwoGroups:
        ui->actionPlantTwoGroups->setChecked(true);
        break;
    case plantPatternRandom:
        ui->actionPlantRandom->setChecked(true);
        break;
    }
}

void MainWindow::showHub()
{
    mHubDialog->setAvailableSpecies(ui->laboratory->species());
    mHubDialog->exec();
}

void MainWindow::onStartRunRequested(RunConfig config)
{
    // Stop any in-flight sandbox/run simulation before mutating the chosen
    // species' stats below - CalculationThread may otherwise be reading
    // metabolism()/spawningEnergy() concurrently while spawning animals.
    ui->laboratory->stop();

    applyMetaBonuses(config);

    const int clearedRunCount = mSettings.value("Meta/ClearedRunCount", 0).toInt();
    config.metaTier = computeMetaTier(clearedRunCount).tier;

    ui->laboratory->beginRun(config);
    updateRunHud();
}

void MainWindow::onHubClosedWithoutRun()
{
    // Nothing else to do: HubDialog already hides/closes itself on reject,
    // and the sandbox (or an already-in-progress run) simply continues.
}

void MainWindow::onEpochAdvanced(int epoch)
{
    if (!ui->laboratory->isRunModeActive())
    {
        return;
    }

    // Pause immediately: a chosen mutation is applied directly to the
    // player's Species fields via public setters, which is only safe once
    // CalculationThread has actually stopped (see Laboratory::stop()).
    ui->laboratory->stop();

    Species * player = ui->laboratory->playerSpecies();
    const bool playerExtinct = (player == nullptr) || player->animals().isEmpty();
    // Mirrors Laboratory::updateRunState()'s own win/lose check exactly, so
    // this predicts whether the epoch that just fired will also end the
    // run (in which case Laboratory itself will call finishRun()/emit
    // runEnded() right after this slot returns) - skip the mutation picker
    // in that case so it never appears on top of / right before the
    // victory/defeat screen.
    const RunOutcome predictedOutcome = evaluateRunOutcome(playerExtinct, epoch, ui->laboratory->targetEpochs());

    if (predictedOutcome == RunOutcome::InProgress && player != nullptr)
    {
        MutationChoiceContext context;
        context.movements = player->movements();
        context.metabolism = player->metabolism();
        context.spawningEnergy = player->spawningEnergy();

        const quint32 seed = quint32(QRandomGenerator::global()->generate());
        const QVector<MutationChoice> choices = generateMutationChoices(context, 3, seed);

        MutationChoiceDialog dialog(choices, this);
        const int execResult = dialog.exec();
        if (execResult == QDialog::Accepted && dialog.selectedIndex() >= 0)
        {
            const MutationChoice & choice = choices.at(dialog.selectedIndex());
            switch (choice.kind)
            {
            case MutationChoiceKind::MovementTweak:
                player->setMovement(choice.moveFriends, choice.moveEnemies, choice.moveDirection);
                break;
            case MutationChoiceKind::MetabolismShift:
                player->setMetabolism(qMax(1, int(player->metabolism() * (1.0 + choice.statDeltaFraction))));
                break;
            case MutationChoiceKind::SpawningEnergyShift:
                player->setSpawningEnergy(qMax(1, int(player->spawningEnergy() * (1.0 + choice.statDeltaFraction))));
                break;
            }
        }
    }

    // Only resume here if the run is still actually in progress. If our
    // prediction above says otherwise (extinct, or past the target epoch),
    // leave the lab stopped - the enclosing Laboratory::updateRunState()
    // call that triggered this signal will finish evaluating the outcome
    // and call finishRun()/emit runEnded() right after this slot returns.
    //
    // Resuming is deferred to the next event-loop iteration (rather than
    // calling toggleStart() directly here) because this slot runs
    // synchronously inside Laboratory::updateRunState()'s emit
    // epochAdvanced() call, with more positionLock-touching work
    // (advanceHazards()/applyEncounterSpec(), and potentially
    // buildRunResult() on a later tick) still to run in that same function
    // after this slot returns. Restarting CalculationThread immediately
    // would let it run concurrently with that remaining work - the same
    // cross-thread positionLock contention documented on
    // Laboratory::mCachedPlayerAnimalCount that used to crash the app.
    if (predictedOutcome == RunOutcome::InProgress)
    {
        QMetaObject::invokeMethod(ui->laboratory, [this]() { ui->laboratory->toggleStart(); }, Qt::QueuedConnection);
    }

    updateRunHud();
}

void MainWindow::onRunEnded(RunResult result)
{
    if (result.outcome == RunOutcome::Won)
    {
        const int clearedRunCount = mSettings.value("Meta/ClearedRunCount", 0).toInt();
        mSettings.setValue("Meta/ClearedRunCount", clearedRunCount + 1);
    }

    updateRunHud();

    RunResultDialog outcomeDialog(result, this);
    outcomeDialog.exec();

    mHubDialog->applyRunResult(result);
    showHub();
}

void MainWindow::applyMetaBonuses(RunConfig & config) const
{
    Species * player = config.playerSpecies;
    if (player == nullptr)
    {
        return;
    }

    const double metabolismBonus = mMeta.metabolismBonus();
    const double spawningEnergyBonus = mMeta.spawningEnergyBonus();
    const int mutationBonusPoints = qRound(mMeta.mutationRateBonus() * 100.0);

    player->setMetabolism(qMax(1, int(player->metabolism() * (1.0 - metabolismBonus))));
    player->setSpawningEnergy(qMax(1, int(player->spawningEnergy() * (1.0 - spawningEnergyBonus))));

    player->setMovementMutation(qBound(0, player->movementMutation() + mutationBonusPoints, 100));
    player->setMetabolismMutation(qBound(0, player->metabolismMutation() + mutationBonusPoints, 100));
    player->setSpawningEnergyMutation(qBound(0, player->spawningEnergyMutation() + mutationBonusPoints, 100));
}

void MainWindow::updateRunHud()
{
    const bool active = ui->laboratory->isRunModeActive();
    mRunHudLabel->setVisible(active);
    mActionNewRun->setEnabled(!active);

    if (!active)
    {
        return;
    }

    Species * player = ui->laboratory->playerSpecies();
    const QString speciesName = (player != nullptr) ? player->name() : tr("Unknown");
    const int population = (player != nullptr) ? playerPopulationFromStatistics(speciesName) : 0;

    QString text = tr("RUN — Epoch %1 / %2  |  %3 population: %4")
        .arg(ui->laboratory->currentEpoch())
        .arg(ui->laboratory->targetEpochs())
        .arg(speciesName)
        .arg(population);

    switch (ui->laboratory->runOutcome())
    {
    case RunOutcome::Won:
        text += tr("  |  VICTORY");
        mRunHudLabel->setStyleSheet(QStringLiteral("padding: 4px; background-color: rgba(46, 139, 87, 90);"));
        break;
    case RunOutcome::Lost:
        text += tr("  |  DEFEAT");
        mRunHudLabel->setStyleSheet(QStringLiteral("padding: 4px; background-color: rgba(178, 34, 34, 90);"));
        break;
    case RunOutcome::InProgress:
    default:
        mRunHudLabel->setStyleSheet(QStringLiteral("padding: 4px; background-color: rgba(0, 0, 0, 40);"));
        break;
    }

    mRunHudLabel->setText(text);
}

int MainWindow::playerPopulationFromStatistics(const QString & speciesName) const
{
    const QString stats = ui->laboratory->statistics();
    const QString needle = speciesName + QStringLiteral(" (");
    const int nameIndex = stats.indexOf(needle);
    if (nameIndex < 0)
    {
        return 0;
    }
    const int numberStart = nameIndex + needle.size();
    const int numberEnd = stats.indexOf(QLatin1Char(')'), numberStart);
    if (numberEnd < 0)
    {
        return 0;
    }
    bool ok = false;
    const int population = stats.mid(numberStart, numberEnd - numberStart).toInt(&ok);
    return ok ? population : 0;
}
