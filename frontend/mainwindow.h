#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "BacktestTradingGateway.h"
#include "ByBitGateway.h"
#include "ByBitTradingGateway.h"
#include "JsonStrategyConfig.h"
#include "StrategyFactory.h"
#include "StrategyInstance.h"
#include "StrategyResult.h"
#include "WorkStatus.h"
#include "chart_window.h"

#include <QMainWindow>
#include <fstream>
#include <memory>
#include <qspinbox.h>
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
    std::string m_symbol;
    std::string m_strategy_name;
};

class MainWindow final
    : public QMainWindow
    , public IEventConsumer<LambdaEvent>
{
    Q_OBJECT

public:
    MainWindow(QWidget * parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_pb_run_clicked();
    void on_pb_optimize_clicked();
    void on_pb_stop_clicked();
    void on_cb_strategy_currentTextChanged(const QString & arg1);

    void on_pb_charts_clicked();
    void render_result(StrategyResult result);
    void optimized_config_slot(const JsonStrategyConfig & entry_config, const JsonStrategyConfig & exit_config);

    void on_lambda(const std::function<void()> & lambda);

signals:
    void signal_optimized_config(JsonStrategyConfig entry_config, JsonStrategyConfig exit_config);
    void signal_optimizer_passed_check(int passed_checks, int total_checks);
    void signal_lambda(std::function<void()> lambda);

private:
    // IEventConsumer<LambdaEvent>
    bool push_to_queue(std::any value) override;
    bool push_to_queue_delayed(std::chrono::milliseconds delay, const std::any value) override;

private:
    void handle_status_changed(WorkStatus status);
    void construct_optimizer_ui();
    void subscribe_to_strategy();
    std::optional<Timerange> get_timerange() const;

    std::optional<JsonStrategyMetaInfo> get_strategy_parameters() const;

private:
    Ui::MainWindow * ui;

    StrategyFactory m_strategy_factory;

    ByBitGateway m_gateway;
    std::unique_ptr<BacktestTradingGateway> m_backtest_tr_gateway;
    ByBitTradingGateway m_trading_gateway;

    std::shared_ptr<StrategyInstance> m_strategy_instance;
    std::list<std::shared_ptr<ISubsription>> m_subscriptions;

    std::optional<JsonStrategyMetaInfo> m_last_set_strategy_parameters;
    std::map<std::string, QDoubleSpinBox *> m_strategy_parameters_spinboxes;
    std::map<std::string, double> m_strategy_parameters_values;

    SavedStateUi saved_state;

    std::unique_ptr<ChartWindow> m_chart_window;
};
#endif // MAINWINDOW_H
