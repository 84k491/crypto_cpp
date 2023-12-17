#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ByBitGateway.h"
#include "DragableChart.h"
#include "StrategyFactory.h"
#include "StrategyResult.h"
#include "JsonStrategyConfig.h"

#include <QMainWindow>
#include <QScatterSeries>
#include <QtCharts>
#include <fstream>
#include <qtypes.h>
#include <string>

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
    void on_pb_optimize_clicked();
    void render_result(StrategyResult result);
    void optimized_config_slot(const JsonStrategyConfig & config);
    void on_strategy_parameters_changed(const std::string & name, double value);

signals:
    void signal_price(std::chrono::milliseconds ts, double price);
    void signal_signal(Signal signal);
    void signal_strategy_internal(const std::string name,
                                  std::chrono::milliseconds ts,
                                  double data);
    void signal_depo(std::chrono::milliseconds ts, double depo);
    void signal_result(StrategyResult result);
    void signal_optimized_config(JsonStrategyConfig config);
    void signal_strategy_parameters_changed(std::string name, double value);

private:
    void construct_optimizer_ui();
    std::optional<Timerange> get_timerange() const;

    std::optional<JsonStrategyMetaInfo> get_strategy_parameters() const;
    void setup_specific_parameters(JsonStrategyMetaInfo strategy_parameters);
    JsonStrategyConfig get_config_from_ui() const;

private:
    Ui::MainWindow * ui;

    StrategyFactory m_strategy_factory;

    ByBitGateway m_gateway;
    DragableChart * m_chartView = nullptr;

    std::optional<JsonStrategyMetaInfo> m_last_set_strategy_parameters;
    std::map<std::string, double> m_strategy_parameters_values;

    SavedStateUi saved_state;
};
#endif // MAINWINDOW_H
