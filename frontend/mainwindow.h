#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "BacktestTradingGateway.h"
#include "ByBitGateway.h"
#include "ByBitTradingGateway.h"
#include "JsonStrategyConfig.h"
#include "MultiSeriesChart.h"
#include "StrategyFactory.h"
#include "StrategyInstance.h"
#include "StrategyResult.h"
#include "Tpsl.h"
#include "TpslExitStrategy.h"
#include "WorkStatus.h"

#include <QMainWindow>
#include <QScatterSeries>
#include <QtCharts>
#include <fstream>
#include <memory>
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
        if (j.contains("start_ts_unix_time")) {
            j.at("start_ts_unix_time").get_to(m_start_ts_unix_time);
        }
        if (j.contains("work_hours")) {
            j.at("work_hours").get_to(m_work_hours);
        }
        if (j.contains("symbol")) {
            j.at("symbol").get_to(m_symbol);
        }
        if (j.contains("strategy_name")) {
            j.at("strategy_name").get_to(m_strategy_name);
        }
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
        j["symbol"] = m_symbol;
        j["strategy_name"] = m_strategy_name;
        save_file << j.dump();
        save_file.close();
    }

    uint64_t m_start_ts_unix_time = {};
    int m_work_hours = {};
    std::string m_symbol = {};
    std::string m_strategy_name = {};
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget * parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_pb_run_clicked();
    void on_pb_optimize_clicked();
    void render_result(StrategyResult result);
    void optimized_config_slot(const JsonStrategyConfig & config);
    void on_strategy_parameters_changed(const std::string & name, double value);

    void on_cb_strategy_currentTextChanged(const QString & arg1);

    void on_pb_stop_clicked();

    void on_pb_test_clicked();

signals:
    void signal_price(std::chrono::milliseconds ts, double price);
    void signal_tpsl(std::chrono::milliseconds ts, Tpsl tpsl);
    void signal_signal(Signal signal);
    void signal_strategy_internal(const std::string name,
                                  std::chrono::milliseconds ts,
                                  double data);
    void signal_depo(std::chrono::milliseconds ts, double depo);
    void signal_result(StrategyResult result);
    void signal_work_status(WorkStatus status);
    void signal_optimized_config(JsonStrategyConfig config);
    void signal_strategy_parameters_changed(std::string name, double value);
    void signal_optimizer_passed_check(int passed_checks, int total_checks);

private:
    void construct_optimizer_ui();
    std::optional<Timerange> get_timerange() const;

    std::optional<JsonStrategyMetaInfo> get_strategy_parameters() const;
    void setup_specific_parameters(const JsonStrategyMetaInfo & strategy_parameters);
    JsonStrategyConfig get_entry_config_from_ui() const;
    TpslExitStrategyConfig get_exit_config_from_ui() const;

    MultiSeriesChart & get_or_create_chart(const std::string & chart_name);

    void get_strategy_data_snapshots();
    void subscribe_to_strategy();

private:
    Ui::MainWindow * ui;

    StrategyFactory m_strategy_factory;

    ByBitGateway m_gateway;
    std::unique_ptr<BacktestTradingGateway> m_backtest_tr_gateway;
    ByBitTradingGateway m_trading_gateway;

    std::unique_ptr<StrategyInstance> m_strategy_instance;
    std::list<std::shared_ptr<ISubsription>> m_subscriptions;

    const std::string m_price_chart_name = "prices";
    const std::string m_depo_chart_name = "depo";
    std::map<std::string, MultiSeriesChart *> m_charts;

    std::optional<JsonStrategyMetaInfo> m_last_set_strategy_parameters;
    std::map<std::string, QDoubleSpinBox *> m_strategy_parameters_spinboxes;
    std::map<std::string, double> m_strategy_parameters_values;

    SavedStateUi saved_state;
};
#endif // MAINWINDOW_H
