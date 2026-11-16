#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "JsonStrategyConfig.h"
#include "Logger.h"
#include "Macros.h"
#include "OrdinaryLeastSquares.h"
#include "PositionResultView.h"
#include "StrategyInstance.h"
#include "StrategyParametersWidget.h"
#include "Symbol.h"
#include "WorkStatus.h"
#include "chart_window.h"

#include <nlohmann/json.hpp>

MainWindow::MainWindow(Backend & backend, QWidget * parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_event_consumer(*this)
    , m_backend(backend)
    , m_backend_sub(m_event_consumer)
    , m_run_sub(std::make_unique<EventSubcriber>(m_event_consumer))
    , m_positions_sub(std::make_unique<EventSubcriber>(m_event_consumer))

{
    ui->setupUi(this);
    ui->wt_entry_params->setTitle("Entry parameters");

    connect(this, &MainWindow::signal_lambda, this, &MainWindow::on_lambda);

    ui->pb_stop->setEnabled(false);

    ui->sb_work_hours->setValue(saved_state.m_work_hours);
    if (saved_state.m_start_ts_unix_time != 0) {
        ui->dt_from->setDateTime(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(saved_state.m_start_ts_unix_time)));
    }

    const auto names = m_backend.strategy_names_channel().get();
    for (const auto & name : names) {
        ui->cb_strategy->addItem(name.c_str());
    }
    ui->cb_strategy->setCurrentText(saved_state.m_strategy_name.c_str());

    const auto symbols = m_backend.symbols_channel().get();
    for (const auto & symbol : symbols) {
        ui->cb_symbol->addItem(symbol.symbol_name.c_str());
    }
    ui->cb_symbol->setCurrentText(saved_state.m_symbol.c_str());

    update_playback_controls();
    subscribe_to_backend();

    LOG_STATUS("End of mainwindow constructor");
}

void MainWindow::subscribe_to_backend()
{
    m_backend_sub.subscribe(
            m_backend.current_run_channel(),
            [this](const CurrentRun & run) { handle_current_run_changed(run); });
    m_backend_sub.subscribe(
            m_backend.optimizer_progress_channel(),
            [this](const OptimizerProgress & progress) { handle_optimizer_progress(progress); });
    m_backend_sub.subscribe(
            m_backend.optimized_config_channel(),
            [this](const JsonStrategyConfig & entry_config) { ui->wt_entry_params->setup_values(entry_config); });
    handle_current_run_changed(m_backend.current_run_channel().get());
}

void MainWindow::handle_current_run_changed(const CurrentRun & run)
{
    m_current_run = run;
    m_run_sub = std::make_unique<EventSubcriber>(m_event_consumer);
    m_positions_sub = std::make_unique<EventSubcriber>(m_event_consumer);

    const auto strategy_instance_ptr = run.strategy_instance.lock();
    ui->pb_charts->setEnabled(strategy_instance_ptr != nullptr);
    if (!strategy_instance_ptr) {
        update_playback_controls();
        return;
    }
    auto & strategy_instance = *strategy_instance_ptr;

    m_run_sub->subscribe(
            strategy_instance.status_channel(),
            [this](const WorkStatus & status) { handle_status_changed(status); });
    m_run_sub->subscribe(
            strategy_instance.strategy_result_channel(),
            [this](const StrategyResult & result) {
                render_result(result);
            });
    handle_status_changed(strategy_instance.status_channel().get());
}

void MainWindow::handle_status_changed(WorkStatus status)
{
    UNWRAP_RET_VOID(strategy_instance, m_current_run.strategy_instance.lock());

    LOG_STATUS("Work status: {}", to_string(status));
    ui->lb_work_status->setText(to_string(status).c_str());
    if (status == WorkStatus::Running) {
        ui->pb_run->setEnabled(false);
        ui->pb_stop->setEnabled(true);
    }
    if (status == WorkStatus::Stopped || status == WorkStatus::Panic) {
        ui->pb_run->setEnabled(true);
        ui->pb_stop->setEnabled(false);
    }

    render_result(strategy_instance.strategy_result_channel().get());

    subscribe_for_positions();

    update_playback_controls();
}

void MainWindow::handle_optimizer_progress(const OptimizerProgress & progress)
{
    if (progress.passed_checks == progress.total_checks) {
        ui->pb_optimize->setEnabled(true);
        ui->pb_optimize->setText("Optimize");
    }
    else {
        ui->pb_optimize->setEnabled(false);
        ui->pb_optimize->setText((std::to_string(progress.passed_checks) + std::string(" / ") + std::to_string(progress.total_checks)).c_str());
    }
}

