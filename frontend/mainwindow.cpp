#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "ITradingGateway.h"
#include "JsonStrategyConfig.h"
#include "Optimizer.h"
#include "StrategyInstance.h"
#include "StrategyParametersWidget.h"
#include "Symbol.h"
#include "Tpsl.h"
#include "WorkStatus.h"

#include <nlohmann/json.hpp>

#include <print>
#include <qtypes.h>
#include <thread>

MainWindow::MainWindow(QWidget * parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->wt_entry_params->setTitle("Entry parameters");

    if (const auto meta_info_opt = StrategyFactory::get_meta_info("TpslExit"); meta_info_opt.has_value()) {
        ui->wt_exit_params->setup_widget(meta_info_opt.value());
    }
    ui->wt_exit_params->setTitle("Exit parameters");

    connect(this, &MainWindow::signal_lambda, this, &MainWindow::on_lambda);

    connect(this,
            &MainWindow::signal_price,
            this,
            [this](std::chrono::milliseconds ts, double price) {
                get_or_create_chart(m_price_chart_name).push_series_value("price", ts, price);
            });

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
    ui->cb_strategy->addItem("TunedBB");
    ui->cb_strategy->addItem("DebugEveryTick");
    ui->cb_strategy->setCurrentText(saved_state.m_strategy_name.c_str());

    const auto symbols = m_gateway.get_symbols("USDT");
    for (const auto & symbol : symbols) {
        ui->cb_symbol->addItem(symbol.symbol_name.c_str());
    }
    ui->cb_symbol->setCurrentText(saved_state.m_symbol.c_str());

    std::cout << "End of mainwindow constructor" << std::endl;
}

void MainWindow::handle_status_changed(WorkStatus status)
{
    std::cout << "Work status: " << to_string(status) << std::endl;
    ui->lb_work_status->setText(to_string(status).c_str());
    if (status == WorkStatus::Backtesting || status == WorkStatus::Live) {
        ui->pb_run->setEnabled(false);
        ui->pb_stop->setEnabled(true);
    }
    if (status == WorkStatus::Stopped || status == WorkStatus::Live) {
        subscribe_to_strategy();
    }
    if (status == WorkStatus::Stopped || status == WorkStatus::Panic) {
        ui->pb_run->setEnabled(true);
        ui->pb_stop->setEnabled(false);
        m_subscriptions.clear();
        std::cout << "Resetting strategy instance" << std::endl;
        m_strategy_instance.reset();
    }
    switch (status) {
    case WorkStatus::Backtesting: break;
    default: {
        render_result(m_strategy_instance->strategy_result_publisher().get());
        break;
    }
    }
}

void MainWindow::on_pb_stop_clicked()
{
    m_strategy_instance->stop_async();
    m_strategy_instance->wait_for_finish().wait();
    std::cout << "Strategy stopped" << std::endl;
}

void MainWindow::on_pb_run_clicked()
{
    m_subscriptions.clear();

    for (auto & m_chart : m_charts) {
        m_chart.second->clear();
    }
    const auto timerange_opt = get_timerange();
    if (!timerange_opt) {
        std::cout << "ERROR Invalid timerange" << std::endl;
        return;
    }
    const auto & timerange = *timerange_opt;

    const auto strategy_name = ui->cb_strategy->currentText().toStdString();
    const auto entry_config = ui->wt_entry_params->get_config();
    const auto strategy_ptr_opt = StrategyFactory::build_strategy(strategy_name, entry_config);
    if (!strategy_ptr_opt.has_value() || !strategy_ptr_opt.value() || !strategy_ptr_opt.value()->is_valid()) {
        std::cout << "ERROR Failed to build strategy" << std::endl;
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
        std::cout << "ERROR Invalid symbol on starting strategy" << std::endl;
        return;
    }

    auto & tr_gateway = [&]() -> ITradingGateway & {
        if (ui->cb_live->isChecked()) {
            return m_trading_gateway;
        }
        else {
            m_backtest_tr_gateway = std::make_unique<BacktestTradingGateway>();
            return *m_backtest_tr_gateway;
        }
    }();

    m_strategy_instance = std::make_unique<StrategyInstance>(
            symbol.value(),
            md_request,
            strategy_ptr_opt.value(),
            ui->wt_exit_params->get_config(),
            m_gateway,
            tr_gateway);

    if (auto * ptr = dynamic_cast<BacktestTradingGateway *>(&tr_gateway); ptr != nullptr) {
        ptr->set_price_source(m_strategy_instance->klines_publisher());
    }

    m_subscriptions.push_back(m_strategy_instance->status_publisher().subscribe(
            *this,
            [&](const WorkStatus & status) { handle_status_changed(status); }));

    m_strategy_instance->run_async();
    std::cout << "strategy started" << std::endl;
}

