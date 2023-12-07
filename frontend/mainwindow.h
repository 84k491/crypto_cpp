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

signals:
    void signal_price(std::chrono::milliseconds ts, double price);
    void signal_signal(Signal signal);
    void signal_strategy_internal(const std::string name,
                                  std::chrono::milliseconds ts,
                                  double data);

private:
    Ui::MainWindow * ui;

    ByBitGateway m_gateway;
    DragableChart * m_chartView = nullptr;
};
#endif // MAINWINDOW_H
