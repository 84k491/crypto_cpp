#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "DoubleSmaStrategy.h"
#include "ITradingGateway.h"
#include "JsonStrategyConfig.h"
#include "Optimizer.h"
#include "StrategyInstance.h"
#include "Symbol.h"
#include "Tpsl.h"
#include "WorkStatus.h"

#include <nlohmann/json.hpp>

#include <openssl/hmac.h>
#include <qboxlayout.h>
#include <qscatterseries.h>
#include <qspinbox.h>
#include <qtypes.h>
#include <thread>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/roles/client_endpoint.hpp>

MainWindow::MainWindow(QWidget * parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(this,
            &MainWindow::signal_price,
            this,
            [this](std::chrono::milliseconds ts, double price) {
                get_or_create_chart(m_price_chart_name).push_series_value("price", ts, price);
            });

    connect(this,
            &MainWindow::signal_tpsl,
            this,
            [this](std::chrono::milliseconds ts, Tpsl tpsl) {
                get_or_create_chart(m_price_chart_name).push_tpsl(ts, tpsl);
            });

    connect(this,
            &MainWindow::signal_signal,
            this,
            [this](Signal signal) {
                get_or_create_chart(m_price_chart_name).push_signal(signal);
            });

    connect(this,
            &MainWindow::signal_strategy_internal,
            this,
            [this](const std::string & name,
                   std::chrono::milliseconds ts,
                   double data) {
                get_or_create_chart(m_price_chart_name).push_series_value(name, ts, data);
            });

    connect(this,
            &MainWindow::signal_depo,
            this,
            [this](std::chrono::milliseconds ts, double depo) {
                get_or_create_chart(m_depo_chart_name).push_series_value("depo", ts, depo);
            });

    connect(this,
            &MainWindow::signal_result,
            this,
            &MainWindow::render_result);

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
    connect(this, &MainWindow::signal_work_status, this, [this](const WorkStatus status) {
        std::cout << "Work status: " << to_string(status) << std::endl;
        ui->lb_work_status->setText(to_string(status).c_str());
        if (status == WorkStatus::Backtesting || status == WorkStatus::Live) {
            ui->pb_run->setEnabled(false);
            ui->pb_stop->setEnabled(true);
        }
        if (status == WorkStatus::Stopped || status == WorkStatus::Live) {
            subscribe_to_strategy();
        }
        if (status == WorkStatus::Stopped || status == WorkStatus::Crashed) {
            ui->pb_run->setEnabled(true);
            ui->pb_stop->setEnabled(false);
            m_subscriptions.clear();
            m_strategy_instance.reset();
        }
    });

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
    ui->cb_strategy->addItem("DebugEveryTick");
    ui->cb_strategy->setCurrentText(saved_state.m_strategy_name.c_str());

    const auto symbols = m_gateway.get_symbols("USDT");
    for (const auto & symbol : symbols) {
        ui->cb_symbol->addItem(symbol.symbol_name.c_str());
    }
    ui->cb_symbol->setCurrentText(saved_state.m_symbol.c_str());

    std::cout << "End of mainwindow constructor" << std::endl;
}

void MainWindow::on_pb_stop_clicked()
{
    m_strategy_instance->stop_async();
    m_strategy_instance->wait_for_finish().wait();
    std::cout << "Strategy stopped" << std::endl;
    m_strategy_instance.reset();
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
    const auto strategy_ptr_opt = StrategyFactory::build_strategy(strategy_name, get_entry_config_from_ui());
    if (!strategy_ptr_opt.has_value() || !strategy_ptr_opt.value() || !strategy_ptr_opt.value()->is_valid()) {
        std::cout << "ERROR Failed to build strategy" << std::endl;
        return;
    }

    const auto md_request = [&]() {
        MarketDataRequest result;
        if (ui->cb_live->isChecked()) {
            result.go_live = true;
            result.historical_range.reset();
        }
        else {
            result.go_live = false;
            result.historical_range = MarketDataRequest::HistoricalRange();
            result.historical_range->start = timerange.start();
            result.historical_range->end = timerange.end();
        }
        return result;
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
            get_exit_config_from_ui(),
            m_gateway,
            tr_gateway);

    if (auto * ptr = dynamic_cast<BacktestTradingGateway *>(&tr_gateway); ptr != nullptr) {
        ptr->set_price_source(m_strategy_instance->klines_publisher());
    }

    m_subscriptions.push_back(m_strategy_instance->status_publisher().subscribe(
            [&](const WorkStatus & status) {
                emit signal_work_status(status);
                switch (status) {
                case WorkStatus::Backtesting: break;
                default: {
                    emit signal_result(m_strategy_instance->strategy_result_publisher().get());
                    break;
                }
                }
            }));

    m_strategy_instance->run_async();
    std::cout << "strategy started" << std::endl;
}

