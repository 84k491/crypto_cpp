#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "ITradingGateway.h"
#include "JsonStrategyConfig.h"
#include "Logger.h"
#include "Optimizer.h"
#include "PositionResultView.h"
#include "StrategyInstance.h"
#include "StrategyParametersWidget.h"
#include "Symbol.h"
#include "WorkStatus.h"
#include "chart_window.h"

#include <nlohmann/json.hpp>

#include <qtypes.h>
#include <thread>

MainWindow::MainWindow(QWidget * parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_event_consumer(std::make_shared<MainWindowEventConsumer>(*this))
{
    ui->setupUi(this);
    ui->wt_entry_params->setTitle("Entry parameters");

    if (const auto meta_info_opt = StrategyFactory::get_meta_info("TpslExit"); meta_info_opt.has_value()) {
        ui->wt_exit_params->setup_widget(meta_info_opt.value());
    }
    ui->wt_exit_params->setTitle("Exit parameters");

    connect(this, &MainWindow::signal_lambda, this, &MainWindow::on_lambda);

    connect(this, &MainWindow::signal_optimizer_passed_check, this, [this](int passed_checks, int total_checks) {
        if (passed_checks == total_checks) {
            ui->pb_optimize->setEnabled(true);
            ui->pb_optimize->setText("Optimize");
        }
        else {
            ui->pb_optimize->setEnabled(false);
            ui->pb_optimize->setText((std::to_string(passed_checks) + std::string(" / ") + std::to_string(total_checks)).c_str());
        }
    });

    ui->pb_stop->setEnabled(false);
    connect(this,
            &MainWindow::signal_optimized_config,
            this,
            &MainWindow::optimized_config_slot);

    ui->sb_work_hours->setValue(saved_state.m_work_hours);
    if (saved_state.m_start_ts_unix_time != 0) {
        ui->dt_from->setDateTime(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(saved_state.m_start_ts_unix_time)));
    }

    ui->cb_strategy->addItem("DoubleSma");
    ui->cb_strategy->addItem("BollingerBands");
    ui->cb_strategy->addItem("CandleBB");
    ui->cb_strategy->addItem("DebugEveryTick");
    ui->cb_strategy->addItem("RateOfChange");
    ui->cb_strategy->addItem("DSMADiff");
    ui->cb_strategy->addItem("Ratchet");
    ui->cb_strategy->addItem("RelativeStrengthIndex");
    ui->cb_strategy->addItem("BBRSI");
    ui->cb_strategy->setCurrentText(saved_state.m_strategy_name.c_str());
    ui->cb_exit_strategy->addItem("TpslExit");
    ui->cb_exit_strategy->addItem("TrailingStop");
    ui->cb_exit_strategy->addItem("DynamicTrailingStop");
    ui->cb_exit_strategy->setCurrentText("DynamicTrailingStop");

    const auto symbols = m_gateway.get_symbols("USDT");
    for (const auto & symbol : symbols) {
        ui->cb_symbol->addItem(symbol.symbol_name.c_str());
    }
    ui->cb_symbol->setCurrentText(saved_state.m_symbol.c_str());

    Logger::log<LogLevel::Status>("End of mainwindow constructor");
}

void MainWindow::handle_status_changed(WorkStatus status)
{
    Logger::logf<LogLevel::Status>("Work status: {}", to_string(status));
    ui->lb_work_status->setText(to_string(status).c_str());
    if (status == WorkStatus::Backtesting || status == WorkStatus::Live) {
        ui->pb_run->setEnabled(false);
        ui->pb_stop->setEnabled(true);
    }
    if (status == WorkStatus::Stopped || status == WorkStatus::Live || status == WorkStatus::Backtesting) {
        subscribe_to_strategy();
    }
    if (status == WorkStatus::Stopped || status == WorkStatus::Panic) {
        ui->pb_run->setEnabled(true);
        ui->pb_stop->setEnabled(false);
        m_subscriptions.clear();
    }
    switch (status) {
    case WorkStatus::Backtesting: break;
    default: {
        render_result(m_strategy_instance->strategy_result_channel().get());
        subscribe_for_positions();
        break;
    }
    }
}

void MainWindow::subscribe_to_strategy()
{
    Logger::log<LogLevel::Status>("mainwindow subscribe_to_strategy");
    m_subscriptions.push_back(m_strategy_instance->strategy_result_channel().subscribe(
            m_event_consumer,
            [&](const StrategyResult & result) {
                render_result(result);
            }));
}

void MainWindow::on_pb_stop_clicked()
{
    m_strategy_instance->stop_async();
    m_strategy_instance->finish_future().wait();
    Logger::log<LogLevel::Status>("Strategy stopped");
}

