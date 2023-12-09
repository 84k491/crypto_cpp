#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ByBitGateway.h"
#include "DragableChart.h"
#include "StrategyResult.h"

#include <QMainWindow>
#include <QScatterSeries>
#include <QtCharts>
#include <qtypes.h>
#include <fstream>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class SavedStateUi
{
public:
    SavedStateUi()
    {
        std::ifstream save_file("saved_state.json");
        if (!save_file.is_open()) {
            std::cout << "ERROR Failed to open saved state file" << std::endl;
            return;
        }

        auto j = json::parse(save_file);
        j.at("start_ts_unix_time").get_to(m_start_ts_unix_time);
        j.at("work_hours").get_to(m_work_hours);
        save_file.close();
    }

    ~SavedStateUi()
    {
        std::ofstream save_file("saved_state.json");
        if (!save_file.is_open()) {
            std::cout << "ERROR Failed to open saved state file" << std::endl;
            return;
        }

        json j;
        j["start_ts_unix_time"] = m_start_ts_unix_time;
        j["work_hours"] = m_work_hours;
        save_file << j.dump();
        save_file.close();
    }

    uint64_t m_start_ts_unix_time = {};
    int m_work_hours = {};
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget * parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_pushButton_clicked();
    void render_result(StrategyResult result);

signals:
    void signal_price(std::chrono::milliseconds ts, double price);
    void signal_signal(Signal signal);
    void signal_strategy_internal(const std::string name,
                                  std::chrono::milliseconds ts,
                                  double data);
    void signal_depo(std::chrono::milliseconds ts, double depo);
    void signal_result(StrategyResult result);

private:
    Ui::MainWindow * ui;

    ByBitGateway m_gateway;
    DragableChart * m_chartView = nullptr;

    SavedStateUi saved_state;
};
#endif // MAINWINDOW_H
