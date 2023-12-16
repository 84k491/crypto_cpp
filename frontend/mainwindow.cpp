#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "DoubleSmaStrategy.h"
#include "Optimizer.h"
#include "StrategyInstance.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <memory>
#include <qboxlayout.h>
#include <qscatterseries.h>
#include <qtypes.h>

MainWindow::MainWindow(QWidget * parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_chartView(new DragableChart())
{
    ui->setupUi(this);

    connect(this,
            &MainWindow::signal_price,
            m_chartView,
            &DragableChart::on_push_price);

    connect(this,
            &MainWindow::signal_signal,
            m_chartView,
            &DragableChart::on_push_signal);

    connect(this,
            &MainWindow::signal_strategy_internal,
            m_chartView,
            &DragableChart::on_push_strategy_internal);
    connect(this,
            &MainWindow::signal_depo,
            m_chartView,
            &DragableChart::on_push_depo);
    connect(this,
            &MainWindow::signal_result,
            this,
            &MainWindow::render_result);
    connect(this,
            &MainWindow::signal_optimized_config,
            this,
            &MainWindow::on_optimized_config);

    ui->verticalLayout_graph->addWidget(m_chartView);

    ui->sb_work_hours->setValue(saved_state.m_work_hours);
    if (saved_state.m_start_ts_unix_time != 0) {
        ui->dt_from->setDateTime(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(saved_state.m_start_ts_unix_time)));
    }
    std::cout << "End of mainwindow constructor" << std::endl;
}

void MainWindow::on_pushButton_clicked()
{
    m_chartView->clear();
    const auto timerange_opt = get_timerange();
    if (!timerange_opt) {
        std::cout << "ERROR Invalid timerange" << std::endl;
        return;
    }
    const auto & timerange = *timerange_opt;

    const auto slow_interval = std::chrono::minutes{ui->sb_slow_interval->value()};
    const auto fast_interval = std::chrono::minutes{ui->sb_fast_interval->value()};
    DoubleSmaStrategyConfig config{slow_interval, fast_interval};
    if (!config.is_valid()) {
        std::cout << "ERROR Invalid config" << std::endl;
        return;
    }

    std::thread t([this, timerange, config]() {
        StrategyInstance strategy_instance(
                timerange,
                config,
                m_gateway);

        strategy_instance.subscribe_for_klines([&](std::pair<std::chrono::milliseconds, OHLC> ts_and_ohlc) {
            const auto & [ts, ohlc] = ts_and_ohlc;
            emit signal_price(ts, ohlc.close);
        });
        strategy_instance.subscribe_for_signals([&](const Signal & signal) {
            emit signal_signal(signal);
        });
        strategy_instance.subscribe_for_strategy_internal([this](const std::string & name, std::chrono::milliseconds ts, double data) {
            emit signal_strategy_internal(name, ts, data);
        });
        strategy_instance.subscribe_for_depo([&](std::chrono::milliseconds ts, double value) {
            emit signal_depo(ts, value);
        });
        strategy_instance.run();
        const auto result = strategy_instance.get_strategy_result();
        emit signal_result(result);
        std::cout << "strategy finished" << std::endl;
    });
    t.detach();
}

MainWindow::~MainWindow()
{
    const auto start = std::chrono::milliseconds{ui->dt_from->dateTime().toMSecsSinceEpoch()};
    const auto work_hours = std::chrono::hours{ui->sb_work_hours->value()};

    saved_state.m_start_ts_unix_time = start.count();
    saved_state.m_work_hours = static_cast<int>(work_hours.count());

    delete ui;
}

void MainWindow::render_result(StrategyResult result)
{
    ui->lb_position_amount->setText(QString::number(result.position_currency_amount));
    ui->lb_final_profit->setText(QString::number(result.final_profit));
    ui->lb_trades_count->setText(QString::number(result.trades_count));
    ui->lb_fees_paid->setText(QString::number(result.fees_paid));
    ui->lb_profit_per_trade->setText(QString::number(result.profit_per_trade));
    ui->lb_best_profit->setText(QString::number(result.best_profit_trade));
    ui->lb_worst_loss->setText(QString::number(result.worst_loss_trade));
    ui->lb_max_depo->setText(QString::number(result.max_depo));
    ui->lb_min_depo->setText(QString::number(result.min_depo));
    ui->lb_longest_profit_pos->setText(QString::number(result.longest_profit_trade_time.count()));
    ui->lb_longest_loss_pos->setText(QString::number(result.longest_loss_trade_time.count()));
}

void MainWindow::on_optimized_config(nlohmann::json config)
{
    DoubleSmaStrategyConfig new_config(config);
    ui->sb_slow_interval->setValue(std::chrono::duration_cast<std::chrono::minutes>(new_config.m_slow_interval).count());
    ui->sb_fast_interval->setValue(std::chrono::duration_cast<std::chrono::minutes>(new_config.m_fast_interval).count());
}

std::optional<Timerange> MainWindow::get_timerange()
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
    const auto timerange_opt = get_timerange();
    if (!timerange_opt) {
        std::cout << "ERROR Invalid timerange" << std::endl;
        return;
    }
    const auto & timerange = *timerange_opt;

    const auto json_data = [&]() -> std::optional<nlohmann::json> {
        const std::string json_file_path = "./strategy_parameters/DoubleSma.json";
        std::ifstream file(json_file_path);
        if (file.is_open()) {
            nlohmann::json json_data;
            file >> json_data;
            file.close();
            return json_data;
        }
        std::cout << "ERROR: Failed to open JSON file: " << json_file_path << std::endl;
        return {};
    }();

    if (!json_data) {
        return;
    }

    std::thread t([this, timerange, json_data]() {
        Optimizer<DoubleSmaStrategy> optimizer(m_gateway, timerange, *json_data);
        optimizer.subscribe_for_passed_check([this](unsigned i, unsigned total) {
            // std::cout << "Passed check: " << i << "/" << total << std::endl;
        });

        const auto best_config = optimizer.optimize();
        emit signal_optimized_config(best_config);
        std::cout << "Best config: " << best_config << std::endl;
    });
    t.detach();
}