void MainWindow::subscribe_to_strategy()
{
    std::cout << "subscribe_to_strategy" << std::endl;
    m_subscriptions.push_back(m_strategy_instance->klines_publisher().subscribe(
            [this](const auto & vec) {
                std::vector<std::pair<std::chrono::milliseconds, double>> new_data;
                new_data.reserve(vec.size());
                for (const auto & [ts, v] : vec) {
                    new_data.emplace_back(ts, v.close);
                }
                auto & plot = get_or_create_chart(m_price_chart_name);
                plot.push_series_vector("price", new_data);
            },
            [&](std::chrono::milliseconds ts, const OHLC & ohlc) {
                emit signal_price(ts, ohlc.close);
            }));
    m_subscriptions.push_back(m_strategy_instance->tpsl_publisher().subscribe(
            *this,
            [this](const std::vector<std::pair<std::chrono::milliseconds, Tpsl>> & input_vec) {
                std::vector<std::pair<std::chrono::milliseconds, double>> tp, sl;
                for (const auto & [ts, tpsl] : input_vec) {
                    tp.emplace_back(ts, tpsl.take_profit_price);
                    sl.emplace_back(ts, tpsl.stop_loss_price);
                }
                auto & plot = get_or_create_chart(m_price_chart_name);
                plot.push_scatter_series_vector("take_profit", tp);
                plot.push_scatter_series_vector("stop_loss", sl);
            },
            [&](std::chrono::milliseconds ts, const Tpsl & tpsl) {
                get_or_create_chart(m_price_chart_name).push_tpsl(ts, tpsl);
            }));
    m_subscriptions.push_back(
            m_strategy_instance
                    ->strategy_internal_data_publisher()
                    .subscribe(
                            *this,
                            [this](
                                    const std::vector<
                                            std::pair<
                                                    std::chrono::milliseconds,
                                                    std::pair<std::string, double>>> & vec) {
                                std::map<std::string, std::vector<std::pair<std::chrono::milliseconds, double>>> vec_map;
                                for (const auto & [ts, v] : vec) {
                                    const auto & [name, value] = v;
                                    vec_map[name].emplace_back(ts, value);
                                }
                                auto & plot = get_or_create_chart(m_price_chart_name);
                                for (const auto & [name, out_vec] : vec_map) {
                                    plot.push_series_vector(name, out_vec);
                                }
                            },
                            [&](std::chrono::milliseconds ts, const std::pair<const std::string, double> & data_pair) {
                                const auto & [name, data] = data_pair;
                get_or_create_chart(m_price_chart_name).push_series_value(name, ts, data);
                            }));
    m_subscriptions.push_back(m_strategy_instance->signals_publisher().subscribe(
            *this,
            [this](const std::vector<std::pair<std::chrono::milliseconds, Signal>> & input_vec) {
                std::vector<std::pair<std::chrono::milliseconds, double>> buy, sell;
                for (const auto & [ts, signal] : input_vec) {
                    switch (signal.side) {
                    case Side::Close: break;
                    case Side::Buy: {
                        buy.emplace_back(ts, signal.price);
                        break;
                    }
                    case Side::Sell: {
                        sell.emplace_back(ts, signal.price);
                        break;
                    }
                    }
                }
                auto & plot = get_or_create_chart(m_price_chart_name);
                plot.push_scatter_series_vector("buy_trade", buy);
                plot.push_scatter_series_vector("sell_trade", sell);
            },
            [&](std::chrono::milliseconds, const Signal & signal) {
                get_or_create_chart(m_price_chart_name).push_signal(signal);
            }));
    m_subscriptions.push_back(m_strategy_instance->depo_publisher().subscribe(
            *this,
            [this](const auto & vec) {
                auto & plot = get_or_create_chart(m_depo_chart_name);
                plot.push_series_vector("depo", vec);
            },
            [&](std::chrono::milliseconds ts, double depo) {
                get_or_create_chart(m_depo_chart_name).push_series_value("depo", ts, depo);
            }));
    m_subscriptions.push_back(m_strategy_instance->strategy_result_publisher().subscribe(
            *this,
            [&](const StrategyResult & result) {
                const auto status = m_strategy_instance->status_publisher().get();
                switch (status) {
                case WorkStatus::Backtesting: break;
                default: {
                    render_result(result);
                    break;
                }
                }
            }));
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
    ui->lb_fees_paid->setText(QString::number(result.fees_paid));
    ui->lb_profit_per_trade->setText(QString::number(result.profit_per_trade()));
    ui->lb_best_profit->setText(QString::number(result.best_profit_trade.value_or(0.0)));
    ui->lb_worst_loss->setText(QString::number(result.worst_loss_trade.value_or(0.0)));
    ui->lb_max_depo->setText(QString::number(result.max_depo));
    ui->lb_min_depo->setText(QString::number(result.min_depo));
    ui->lb_longest_profit_pos->setText(QString::number(result.longest_profit_trade_time.count()));
    ui->lb_longest_loss_pos->setText(QString::number(result.longest_loss_trade_time.count()));
    ui->lb_win_rate->setText(QString::number(result.win_rate() * 100.));
}

