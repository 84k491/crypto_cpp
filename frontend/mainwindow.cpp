#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "DoubleSmaStrategy.h"
#include "JsonStrategyConfig.h"
#include "Optimizer.h"
#include "StrategyInstance.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <qboxlayout.h>
#include <qscatterseries.h>
#include <qspinbox.h>
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
            &MainWindow::optimized_config_slot);

    ui->verticalLayout_graph->addWidget(m_chartView);

    ui->sb_work_hours->setValue(saved_state.m_work_hours);
    if (saved_state.m_start_ts_unix_time != 0) {
        ui->dt_from->setDateTime(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(saved_state.m_start_ts_unix_time)));
    }

    ui->cb_strategy->addItem("DoubleSma");
    ui->cb_strategy->addItem("BollingerBands");

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

    const auto strategy_name = ui->cb_strategy->currentText().toStdString();
    const auto strategy_ptr_opt = StrategyFactory::build_strategy(strategy_name, get_config_from_ui());
    if (!strategy_ptr_opt.has_value() || !strategy_ptr_opt.value() || !strategy_ptr_opt.value()->is_valid()) {
        std::cout << "ERROR Failed to build strategy" << std::endl;
        return;
    }

    std::thread t([this, timerange, strategy = strategy_ptr_opt.value()]() {
        StrategyInstance strategy_instance(
                timerange,
                strategy,
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

void MainWindow::optimized_config_slot(const JsonStrategyConfig & config)
{
    DoubleSmaStrategyConfig new_config(config);
    // TODO
    // ui->sb_slow_interval->setValue(std::chrono::duration_cast<std::chrono::minutes>(new_config.m_slow_interval).count());
    // ui->sb_fast_interval->setValue(std::chrono::duration_cast<std::chrono::minutes>(new_config.m_fast_interval).count());
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
    const auto json_data = get_strategy_parameters();
    const auto timerange_opt = get_timerange();
    if (!timerange_opt || !json_data) {
        std::cout << "ERROR no value in required optional" << std::endl;
        return;
    }
    const auto & timerange = *timerange_opt;

    std::thread t([this, timerange, json_data]() {
        Optimizer<DoubleSmaStrategy> optimizer(m_gateway, timerange, *json_data);
        optimizer.subscribe_for_passed_check([this](unsigned i, unsigned total) {
            // std::cout << "Passed check: " << i << "/" << total << std::endl;
        });

        const auto best_config = optimizer.optimize();
        if (!best_config.has_value()) {
            std::cout << "ERROR no best config" << std::endl;
            return;
        }
        emit signal_optimized_config(best_config.value());
        std::cout << "Best config: " << best_config.value().get() << std::endl;
    });
    t.detach();
}

void MainWindow::setup_specific_parameters(JsonStrategyMetaInfo strategy_parameters)
{
    delete ui->gb_specific_parameters->layout();

    m_last_set_strategy_parameters = strategy_parameters;
    const auto params = strategy_parameters.get()["parameters"].get<std::vector<nlohmann::json>>();
    auto * top_layout = new QVBoxLayout();

    auto * name_layout = new QHBoxLayout();
    auto * strategy_name_label = new QLabel(strategy_parameters.get()["strategy_name"].get<std::string>().c_str());
    name_layout->addWidget(strategy_name_label);
    top_layout->addItem(name_layout);
    for (const auto & param : params) {
        const auto name = param["name"].get<std::string>();
        const auto max_value = param["max_value"].get<int>();
        const auto min_value = param["min_value"].get<int>();

        auto * layout = new QHBoxLayout();
        auto * label = new QLabel(name.c_str());
        auto * double_spin_box = new QDoubleSpinBox();
        double_spin_box->setMinimum(min_value);
        double_spin_box->setMaximum(max_value);

        double_spin_box->setValue(min_value);
        m_strategy_parameters_values[name] = min_value;
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
    qDeleteAll(ui->gb_specific_parameters->children());
    ui->gb_specific_parameters->setLayout(top_layout);
}

JsonStrategyConfig MainWindow::get_config_from_ui() const
{
    nlohmann::json config;
    for (const auto & [name, value] : m_strategy_parameters_values) {
        config[name] = value;
    }
    return config;
}

std::optional<JsonStrategyMetaInfo> MainWindow::get_strategy_parameters() const
{
    const auto json_data = StrategyFactory::get_meta_info(ui->cb_strategy->currentText().toStdString());
    return json_data;
}

void MainWindow::on_strategy_parameters_changed(const std::string & name, double value)
{
    std::cout << "on_strategy_parameters_changed: " << name << " " << value << std::endl;
    m_strategy_parameters_values[name] = value;
}

void MainWindow::on_cb_strategy_currentTextChanged(const QString & arg1)
{
    const auto params_opt = get_strategy_parameters();
    if (params_opt) {
        setup_specific_parameters(*params_opt);
    }
}
