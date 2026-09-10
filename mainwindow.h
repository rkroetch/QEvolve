#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QtCharts>
#include <QSettings>

#include "laboratory.h"

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void updateStatusBar();
    void toggleStart();
    void reset();

private slots:
    void onPlantPatternChanged(PlantPattern pattern);

private:
    Ui::MainWindow *ui;
    QSettings mSettings;
    QTimer mNumAnimalsTimer;
    QLabel mNumAnimalsLabel;
    QChart mChart;
    QLineSeries mSeries;
};

#endif // MAINWINDOW_H
