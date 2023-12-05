#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "DoubleSmaStrategy.h"
#include "StrategyInstance.h"

#include <qscatterseries.h>

MainWindow::MainWindow(QWidget * parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , chart(new QChart())
    , prices(new QLineSeries())
    , buy_signals(new QScatterSeries())
    , sell_signals(new QScatterSeries())
    , slow_avg(new QLineSeries())
    , fast_avg(new QLineSeries())
{
    ui->setupUi(this);

    gateway.subscribe_for_klines([&](std::pair<std::chrono::milliseconds, OHLC> ts_and_ohlc) {
        const auto & [ts, ohlc] = ts_and_ohlc;
        prices->append(static_cast<double>(ts.count()), ohlc.close);
        update_axes(ts, ohlc.close);
    });

    buy_signals->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    buy_signals->setMarkerSize(15.0);
    sell_signals->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
    sell_signals->setMarkerSize(11.0);

    std::cout << "End of mainwindow constructor" << std::endl;
}

void MainWindow::setupChart()
{
    axisX = new QDateTimeAxis;
    axisX->setTickCount(10);
    axisX->setFormat("hh:mm:ss");
    axisX->setTitleText("Date");

    chart = new QChart();
    chart->legend()->hide();
    // add series to chart before attaching axis
    chart->addSeries(prices);
    chart->addSeries(slow_avg);
    chart->addSeries(fast_avg);
    chart->addSeries(buy_signals);
    chart->addSeries(sell_signals);
    chart->createDefaultAxes();
    chart->removeAxis(chart->axisX());
    chart->addAxis(axisX, Qt::AlignBottom);
    prices->attachAxis(axisX);
    slow_avg->attachAxis(axisX);
    fast_avg->attachAxis(axisX);
    buy_signals->attachAxis(axisX);
    sell_signals->attachAxis(axisX);

    chart->setTitle("Simple line chart example");

    chartView = new DragableChart(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    ui->verticalLayout_graph->addWidget(chartView);
    run_strategy();
}

void MainWindow::update_axes(std::chrono::milliseconds x, double y)
{
    if (x.count() < x_min || x.count() > x_max) {
        if (x.count() < x_min) {
            x_min = x.count();
        }
        if (x.count() > x_max) {
            x_max = x.count();
        }
        axisX->setRange(QDateTime::fromMSecsSinceEpoch(x_min), QDateTime::fromMSecsSinceEpoch(x_max));
    }

    if (y < y_min || y > y_max) {
        if (y < y_min) {
            y_min = y;
        }
        if (y > y_max) {
            y_max = y;
        }
        chart->axisY()->setRange(y_min, y_max);
    }
}

void MainWindow::on_pushButton_clicked()
{
    setupChart();
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
            gateway);

    strategy_instance.subscribe_for_signals([&](const Signal & signal) {
        if (signal.side == Side::Buy) {
            buy_signals->append(static_cast<double>(signal.timestamp.count()), signal.price);
        }
        else {
            sell_signals->append(static_cast<double>(signal.timestamp.count()), signal.price);
        }
    });
    strategy_instance.run();

    const auto internal_data = strategy_instance.get_strategy_internal_data_history();
    for (const auto & [ts, avg_slow] : internal_data.find("slow_avg_history")->second) {
        slow_avg->append(static_cast<double>(ts.count()), avg_slow);
    }

    for (const auto & [ts, avg_fast] : internal_data.find("fast_avg_history")->second) {
        fast_avg->append(static_cast<double>(ts.count()), avg_fast);
    }

    // chartView->update();
}