void MainWindow::on_pb_run_clicked()
{
    m_subscriptions.clear();

    const auto timerange_opt = get_timerange();
    if (!timerange_opt) {
        Logger::log<LogLevel::Error>("Invalid timerange");
        return;
    }
    const auto & timerange = *timerange_opt;

    const auto strategy_name = ui->cb_strategy->currentText().toStdString();
    const auto entry_config = ui->wt_entry_params->get_config();
    const auto strategy_ptr_opt = StrategyFactory::build_strategy(strategy_name, entry_config);
    if (!strategy_ptr_opt.has_value() || !strategy_ptr_opt.value() || !strategy_ptr_opt.value()->is_valid()) {
        Logger::log<LogLevel::Error>("Failed to build strategy");
        return;
    }

    const auto md_request = [&]() {
        std::optional<HistoricalMDRequestData> req_data;
        if (!ui->cb_live->isChecked()) {
            req_data = {.start = timerange.start(), .end = timerange.end()};
        }
        return req_data;
    }();

    const auto symbol = [&]() -> std::optional<Symbol> {
        const auto symbols = m_gateway.get_symbols("USDT");
        for (const auto & s : symbols) {
            if (s.symbol_name == ui->cb_symbol->currentText().toStdString()) {
                return s;
            }
        }
        return std::nullopt;
    }();
    if (!symbol.has_value()) {
        Logger::logf<LogLevel::Error>(
                "Invalid symbol on starting strategy: {}",
                ui->cb_symbol->currentText().toStdString());
        return;
    }

    m_strategy_instance.reset();
    auto & tr_gateway = [&]() -> ITradingGateway & {
        if (ui->cb_live->isChecked()) {
            if (!m_trading_gateway) {
                m_trading_gateway = std::make_unique<ByBitTradingGateway>();
            }
            return *m_trading_gateway;
        }
        else {
            m_backtest_tr_gateway = std::make_unique<BacktestTradingGateway>();
            return *m_backtest_tr_gateway;
        }
    }();

    m_strategy_instance = std::make_shared<StrategyInstance>(
            symbol.value(),
            md_request,
            strategy_ptr_opt.value(),
            ui->cb_exit_strategy->currentText().toStdString(),
            ui->wt_exit_params->get_config(),
            m_gateway,
            tr_gateway);
    if (ui->sb_channel_capacity_h->value() >= 0) {
        m_strategy_instance->set_channel_capacity(std::chrono::hours{ui->sb_channel_capacity_h->value()});
    }
    ui->pb_charts->setEnabled(true);

    if (auto * ptr = dynamic_cast<BacktestTradingGateway *>(&tr_gateway); ptr != nullptr) {
        ptr->set_price_source(m_strategy_instance->price_channel());
    }

    m_subscriptions.push_back(m_strategy_instance->status_channel().subscribe(
            m_event_consumer,
            [&](const WorkStatus & status) { handle_status_changed(status); }));

    m_strategy_instance->run_async();
    Logger::log<LogLevel::Status>("Strategy started");
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
    ui->lb_trades_count->setText(QString::number(result.trades_count));
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
}

void MainWindow::subscribe_for_positions()
{
    auto * holder_widget = new QWidget(ui->sa_positions);
    auto * lo = new QVBoxLayout(holder_widget);
    ui->sa_positions->setWidget(holder_widget);
    ui->sa_positions->setWidgetResizable(true);

    m_subscriptions.push_back(m_strategy_instance->positions_channel().subscribe(
            m_event_consumer,
            [&](const std::list<std::pair<std::chrono::milliseconds, PositionResult>> & list) {
                for (const auto & [_, position_result] : list) {
                    auto * view = new PositionResultView();
                    view->update(position_result);
                    view->set_strategy_instance(m_strategy_instance);
                    lo->addWidget(view);
                }
            },
            [&](auto, auto) {
                // TODO implement
                // it requieres a separate class of view to handle updates
                // to avoid rendering all positions on each update
            }));
}

void MainWindow::optimized_config_slot(const JsonStrategyConfig & entry_config, const JsonStrategyConfig & exit_config)
{
    ui->wt_entry_params->setup_values(entry_config);
    ui->wt_exit_params->setup_values(exit_config);
}

std::optional<Timerange> MainWindow::get_timerange() const
{
    const auto start = std::chrono::milliseconds{ui->dt_from->dateTime().toMSecsSinceEpoch()};
    const auto work_hours = std::chrono::hours{ui->sb_work_hours->value()};
    const auto end = std::chrono::milliseconds{start + work_hours};

    if (start >= end) {
        Logger::log<LogLevel::Error>("Invalid timerange");
        return {};
    }
    if (start.count() < 0 || end.count() < 0) {
        Logger::log<LogLevel::Error>("Invalid timerange");
        return {};
    }
    return {{start, end}};
}

