#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QAction>
#include <QBrush>
#include <QEvent>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "difficultycurve.h"
#include "hubdialog.h"
#include "mutationchoice.h"
#include "mutationchoicedialog.h"
#include "runresultdialog.h"
#include "settingsdialog.h"
#include "species.h"

namespace {

// DB16 palette swatches used by the run HUD/chart theming below - kept in
// sync by hand with resources/theme.qss (Qt style sheets can't restyle
// QChart internals, so its DB16 colors are applied directly in code here
// instead of via QSS).
const QColor kDb16NearBlack(0x14, 0x0c, 0x1c);
const QColor kDb16Maroon(0x44, 0x24, 0x34);
const QColor kDb16Indigo(0x30, 0x34, 0x6d);
const QColor kDb16Steel(0x85, 0x95, 0xa1);
const QColor kDb16Red(0xd0, 0x46, 0x48);
const QColor kDb16Green(0x6d, 0xaa, 0x2c);
const QColor kDb16Cyan(0x6d, 0xc2, 0xca);
const QColor kDb16LightGray(0xd2, 0xd2, 0xd2);
const QColor kDb16White(0xff, 0xff, 0xff);

// Run-HUD banner styles (flat DB16 fills, 2px pixel-bevel bottom border -
// no rounded corners/gradients, matching resources/theme.qss's chrome
// styling). Kept as raw QSS strings (rather than theme.qss selectors)
// since mRunHudLabel is a plain, unnamed QLabel created in code and its
// background needs to swap per run outcome at runtime.
const QString kRunHudInProgressStyle = QStringLiteral(
    "padding: 4px; background-color: #30346d; color: #d2d2d2; "
    "border-bottom: 2px solid #8595a1;");
const QString kRunHudWonStyle = QStringLiteral(
    "padding: 4px; background-color: #6daa2c; color: #140c1c; "
    "border-bottom: 2px solid #ffffff;");
const QString kRunHudLostStyle = QStringLiteral(
    "padding: 4px; background-color: #d04648; color: #ffffff; "
    "border-bottom: 2px solid #140c1c;");

// Renders `filled` out of `total` blocks as a pixel-bar using box-drawing
// characters, e.g. threatBar(3, 5) == "[###..]" - a cheap, monospace-font-
// friendly stand-in for a graphical meter that reads as "threat rising"
// without pulling in any new widget. Purely a display of the epoch/
// targetEpochs ratio Laboratory already exposes (read-only), not a new
// piece of run state.
QString threatBar(int filled, int total)
{
    total = qMax(1, total);
    filled = qBound(0, filled, total);
    QString bar = QStringLiteral("[");
    bar += QString(filled, QLatin1Char('#'));
    bar += QString(total - filled, QLatin1Char('.'));
    bar += QStringLiteral("]");
    return bar;
}

// Reads a species' population out of an already-fetched
// Laboratory::statistics() string, via its cached "<Name> (<count>)"
// per-species header line (see Species::statistics()) - shared by
// MainWindow::playerPopulationFromStatistics() and the per-species
// population chart, both of which must avoid touching Species::animals()
// directly from the GUI thread while CalculationThread may be running.
int populationFromStats(const QString & stats, const QString & speciesName)
{
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

// Draws a small flat, blocky cog/gear glyph for the settings-overlay button
// - a QPainter shape rather than a Unicode "gear" character (U+2699), since
// glyph coverage for that codepoint isn't guaranteed in every installed
// font (notably not in Consolas, this app's default).
QPixmap buildGearIcon(const QColor & color, int size)
{
    // A plain QPixmap's default format isn't guaranteed to carry an alpha
    // channel on every platform, which silently breaks both fill(transparent)
    // and CompositionMode_Clear below - QImage with an explicit ARGB format
    // does, so draw there and convert once at the end.
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);

    const qreal center = size / 2.0;
    const qreal outerR = size * 0.40;
    const qreal innerR = size * 0.22;
    const qreal toothLen = size * 0.16;
    const qreal toothW = size * 0.18;

    for (int i = 0; i < 8; ++i)
    {
        painter.save();
        painter.translate(center, center);
        painter.rotate(45.0 * i);
        painter.drawRect(QRectF(-toothW / 2.0, -(outerR + toothLen), toothW, toothLen));
        painter.restore();
    }

    painter.drawEllipse(QPointF(center, center), outerR, outerR);

    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.drawEllipse(QPointF(center, center), innerR, innerR);
    painter.end();

    return QPixmap::fromImage(image);
}

} // namespace

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
    ui->populationChartView->setVisible(ui->actionShowSpeciesPopulation->isChecked());
    connect(ui->actionShowStatistics, &QAction::toggled, ui->textBrowser, &QWidget::setVisible);
    connect(ui->actionShowGraph, &QAction::toggled, ui->chartView, &QWidget::setVisible);
    connect(ui->actionShowSpeciesPopulation, &QAction::toggled, ui->populationChartView, &QWidget::setVisible);

    // Only ever scroll speciesScrollArea vertically - horizontal space is
    // always handed to widgetResizable() to match the viewport exactly, so
    // the species buttons stay full-width rather than being reachable via a
    // horizontal scrollbar.
    ui->speciesScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Give the laboratory view the lion's share of space by default when an
    // optional statistics/chart/population panel is toggled on, while still
    // leaving every divider freely draggable afterwards.
    ui->topSplitter->setStretchFactor(0, 0); // textBrowser
    ui->topSplitter->setStretchFactor(1, 4); // laboratoryScrollArea
    ui->topSplitter->setStretchFactor(2, 0); // speciesScrollArea

    // Wrapping speciesButtons in a QScrollArea (so it can scroll instead of
    // forcing a tall minimum height that would stop mainSplitter from
    // shrinking the top row) means it no longer reports a content-derived
    // sizeHint, so topSplitter's initial layout would otherwise squeeze it
    // down to its bare minimumSize. Give it an explicit starting width wide
    // enough for the longest species names.
    ui->topSplitter->setSizes(QList<int>{0, 4000, 240});

    ui->mainSplitter->setStretchFactor(0, 4); // topSplitter
    ui->mainSplitter->setStretchFactor(1, 1); // chartView
    ui->mainSplitter->setStretchFactor(2, 1); // populationChartView

    ui->laboratory->setSpeed(ui->speedSlider->value());

    ui->speciesButtons->setSpecies(ui->laboratory->species());
    connect(ui->speciesButtons, &SpeciesButtonLayoutWidget::speciesActiveToggled,
            ui->laboratory, &Laboratory::setSpeciesActive);

    mChart.legend()->hide();
    mChart.addSeries(&mSeries);
    mChart.createDefaultAxes();
    mChart.setTitle("calculations / second");

    // --- Pixel-art presentation pass: DB16-theme the chart -----------------
    // QChart/QAbstractAxis aren't reachable through the app-wide QSS (see
    // resources/theme.qss), so the same DB16 swatches are applied directly
    // here instead. Antialiasing is deliberately left on for this widget
    // (unlike laboratory.cpp's simulation canvas) since it's rendering thin
    // chart lines and small axis text, not the game's sprites.
    mChart.setBackgroundBrush(QBrush(kDb16NearBlack));
    mChart.setBackgroundRoundness(0);
    mChart.setPlotAreaBackgroundBrush(QBrush(kDb16NearBlack));
    mChart.setPlotAreaBackgroundVisible(true);
    QFont chartTitleFont(QStringLiteral("Consolas"));
    chartTitleFont.setBold(true);
    mChart.setTitleFont(chartTitleFont);
    mChart.setTitleBrush(QBrush(kDb16Cyan));
    mSeries.setPen(QPen(QBrush(kDb16Green), 2));
    for (auto * axis : mSeries.attachedAxes())
    {
        axis->setLabelsColor(kDb16LightGray);
        axis->setLinePenColor(kDb16Steel);
        axis->setGridLineColor(kDb16Maroon);
    }

    ui->chartView->setRenderHints(QPainter::Antialiasing);
    ui->chartView->setChart(&mChart);

    setupPopulationChart();
    ui->populationChartView->setRenderHints(QPainter::Antialiasing);
    ui->populationChartView->setChart(&mPopulationChart);

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
    mRunHudLabel->setStyleSheet(kRunHudInProgressStyle);
    if (auto * rootLayout = qobject_cast<QVBoxLayout *>(ui->centralWidget->layout()))
    {
        rootLayout->insertWidget(0, mRunHudLabel);
    }

    // Settings-gear overlay: a small floating button parented directly to
    // laboratoryScrollArea (not laid out through it) so it draws on top of
    // the lab canvas without stealing layout space or mouse events outside
    // its own small rect. Positioned/kept pinned to the top-right corner
    // via repositionSettingsButton(), called here and from eventFilter()
    // whenever that scroll area resizes.
    mSettingsButton = new QToolButton(ui->laboratoryScrollArea);
    // A drawn icon rather than a Unicode gear glyph (U+2699) - not every
    // installed font covers that codepoint (the app-wide default, Consolas,
    // doesn't), so a glyph-based button risks silently rendering blank.
    mSettingsButton->setIcon(QIcon(buildGearIcon(kDb16LightGray, 20)));
    mSettingsButton->setIconSize(QSize(20, 20));
    mSettingsButton->setToolTip(tr("Settings"));
    mSettingsButton->setCursor(Qt::PointingHandCursor);
    mSettingsButton->setFixedSize(28, 28);
    mSettingsButton->setStyleSheet(QStringLiteral(
        // padding: 0px overrides theme.qss's app-wide QToolButton rule
        // (5px/12px, sized for text labels) - left as inherited, it left no
        // room at all for this button's 20x20 icon inside a 28x28 button.
        "QToolButton { background-color: rgba(20, 12, 28, 190); "
        "border: 2px solid #8595a1; border-radius: 0px; padding: 0px; } "
        "QToolButton:hover { background-color: rgba(48, 52, 109, 220); } "
        "QToolButton:pressed { background-color: rgba(109, 170, 44, 220); }"));
    connect(mSettingsButton, &QToolButton::clicked, this, &MainWindow::showSettingsDialog);
    mSettingsButton->raise();
    ui->laboratoryScrollArea->installEventFilter(this);
    repositionSettingsButton();

    updateRunHud();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupPopulationChart()
{
    mPopulationChart.setTitle("species population");

    // --- Pixel-art presentation pass: DB16-theme the chart -----------------
    // Same treatment as mChart above (QChart isn't reachable through the
    // app-wide QSS), except the legend is kept visible here since it's the
    // only place species-color -> species-name is spelled out for this
    // panel.
    mPopulationChart.setBackgroundBrush(QBrush(kDb16NearBlack));
    mPopulationChart.setBackgroundRoundness(0);
    mPopulationChart.setPlotAreaBackgroundBrush(QBrush(kDb16NearBlack));
    mPopulationChart.setPlotAreaBackgroundVisible(true);
    QFont chartTitleFont(QStringLiteral("Consolas"));
    chartTitleFont.setBold(true);
    mPopulationChart.setTitleFont(chartTitleFont);
    mPopulationChart.setTitleBrush(QBrush(kDb16Cyan));
    mPopulationChart.legend()->setLabelColor(kDb16LightGray);
    mPopulationChart.legend()->setFont(chartTitleFont);

    QLineSeries * firstSeries = nullptr;
    for (Species * species : ui->laboratory->species())
    {
        if (species == nullptr || species->type() != Species::typeAnimal)
        {
            continue;
        }

        auto * series = new QLineSeries(&mPopulationChart);
        series->setName(species->name());
        series->setPen(QPen(QBrush(species->color()), 2));
        mPopulationChart.addSeries(series);
        mPopulationSeries.insert(species, series);
        firstSeries = (firstSeries != nullptr) ? firstSeries : series;
    }

    // createDefaultAxes() attaches shared default axes to every series
    // already added to the chart; skip it entirely if there turned out to
    // be no animal species to plot.
    if (firstSeries != nullptr)
    {
        mPopulationChart.createDefaultAxes();
        for (auto * axis : mPopulationChart.axes())
        {
            axis->setLabelsColor(kDb16LightGray);
            axis->setLinePenColor(kDb16Steel);
            axis->setGridLineColor(kDb16Maroon);
        }
    }
}

