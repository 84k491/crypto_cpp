#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "ByBitGateway.h"
#include "DoubleSmaStrategy.h"

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
    const auto md_interval = end - start;

    auto * prices = new QLineSeries();
    auto * buy_signals = new QScatterSeries();
    buy_signals->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    buy_signals->setMarkerSize(15.0);
    auto * sell_signals = new QScatterSeries();
    sell_signals->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
    sell_signals->setMarkerSize(11.0);

    DoubleSmaStrategy strategy(std::chrono::minutes{25}, std::chrono::minutes{5});
    gateway.set_on_kline_received([&](std::pair<std::chrono::milliseconds, OHLC> ts_and_ohlc) {
        const auto & [ts, ohlc] = ts_and_ohlc;
        const auto signal = strategy.push_price({ts, ohlc.close});
        prices->append(static_cast<double>(ts.count()), ohlc.close);
        if (signal.has_value()) {
            if (signal.value().side == Side::Buy) {
                buy_signals->append(static_cast<double>(ts.count()), ohlc.close);
            }
            else {
                sell_signals->append(static_cast<double>(ts.count()), ohlc.close);
            }
        }
    });
    gateway.get_klines(start, end);

    auto * slow_avg = new QLineSeries();
    for (const auto & [ts, avg_slow] : strategy.get_slow_avg_history()) {
        slow_avg->append(static_cast<double>(ts.count()), avg_slow);
    }

    auto * fast_avg = new QLineSeries();
    for (const auto & [ts, avg_fast] : strategy.get_fast_avg_history()) {
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
}

MainWindow::~MainWindow()
{
    delete ui;
}