void MainWindow::on_pb_optimize_clicked()
{
    if (m_strategy_instance) {
        if (m_strategy_instance->status_channel().get() != WorkStatus::Stopped) {
            Logger::log<LogLevel::Error>("Strategy is not stopped");
            return;
        }
        m_strategy_instance.reset();
    }

    const auto entry_strategy_meta_info = get_entry_strategy_parameters();
    const auto timerange_opt = get_timerange();
    const auto exit_strategy_meta_info = StrategyFactory::get_meta_info(ui->cb_exit_strategy->currentText().toStdString());
    if (!timerange_opt || !entry_strategy_meta_info || !exit_strategy_meta_info) {
        Logger::log<LogLevel::Error>("No value in required optional");
        return;
    }
    const std::string entry_strategy_name = ui->cb_strategy->currentText().toStdString();
    const std::string exit_strategy_name = ui->cb_exit_strategy->currentText().toStdString();

    OptimizerInputs optimizer_inputs = {
            .entry_strategy = {
                    .meta = entry_strategy_meta_info.value(),
                    .current_config = ui->wt_entry_params->get_config(),
                    .optimizable_parameters = ui->wt_optimizer_parameters->optimizable_parameters(),
            },
            .exit_strategy = {
                    .meta = exit_strategy_meta_info.value(),
                    .current_config = ui->wt_exit_params->get_config(),
                    .optimizable_parameters = ui->wt_optimizer_parameters->optimizable_parameters(),
            }};

    const auto & timerange = *timerange_opt;

    const auto symbol = [&]() -> std::optional<Symbol> {
        const auto symbols = m_gateway.get_symbols("USDT");
        for (const auto & s : symbols) {
            if (s.symbol_name == ui->cb_symbol->currentText().toStdString()) {
                return s;
            }
        }
        return std::nullopt;
    }();

    if (!symbol.has_value()) {
        Logger::logf<LogLevel::Error>(
                "Invalid symbol on starting optimizer: {}",
                ui->cb_symbol->currentText().toStdString());
        return;
    }

    std::thread t([this,
                   timerange,
                   optimizer_inputs,
                   symbol,
                   entry_strategy_name,
                   exit_strategy_name]() {
        Optimizer optimizer(
                m_gateway,
                symbol.value(),
                timerange,
                entry_strategy_name,
                exit_strategy_name,
                optimizer_inputs,
                ui->sb_optimizer_threads->value());

        optimizer.subscribe_for_passed_check([this](int passed_checks, int total_checks) {
            emit signal_optimizer_passed_check(passed_checks, total_checks);
        });

        const auto best_config = optimizer.optimize();
        if (!best_config.has_value()) {
            Logger::log<LogLevel::Error>("No best config");
            return;
        }
        emit signal_optimized_config(best_config.value().first, best_config.value().second);
        Logger::logf<LogLevel::Info>("Best config: {}; {}", best_config.value().first, best_config.value().second);
    });
    t.detach();
}

std::optional<JsonStrategyMetaInfo> MainWindow::get_entry_strategy_parameters() const
{
    const auto json_data = StrategyFactory::get_meta_info(ui->cb_strategy->currentText().toStdString());
    return json_data;
}

std::optional<JsonStrategyMetaInfo> MainWindow::get_exit_strategy_parameters() const
{
    const auto json_data = StrategyFactory::get_meta_info(ui->cb_exit_strategy->currentText().toStdString());
    return json_data;
}

void MainWindow::on_cb_strategy_currentTextChanged(const QString &)
{
    const auto entry_params_opt = get_entry_strategy_parameters();
    if (entry_params_opt) {
        ui->wt_entry_params->setup_widget(entry_params_opt.value());

        const auto exit_params_opt = get_exit_strategy_parameters();
        if (exit_params_opt.has_value()) {
            ui->wt_optimizer_parameters->setup_widget(entry_params_opt.value(), exit_params_opt.value());
        }
    }
}

void MainWindow::on_cb_exit_strategy_currentTextChanged(const QString &)
{
    const auto exit_params_opt = get_exit_strategy_parameters();
    if (exit_params_opt) {
        ui->wt_exit_params->setup_widget(exit_params_opt.value());

        const auto entry_params_opt = get_entry_strategy_parameters();
        if (entry_params_opt) {
            ui->wt_optimizer_parameters->setup_widget(entry_params_opt.value(), exit_params_opt.value());
        }
    }
}

bool MainWindowEventConsumer::push_to_queue(std::any value)
{
    auto & lambda_event = std::any_cast<LambdaEvent &>(value);
    m_mw.signal_lambda(std::move(lambda_event.func));
    return true;
}

void MainWindow::on_lambda(const std::function<void()> & lambda)
{
    lambda();
}

bool MainWindowEventConsumer::push_to_queue_delayed(std::chrono::milliseconds, const std::any)
{
    throw std::runtime_error("Not implemented");
}

void MainWindow::on_pb_charts_clicked()
{
    m_chart_window = new ChartWindow(
            "Whole run",
            m_strategy_instance,
            {},
            {},
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
