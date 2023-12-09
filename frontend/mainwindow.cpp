#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "DoubleSmaStrategy.h"
#include "StrategyInstance.h"

#include <qscatterseries.h>

class SavedStateUi
{
public:
    SavedStateUi() = default;

    long m_start_ts_unix_time = {};
    long m_end_ts_unix_time = {};
};

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

    ui->verticalLayout_graph->addWidget(m_chartView);
    std::cout << "End of mainwindow constructor" << std::endl;
}

void MainWindow::on_pushButton_clicked()
{
    const auto start = std::chrono::milliseconds{ui->dt_from->dateTime().toMSecsSinceEpoch()};
    const auto work_hours = std::chrono::hours{ui->sb_work_hours->value()};
    const auto end = std::chrono::milliseconds{start + work_hours};

    if (start >= end) {
        std::cout << "ERROR Invalid timerange" << std::endl;
        return;
    }
    Timerange timerange{start, end};

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
        strategy_instance.run();
        std::cout << "strategy finished" << std::endl;
    });
    t.detach();
}

MainWindow::~MainWindow()
{
    const auto start = std::chrono::milliseconds{ui->dt_from->dateTime().toMSecsSinceEpoch()};
    const auto work_hours = std::chrono::hours{ui->sb_work_hours->value()};

    delete ui;
}