void MainWindow::on_pb_stop_clicked()
{
    m_backend.stop_channel().push({xg::newGuid()});
}

void MainWindow::on_pb_step_clicked()
{
    m_backend.playback_step_channel().push({});
}

void MainWindow::on_cb_playback_mode_currentIndexChanged(int)
{
    push_playback_settings();
    update_playback_controls();
}

void MainWindow::on_sb_playback_delta_min_valueChanged(int)
{
    push_playback_settings();
}

PlaybackSettingsEvent MainWindow::get_playback_settings() const
{
    return {
            static_cast<PlaybackMode>(ui->cb_playback_mode->currentIndex()),
            std::chrono::minutes{ui->sb_playback_delta_min->value()}};
}

void MainWindow::push_playback_settings()
{
    m_backend.playback_settings_channel().push(get_playback_settings());
}

void MainWindow::update_playback_controls()
{
    const auto mode = get_playback_settings().mode;
    const auto strategy_instance = m_current_run.strategy_instance.lock();
    const bool backtest_runs =
            strategy_instance &&
            m_current_run.is_backtest &&
            strategy_instance->status_channel().get() == WorkStatus::Running;

    ui->sb_playback_delta_min->setEnabled(mode != PlaybackMode::Rapid);
    ui->pb_step->setEnabled(mode == PlaybackMode::Step && backtest_runs);
}

void MainWindow::on_pb_run_clicked()
{
    const auto symbol_name = ui->cb_symbol->currentText().toStdString();
    const auto strategy_name = ui->cb_strategy->currentText().toStdString();
    const auto entry_config = ui->wt_entry_params->get_config();

    if (ui->cb_live->isChecked()) {
        m_backend.start_live_run_channel().push({
                .command_guid = xg::newGuid(),
                .symbol_name = symbol_name,
                .strategy_name = strategy_name,
                .strategy_config = entry_config,
                .channel_capacity = get_channel_capacity(),
        });
        return;
    }

    const auto timerange = get_timerange();
    m_backend.start_backtest_channel().push({
            .command_guid = xg::newGuid(),
            .symbol_name = symbol_name,
            .strategy_name = strategy_name,
            .strategy_config = entry_config,
            .start = timerange.start(),
            .end = timerange.end(),
            .channel_capacity = get_channel_capacity(),
            .playback_settings = get_playback_settings(),
    });
}

MainWindow::~MainWindow()
{
    const auto start = std::chrono::milliseconds{ui->dt_from->dateTime().toMSecsSinceEpoch()};
    const auto work_hours = std::chrono::hours{ui->sb_work_hours->value()};

    saved_state.m_start_ts_unix_time = start.count();
    saved_state.m_work_hours = static_cast<int>(work_hours.count());
    saved_state.m_symbol = ui->cb_symbol->currentText().toStdString();
    saved_state.m_strategy_name = ui->cb_strategy->currentText().toStdString();

    delete ui;
}

void MainWindow::render_result(StrategyResult result)
{
    ui->lb_position_amount->setText(QString::number(result.position_currency_amount));
    ui->lb_final_profit->setText(QString::number(result.final_profit));
    ui->lb_apr->setText(QString::number(result.apr()));
    ui->lb_trades_count->setText(QString::number(result.trades_count));
    ui->lb_trades_per_month->setText(QString::number(result.trades_per_month()));
    ui->lb_trade_date->setText(QString::fromStdString(result.last_trade_date));
    ui->lb_fees_paid->setText(QString::number(result.fees_paid));
    ui->lb_profit_per_trade->setText(QString::number(result.profit_per_trade()));
    ui->lb_best_profit->setText(QString::number(result.best_profit_trade.value_or(0.0)));
    ui->lb_worst_loss->setText(QString::number(result.worst_loss_trade.value_or(0.0)));
    ui->lb_max_depo->setText(QString::number(result.max_depo));
    ui->lb_min_depo->setText(QString::number(result.min_depo));
    ui->lb_longest_profit_pos->setText(QString::number(result.longest_profit_trade_time.count()));
    ui->lb_longest_loss_pos->setText(QString::number(result.longest_loss_trade_time.count()));
    ui->lb_win_rate->setText(QString::number(result.win_rate() * 100.));
    ui->lb_avg_profit_pos_time->setText(QString::number(result.avg_profit_pos_time));
    ui->lb_avg_loss_pos_time->setText(QString::number(result.avg_loss_pos_time));
    ui->lb_trend_deviation->setText(QString::number(result.depo_standard_deviation));

    OLS::PriceRegressionFunction depo_trend{result.depo_trend_coef, result.depo_trend_const};
    ui->lb_trend_profit->setText(QString::number(depo_trend(result.last_position_closed_ts)));
}