void MainWindow::updateStatusBar()
{
    const int old_scrollbar_value = ui->textBrowser->verticalScrollBar()->value();

    const int numAnimals = ui->laboratory->numAnimals();
    qreal calculationsPerSecond = numAnimals * ui->laboratory->cyclesPerSecond();
    mNumAnimalsLabel.setText(QString::number(numAnimals) + "-" + QString::number(calculationsPerSecond, 'f', 2));
    const QString stats = ui->laboratory->statistics();
    ui->textBrowser->setText(stats);
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

    // Population chart is updated unconditionally (like mSeries above) even
    // while its panel is hidden, so the history is already populated the
    // moment the user toggles it on.
    static qreal maxPopulation = 10.0;
    for (auto it = mPopulationSeries.constBegin(); it != mPopulationSeries.constEnd(); ++it)
    {
        const int population = populationFromStats(stats, it.key()->name());
        it.value()->append(x, population);
        maxPopulation = qMax(maxPopulation, qreal(population));
    }
    for (auto * axis : mPopulationChart.axes())
    {
        if (axis->orientation() == Qt::Horizontal)
        {
            axis->setRange(0.0, (x - fmod(x, 60)) + 60.0);
        }
        else if (axis->orientation() == Qt::Vertical)
        {
            axis->setRange(0.0, (maxPopulation - fmod(maxPopulation, 10)) + 10.0);
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

void MainWindow::showSettingsDialog()
{
    SettingsDialog dialog(this);
    dialog.setCrtShaderEnabled(ui->laboratory->crtShaderEnabled());
    connect(&dialog, &SettingsDialog::crtShaderEnabledChanged,
            ui->laboratory, &Laboratory::setCrtShaderEnabled);
    dialog.exec();
}

bool MainWindow::eventFilter(QObject * watched, QEvent * event)
{
    if (watched == ui->laboratoryScrollArea && event->type() == QEvent::Resize)
    {
        repositionSettingsButton();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::repositionSettingsButton()
{
    if (mSettingsButton == nullptr)
    {
        return;
    }
    constexpr int kMargin = 8;
    mSettingsButton->move(ui->laboratoryScrollArea->width() - mSettingsButton->width() - kMargin, kMargin);
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

    // "Threat" is deliberately not a new piece of run state: it's a purely
    // cosmetic pixel-bar rendering of the epoch/targetEpochs ratio already
    // exposed by Laboratory's public, read-only accessors (see the class
    // comment on ui-game-feel-engineer's mandate to consume RunState as a
    // read-only input rather than duplicating the run-loop-engineer's/
    // difficulty-encounter-designer's own state).
    const int targetEpochs = qMax(1, ui->laboratory->targetEpochs());
    const int currentEpoch = qMin(ui->laboratory->currentEpoch(), targetEpochs);
    const QString threat = threatBar(currentEpoch, targetEpochs);

    QString text = tr("RUN %1  Epoch %2/%3  |  %4 pop: %5")
        .arg(threat)
        .arg(currentEpoch)
        .arg(targetEpochs)
        .arg(speciesName)
        .arg(population);

    switch (ui->laboratory->runOutcome())
    {
    case RunOutcome::Won:
        text += tr("  |  VICTORY");
        mRunHudLabel->setStyleSheet(kRunHudWonStyle);
        break;
    case RunOutcome::Lost:
        text += tr("  |  DEFEAT");
        mRunHudLabel->setStyleSheet(kRunHudLostStyle);
        break;
    case RunOutcome::InProgress:
    default:
        mRunHudLabel->setStyleSheet(kRunHudInProgressStyle);
        break;
    }

    mRunHudLabel->setText(text);
}

int MainWindow::playerPopulationFromStatistics(const QString & speciesName) const
{
    return populationFromStats(ui->laboratory->statistics(), speciesName);
}
