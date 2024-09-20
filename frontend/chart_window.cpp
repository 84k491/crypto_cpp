#include "chart_window.h"

#include "Logger.h"
#include "ui_chart_window.h"

ChartWindow::ChartWindow(const std::shared_ptr<StrategyInstance> & strategy, QWidget * parent)
    : QWidget(parent)
    , ui(new Ui::ChartWindow)
    , m_event_consumer(std::make_shared<ChartWindowEventConsumer>(*this))
    , m_strategy_instance(strategy)
{
    ui->setupUi(this);
    this->setWindowTitle("Charts");
    connect(this, &ChartWindow::signal_lambda, this, &ChartWindow::on_lambda);
    subscribe_to_strategy();
}

ChartWindow::~ChartWindow()
{
    delete ui;
}

MultiSeriesChart & ChartWindow::get_or_create_chart(const std::string & chart_name)
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

void ChartWindow::subscribe_to_strategy()
{
    UNWRAP_RET_VOID(str_instance, m_strategy_instance.lock());

    m_subscriptions.push_back(str_instance.klines_publisher().subscribe(
            m_event_consumer,
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
                get_or_create_chart(m_price_chart_name).push_series_value("price", ts, ohlc.close);
            }));
    m_subscriptions.push_back(str_instance.tpsl_publisher().subscribe(
            m_event_consumer,
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
    m_subscriptions.push_back(str_instance.trailing_stop_publisher().subscribe(
            m_event_consumer,
            [this](const std::vector<std::pair<std::chrono::milliseconds, StopLoss>> & input_vec) {
                std::vector<std::pair<std::chrono::milliseconds, double>> tsl_vec;
                tsl_vec.reserve(input_vec.size());
                for (const auto & [ts, tsl] : input_vec) {
                    tsl_vec.emplace_back(ts, tsl.stop_price());
                }
                auto & plot = get_or_create_chart(m_price_chart_name);
                plot.push_scatter_series_vector("trailing_stop_loss", tsl_vec);
            },
            [&](std::chrono::milliseconds ts, const StopLoss & stop_loss) {
                get_or_create_chart(m_price_chart_name).push_stop_loss(ts, stop_loss.stop_price());
            }));
    m_subscriptions.push_back(
            str_instance
                    .strategy_internal_data_publisher()
                    .subscribe(
                            m_event_consumer,
                            [this](const std::vector<
                                    std::pair<
                                            std::chrono::milliseconds,
                                            std::tuple<std::string, std::string, double>>> & vec) {
                                // chart_name -> series_name -> timestamp, value
                                std::map<std::string,
                                         std::map<std::string,
                                                  std::vector<std::pair<std::chrono::milliseconds, double>>>>
                                        vec_map;

                                for (const auto & [ts, v] : vec) {
                                    const auto & [chart_name, series_name, value] = v;
                                    vec_map[chart_name][series_name].emplace_back(ts, value);
                                }
                                for (const auto & [chart_name, series_map] : vec_map) {
                                    for (const auto & [series_name, out_vec] : series_map) {
                                        auto & plot = get_or_create_chart(chart_name);
                                        plot.push_series_vector(series_name, out_vec);
                                    }
                                }
                            },
                            [&](
                                    std::chrono::milliseconds ts,
                                    const std::tuple<const std::string,
                                                     const std::string,
                                                     double> & data_pair) {
                                const auto & [chart_name, name, data] = data_pair;
                                get_or_create_chart(chart_name).push_series_value(name, ts, data);
                            }));
    m_subscriptions.push_back(str_instance.signals_publisher().subscribe(
            m_event_consumer,
            [this](const std::vector<std::pair<std::chrono::milliseconds, Signal>> & input_vec) {
                std::vector<std::pair<std::chrono::milliseconds, double>> buy, sell;
                for (const auto & [ts, signal] : input_vec) {
                    switch (signal.side) {
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
    m_subscriptions.push_back(str_instance.depo_publisher().subscribe(
            m_event_consumer,
            [this](const auto & vec) {
                auto & plot = get_or_create_chart(m_depo_chart_name);
                plot.push_series_vector("depo", vec);
            },
            [&](std::chrono::milliseconds ts, double depo) {
                get_or_create_chart(m_depo_chart_name).push_series_value("depo", ts, depo);
            }));
}

bool ChartWindowEventConsumer::push_to_queue(std::any value)
{
    auto & lambda_event = std::any_cast<LambdaEvent &>(value);
    m_cw.signal_lambda(std::move(lambda_event.func));
    return true;
}

bool ChartWindowEventConsumer::push_to_queue_delayed(std::chrono::milliseconds, const std::any)
{
    throw std::runtime_error("Not implemented");
}

void ChartWindow::on_lambda(const std::function<void()> & lambda)
{
    lambda();
}
