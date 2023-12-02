#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "ByBitGateway.h"
#include "DoubleSmaStrategy.h"
#include "StrategyInstance.h"

#include <QScatterSeries>
#include <QtCharts>
#include <qscatterseries.h>

MainWindow::MainWindow(QWidget * parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ByBitGateway gateway;
    const auto start = std::chrono::milliseconds{1700556200000};
    const auto end = std::chrono::milliseconds{1700735400000};
    Timerange timerange(start, end);

    auto * prices = new QLineSeries();
    auto * buy_signals = new QScatterSeries();
    buy_signals->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    buy_signals->setMarkerSize(15.0);
    auto * sell_signals = new QScatterSeries();
    sell_signals->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
    sell_signals->setMarkerSize(11.0);

    DoubleSmaStrategyConfig config{std::chrono::minutes{55}, std::chrono::minutes{21}};

    StrategyInstance strategy_instance(
            Timerange{start, end},
            DoubleSmaStrategyConfig{
                    std::chrono::minutes{25},
                    std::chrono::minutes{5}},
            gateway);

    gateway.subscribe_for_klines([&](std::pair<std::chrono::milliseconds, OHLC> ts_and_ohlc) {
        const auto & [ts, ohlc] = ts_and_ohlc;
        prices->append(static_cast<double>(ts.count()), ohlc.close);
    });
    strategy_instance.subscribe_for_signals([&](const Signal & signal) {
        if (signal.side == Side::Buy) {
            buy_signals->append(static_cast<double>(signal.timestamp.count()), signal.price);
        }
        else {
            sell_signals->append(static_cast<double>(signal.timestamp.count()), signal.price);
        }
    });
    strategy_instance.run();

    std::cout << "Requesting internal data" << std::endl;
    const auto internal_data = strategy_instance.get_strategy_internal_data_history();
    auto * slow_avg = new QLineSeries();
    for (const auto & [ts, avg_slow] : internal_data.find("slow_avg_history")->second) {
        slow_avg->append(static_cast<double>(ts.count()), avg_slow);
    }

    auto * fast_avg = new QLineSeries();
    for (const auto & [ts, avg_fast] : internal_data.find("fast_avg_history")->second) {
        fast_avg->append(static_cast<double>(ts.count()), avg_fast);
    }

    auto * chart = new QChart();
    chart->legend()->hide();
    chart->addSeries(prices);
    chart->addSeries(slow_avg);
    chart->addSeries(fast_avg);
    chart->addSeries(buy_signals);
    chart->addSeries(sell_signals);

    auto * axisX = new QDateTimeAxis;
    axisX->setTickCount(10);
    axisX->setFormat("hh:mm:ss");
    axisX->setTitleText("Date");
    chart->createDefaultAxes();
    chart->addAxis(axisX, Qt::AlignBottom);
    prices->attachAxis(axisX);
    slow_avg->attachAxis(axisX);
    fast_avg->attachAxis(axisX);
    buy_signals->attachAxis(axisX);
    sell_signals->attachAxis(axisX);

    chart->setTitle("Simple line chart example");

    auto * chartView = new QChartView(chart);
    chartView->setRubberBand(QChartView::RectangleRubberBand);
    chartView->setRenderHint(QPainter::Antialiasing);

    centralWidget()->layout()->addWidget(chartView);

    std::cout << "End of mainwindow constructor" << std::endl;
}

MainWindow::~MainWindow()
{
    delete ui;
}
