#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Backend.h"
#include "JsonStrategyConfig.h"
#include "Logger.h"
#include "StrategyFactory.h"
#include "StrategyInstance.h"
#include "StrategyResult.h"
#include "Timerange.h"
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
            LOG_ERROR("Failed to open saved state file");
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
            LOG_ERROR("Failed to open saved state file");
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

// TODO make template class QtEventConsumer
class MainWindow;
class MainWindowEventConsumer : public ILambdaAcceptor
{
public:
    MainWindowEventConsumer(MainWindow & mw)
        : m_mw(mw)
    {
    }

private:
    // IEventConsumer<LambdaEvent>
    void push(LambdaEvent value) override;
    void discard_subscriber_events(xg::Guid) override {}

private:
    MainWindow & m_mw;
};

class MainWindow final
    : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Backend & backend, QWidget * parent = nullptr);
    ~MainWindow() override;

    void closeEvent(QCloseEvent * event) override;

private slots:
    void on_pb_run_clicked();
    void on_pb_optimize_clicked();
    void on_pb_stop_clicked();
    void on_pb_step_clicked();
    void on_cb_playback_mode_currentIndexChanged(int);
    void on_sb_playback_delta_min_valueChanged(int);
    void on_cb_strategy_currentTextChanged(const QString &);

    void on_pb_charts_clicked();
    void render_result(StrategyResult result);
    void subscribe_for_positions();

    void on_lambda(std::function<void()> lambda);

signals:
    void signal_lambda(std::function<void()> lambda);

private:
    void subscribe_to_backend();
    void handle_current_run_changed(const CurrentRun & run);
    void handle_status_changed(WorkStatus status);
    void handle_optimizer_progress(const OptimizerProgress & progress);
    void construct_optimizer_ui();
    Timerange get_timerange() const;
    std::optional<std::chrono::milliseconds> get_channel_capacity() const;

    PlaybackSettingsEvent get_playback_settings() const;
    void push_playback_settings();
    void update_playback_controls();

    std::optional<JsonStrategyMetaInfo> get_entry_strategy_parameters() const;

private:
    Ui::MainWindow * ui;
    MainWindowEventConsumer m_event_consumer;

    Backend & m_backend;
    CurrentRun m_current_run;

    EventSubcriber m_backend_sub;
    std::unique_ptr<EventSubcriber> m_run_sub;
    std::unique_ptr<EventSubcriber> m_positions_sub;

    std::optional<JsonStrategyMetaInfo> m_last_set_strategy_parameters;
    std::map<std::string, QDoubleSpinBox *> m_strategy_parameters_spinboxes;
    std::map<std::string, double> m_strategy_parameters_values;

    SavedStateUi saved_state;

    ChartWindow * m_chart_window = nullptr;
};
#endif // MAINWINDOW_H
