#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "DoubleSmaStrategy.h"
#include "StrategyInstance.h"

#include <qscatterseries.h>

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

    m_gateway.subscribe_for_klines([&](std::pair<std::chrono::milliseconds, OHLC> ts_and_ohlc) {
        const auto & [ts, ohlc] = ts_and_ohlc;
        emit signal_price(ts, ohlc.close);
    });

    ui->verticalLayout_graph->addWidget(m_chartView);
    std::cout << "End of mainwindow constructor" << std::endl;
}

void MainWindow::on_pushButton_clicked()
{
    std::thread t([&]() { run_strategy(); });
    t.detach();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::run_strategy()
{
    DoubleSmaStrategyConfig config{std::chrono::minutes{55}, std::chrono::minutes{21}};
    const auto start = std::chrono::milliseconds{1700556200000};
    const auto end = std::chrono::milliseconds{1700735400000};
    StrategyInstance strategy_instance(
            Timerange{start, end},
            config,
            m_gateway);

    strategy_instance.subscribe_for_signals([&](const Signal & signal) {
        emit signal_signal(signal);
    });
    strategy_instance.run();

    const auto internal_data = strategy_instance.get_strategy_internal_data_history();
    for (const auto & [name, data_vector] : internal_data) {
        for (const auto & [ts, data] : data_vector) {
            emit signal_strategy_internal(name, ts, data);
        }
    }
    std::cout << "strategy finished" << std::endl;
}