void MainWindow::subscribe_for_positions()
{
    auto * holder_widget = new QWidget(ui->sa_positions);
    auto * lo = new QVBoxLayout(holder_widget);
    ui->sa_positions->setWidget(holder_widget);
    ui->sa_positions->setWidgetResizable(true);

    UNWRAP_RET_VOID(strategy_instance, m_current_run.strategy_instance.lock());
    m_positions_sub = std::make_unique<EventSubcriber>(m_event_consumer);
    m_positions_sub->subscribe(
            strategy_instance.positions_channel(),
            [this, lo](const std::deque<std::pair<std::chrono::milliseconds, PositionResult>> & list) {
                for (const auto & [_, position_result] : list) {
                    auto * view = new PositionResultView();
                    view->update(position_result);
                    view->set_strategy_instance(m_current_run.strategy_instance);
                    lo->addWidget(view);
                }
            },
            [&](auto, auto) {
                // TODO implement
                // it requieres a separate class of view to handle updates
                // to avoid rendering all positions on each update
            });
}

Timerange MainWindow::get_timerange() const
{
    const auto start = std::chrono::milliseconds{ui->dt_from->dateTime().toMSecsSinceEpoch()};
    const auto work_hours = std::chrono::hours{ui->sb_work_hours->value()};
    return {start, start + work_hours};
}

std::optional<std::chrono::milliseconds> MainWindow::get_channel_capacity() const
{
    if (ui->sb_channel_capacity_h->value() < 0) {
        return std::nullopt;
    }
    return std::chrono::hours{ui->sb_channel_capacity_h->value()};
}

void MainWindow::on_pb_optimize_clicked()
{
    const auto entry_strategy_meta_info = get_entry_strategy_parameters();
    if (!entry_strategy_meta_info) {
        LOG_ERROR("No value in required optional");
        return;
    }

    const auto timerange = get_timerange();
    m_backend.start_optimizer_channel().push({
            .symbol_name = ui->cb_symbol->currentText().toStdString(),
            .strategy_name = ui->cb_strategy->currentText().toStdString(),
            .start = timerange.start(),
            .end = timerange.end(),
            .optimizer_inputs = {
                    .entry_strategy = {
                            .meta = entry_strategy_meta_info.value(),
                            .current_config = ui->wt_entry_params->get_config(),
                            .optimizable_parameters = ui->wt_optimizer_parameters->optimizable_parameters(),
                    }},
            .thread_count = static_cast<size_t>(ui->sb_optimizer_threads->value()),
    });
}

std::optional<JsonStrategyMetaInfo> MainWindow::get_entry_strategy_parameters() const
{
    const auto json_data = StrategyFactory::get_meta_info(ui->cb_strategy->currentText().toStdString());
    return json_data;
}

void MainWindow::on_cb_strategy_currentTextChanged(const QString &)
{
    const auto entry_params_opt = get_entry_strategy_parameters();
    if (entry_params_opt) {
        ui->wt_entry_params->setup_widget(entry_params_opt.value());
        ui->wt_optimizer_parameters->setup_widget(entry_params_opt.value());
    }
}

void MainWindowEventConsumer::push(LambdaEvent value)
{
    m_mw.signal_lambda(std::move(value.func));
}

void MainWindow::on_lambda(std::function<void()> lambda)
{
    lambda();
}

void MainWindow::on_pb_charts_clicked()
{
    UNWRAP_RET_VOID(strategy_instance, m_current_run.strategy_instance.lock());
    const auto status = strategy_instance.status_channel().get();
    const bool strategy_runs = status == WorkStatus::Running;
    m_chart_window = new ChartWindow(
            "Whole run",
            m_current_run.strategy_instance,
            std::nullopt,
            strategy_runs,
            true,
            nullptr);
    m_chart_window->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_chart_window, &ChartWindow::destroyed, [this](auto) {
        m_chart_window = nullptr;
    });
    m_chart_window->show();
}

void MainWindow::closeEvent(QCloseEvent *)
{
    if (m_chart_window != nullptr) {
        m_chart_window->close();
    }
}
