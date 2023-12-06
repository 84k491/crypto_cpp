#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "DoubleSmaStrategy.h"
#include "StrategyInstance.h"

#include <qscatterseries.h>

MainWindow::MainWindow(QWidget * parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow), m_chartView(new DragableChart())
{
    ui->setupUi(this);

    m_gateway.subscribe_for_klines([&](std::pair<std::chrono::milliseconds, OHLC> ts_and_ohlc) {
        const auto & [ts, ohlc] = ts_and_ohlc;
        m_chartView->push_price(ts, ohlc.close);
    });

    ui->verticalLayout_graph->addWidget(m_chartView);
    std::cout << "End of mainwindow constructor" << std::endl;
}

void MainWindow::on_pushButton_clicked()
{
    run_strategy();
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
        m_chartView->push_signal(signal);
    });
    strategy_instance.run();

    const auto internal_data = strategy_instance.get_strategy_internal_data_history();
    for (const auto & [name, data_vector] : internal_data) {
        for (const auto & [ts, data] : data_vector) {
            m_chartView->push_strategy_internal(name, ts, data);
        }
    }
    std::cout << "strategy finished" << std::endl;
}
