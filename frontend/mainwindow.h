#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ByBitGateway.h"
#include "DragableChart.h"

#include <QMainWindow>
#include <QScatterSeries>
#include <QtCharts>
#include <qtypes.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget * parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_pushButton_clicked();

private:
    void update_axes(std::chrono::milliseconds x, double y);
    void updateChart(); // TODO remove
    void setupChart();
    void run_strategy();

private:
    Ui::MainWindow * ui;

    ByBitGateway gateway;

    long x_min = std::numeric_limits<long>::max();
    long x_max = std::numeric_limits<long>::min();

    double y_min = std::numeric_limits<double>::max();
    double y_max = std::numeric_limits<double>::min();

    QDateTimeAxis * axisX = nullptr;
    QChart * chart = nullptr;
    DragableChart * chartView = nullptr;
    QLineSeries * prices = nullptr;
    QScatterSeries * buy_signals = nullptr;
    QScatterSeries * sell_signals = nullptr;
    QLineSeries * slow_avg = nullptr;
    QLineSeries * fast_avg = nullptr;
};
#endif // MAINWINDOW_H