void MainWindow::optimized_config_slot(const JsonStrategyConfig & entry_config, const JsonStrategyConfig & exit_config)
{
    ui->wt_entry_params->setup_values(entry_config);
    ui->wt_exit_params->setup_values(exit_config);
}

std::optional<Timerange> MainWindow::get_timerange() const
{
    const auto start = std::chrono::milliseconds{ui->dt_from->dateTime().toMSecsSinceEpoch()};
    std::cout << ui->dt_from->dateTime().toString().toStdString() << std::endl;
    const auto work_hours = std::chrono::hours{ui->sb_work_hours->value()};
    const auto end = std::chrono::milliseconds{start + work_hours};

    if (start >= end) {
        std::cout << "ERROR Invalid timerange" << std::endl;
        return {};
    }
    if (start.count() < 0 || end.count() < 0) {
        std::cout << "ERROR Invalid timerange" << std::endl;
        return {};
    }
    return {{start, end}};
}

void MainWindow::on_pb_optimize_clicked()
{
    const auto entry_strategy_meta_info = get_strategy_parameters();
    const auto timerange_opt = get_timerange();
    const auto exit_strategy_meta_info = StrategyFactory::get_meta_info("TpslExit");
    if (!timerange_opt || !entry_strategy_meta_info || !exit_strategy_meta_info) {
        std::cout << "ERROR no value in required optional" << std::endl;
        return;
    }
    std::string strategy_name = ui->cb_strategy->currentText().toStdString();
    const auto entry_config = [&]() -> std::variant<JsonStrategyMetaInfo, JsonStrategyConfig> {
        if (ui->cb_optimize_entry->isChecked()) {
            return entry_strategy_meta_info.value();
        }
        else {
            return ui->wt_entry_params->get_config();
        }
    };
    const auto exit_config = [&]() -> std::variant<JsonStrategyMetaInfo, TpslExitStrategyConfig> {
        if (ui->cb_optimize_exit->isChecked()) {
            return exit_strategy_meta_info.value();
        }
        else {
            return ui->wt_exit_params->get_config();
        }
    };
    OptimizerInputs optimizer_inputs = {entry_config(), exit_config()};

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
        std::cout << "ERROR Invalid symbol on starting optimizer" << std::endl;
        return;
    }

    std::thread t([this, timerange, optimizer_inputs, symbol, strategy_name]() {
        Optimizer optimizer(
                m_gateway,
                symbol.value(),
                timerange,
                strategy_name,
                optimizer_inputs);

        optimizer.subscribe_for_passed_check([this](int passed_checks, int total_checks) {
            emit signal_optimizer_passed_check(passed_checks, total_checks);
        });

        const auto best_config = optimizer.optimize();
        if (!best_config.has_value()) {
            std::cout << "ERROR no best config" << std::endl;
            return;
        }
        emit signal_optimized_config(best_config.value().first, best_config.value().second.to_json());
        std::cout << "Best config: " << best_config.value().first << "; " << best_config.value().second << std::endl;
    });
    t.detach();
}

std::optional<JsonStrategyMetaInfo> MainWindow::get_strategy_parameters() const
{
    const auto json_data = StrategyFactory::get_meta_info(ui->cb_strategy->currentText().toStdString());
    return json_data;
}

void MainWindow::on_cb_strategy_currentTextChanged(const QString &)
{
    const auto params_opt = get_strategy_parameters();
    if (params_opt) {
        ui->wt_entry_params->setup_widget(params_opt.value());
    }
}

MultiSeriesChart & MainWindow::get_or_create_chart(const std::string & chart_name)
{
    if (auto it = m_charts.find(chart_name); it != m_charts.end()) {
        return *it->second;
    }
    auto * new_chart = new MultiSeriesChart();
    new_chart->set_title(chart_name);
    m_charts[chart_name] = new_chart;
    ui->verticalLayout_graph->addWidget(new_chart);
    return *new_chart;
}

bool MainWindow::push_to_queue(std::any value)
{
    auto & lambda_event = std::any_cast<LambdaEvent &>(value);
    signal_lambda(std::move(lambda_event.func));
    return true;
}

void MainWindow::on_lambda(const std::function<void()> & lambda)
{
    lambda();
}

bool MainWindow::push_to_queue_delayed(std::chrono::milliseconds, const std::any)
{
    throw std::runtime_error("Not implemented");
}
