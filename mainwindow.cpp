#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScrollBar>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    mSettings("ryank", "Evolve", this)
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
}

void MainWindow::toggleStart()
{
    ui->laboratory->toggleStart();
}

void MainWindow::reset()
{
    ui->laboratory->reset();
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