void MainWindow::subscribe_to_strategy()
{
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
                emit signal_tpsl(ts, tpsl);
            }));
    m_subscriptions.push_back(
            m_strategy_instance
                    ->strategy_internal_data_publisher()
                    .subscribe(
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
                                emit signal_strategy_internal(name, ts, data);
                            }));
    m_subscriptions.push_back(m_strategy_instance->signals_publisher().subscribe(
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
                emit signal_signal(signal);
            }));
    m_subscriptions.push_back(m_strategy_instance->depo_publisher().subscribe(
            [this](const auto & vec) {
                auto & plot = get_or_create_chart(m_depo_chart_name);
                plot.push_series_vector("depo", vec);
            },
            [&](std::chrono::milliseconds ts, double depo) {
                emit signal_depo(ts, depo);
            }));
    m_subscriptions.push_back(m_strategy_instance->strategy_result_publisher().subscribe(
            [&](const StrategyResult & result) {
                const auto status = m_strategy_instance->status_publisher().get();
                switch (status) {
                case WorkStatus::Backtesting: break;
                default: {
                    emit signal_result(result);
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
}

void MainWindow::optimized_config_slot(const JsonStrategyConfig & config)
{
    DoubleSmaStrategyConfig new_config(config); // TODO there is not only DSMA
    for (auto it = config.get().begin(); it != config.get().end(); ++it) {
        auto spinbox_it = m_strategy_parameters_spinboxes.find(it.key());
        if (spinbox_it == m_strategy_parameters_spinboxes.end()) {
            std::cout << "ERROR: Could not find spinbox for " << it.key() << std::endl;
            continue;
        }
        m_strategy_parameters_spinboxes.at(it.key())->setValue(it.value().get<double>());
    }
}

std::optional<Timerange> MainWindow::get_timerange() const
{
    const auto start = std::chrono::milliseconds{ui->dt_from->dateTime().toMSecsSinceEpoch()};
    const auto work_hours = std::chrono::hours{ui->sb_work_hours->value()};
    const auto end = std::chrono::milliseconds{start + work_hours};

    if (start >= end) {
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
            return get_entry_config_from_ui();
        }
    };
    const auto exit_config = [&]() -> std::variant<JsonStrategyMetaInfo, TpslExitStrategyConfig> {
        if (ui->cb_optimize_exit->isChecked()) {
            return exit_strategy_meta_info.value();
        }
        else {
            return get_exit_config_from_ui();
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
        emit signal_optimized_config(best_config.value().first);
        std::cout << "Best config: " << best_config.value().first << "; " << best_config.value().second << std::endl;
    });
    t.detach();
}

void MainWindow::setup_specific_parameters(const JsonStrategyMetaInfo & strategy_parameters)
{
    qDeleteAll(ui->gb_specific_parameters->children());
    m_strategy_parameters_spinboxes.clear();

    m_last_set_strategy_parameters = strategy_parameters;
    if (!strategy_parameters.got_parameters()) {
        return;
    }
    const auto params = strategy_parameters.get()["parameters"].get<std::vector<nlohmann::json>>();
    auto * top_layout = new QVBoxLayout();

    auto * name_layout = new QHBoxLayout();
    auto * strategy_name_label = new QLabel(strategy_parameters.get()["strategy_name"].get<std::string>().c_str());
    name_layout->addWidget(strategy_name_label);
    top_layout->addItem(name_layout);
    for (const auto & param : params) {
        const auto name = param["name"].get<std::string>();
        const auto max_value = param["max_value"].get<double>();
        const auto step = param["step"].get<double>();
        const auto min_value = param["min_value"].get<double>();

        auto * layout = new QHBoxLayout();
        auto * label = new QLabel(name.c_str());
        auto * double_spin_box = new QDoubleSpinBox();
        double_spin_box->setMinimum(min_value);
        double_spin_box->setSingleStep(step);
        double_spin_box->setMaximum(max_value);

        double_spin_box->setValue(min_value);
        m_strategy_parameters_values[name] = min_value;
        m_strategy_parameters_spinboxes[name] = double_spin_box;
        connect(
                double_spin_box,
                &QDoubleSpinBox::valueChanged,
                this,
                [this, name](double value) {
                    on_strategy_parameters_changed(name, value);
                });

        label->setText(name.c_str());
        layout->addWidget(label);
        layout->addWidget(double_spin_box);
        top_layout->addItem(layout);
    }
    ui->gb_specific_parameters->setLayout(top_layout);
}

JsonStrategyConfig MainWindow::get_entry_config_from_ui() const
{
    nlohmann::json config;
    for (const auto & [name, value] : m_strategy_parameters_values) {
        config[name] = value;
    }
    return config;
}

TpslExitStrategyConfig MainWindow::get_exit_config_from_ui() const
{
    double risk = ui->dsb_risk->value();
    double risk_reward_ratio = ui->dsb_risk_reward->value();
    return {risk, risk_reward_ratio};
}

std::optional<JsonStrategyMetaInfo> MainWindow::get_strategy_parameters() const
{
    const auto json_data = StrategyFactory::get_meta_info(ui->cb_strategy->currentText().toStdString());
    return json_data;
}

void MainWindow::on_strategy_parameters_changed(const std::string & name, double value)
{
    m_strategy_parameters_values[name] = value;
}

void MainWindow::on_cb_strategy_currentTextChanged(const QString &)
{
    const auto params_opt = get_strategy_parameters();
    if (params_opt) {
        setup_specific_parameters(*params_opt);
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

void MainWindow::on_pb_test_clicked()
{
    // MarketOrder order("BTCUSDT", 0.002, Side::Sell);
    // m_trading_gateway.send_order_sync(order);
}
