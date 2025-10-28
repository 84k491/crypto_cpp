#include "chart_window.h"

#include "ConditionalOrders.h"
#include "Enums.h"
#include "OrdinaryLeastSquares.h"
#include "ui_chart_window.h"

ChartWindow::ChartWindow(
        std::string window_name,
        const std::weak_ptr<StrategyInstance> & strategy,
        std::chrono::milliseconds start_ts,
        std::chrono::milliseconds end_ts,
        bool render_depo,
        QWidget * parent)
    : QWidget(parent)
    , m_start_ts(start_ts)
    , m_end_ts(end_ts)
    , m_render_depo(render_depo)
    , ui(new Ui::ChartWindow)
    , m_event_consumer(*this)
    , m_strategy_instance(strategy)
    , m_sub(m_event_consumer)
{
    ui->setupUi(this);
    m_holder_widget = new QWidget(ui->scrollArea);
    m_layout = new QVBoxLayout(m_holder_widget);
    ui->scrollArea->setWidget(m_holder_widget);
    ui->scrollArea->setWidgetResizable(true);
    this->setWindowTitle(window_name.c_str());
    connect(this, &ChartWindow::signal_lambda, this, &ChartWindow::on_lambda);
    subscribe_to_strategy();
}

ChartWindow::~ChartWindow()
{
    delete ui;
}

int get_child_height(int parent_height, double factor)
{
    return static_cast<int>(std::roundl(parent_height * factor));
}

void ChartWindow::resizeEvent(QResizeEvent * ev)
{
    for (auto & [name, chart] : m_charts) {
        chart->setFixedHeight(get_child_height(ev->size().height(), height_factor));
    }
}

MultiSeriesChart & ChartWindow::get_or_create_chart(const std::string & chart_name)
{
    if (auto it = m_charts.find(chart_name); it != m_charts.end()) {
        return *it->second;
    }
    auto * new_chart = new MultiSeriesChart();
    new_chart->setFixedHeight(get_child_height(height(), height_factor));
    new_chart->set_title(chart_name);
    m_charts[chart_name] = new_chart;
    m_layout->addWidget(new_chart);
    return *new_chart;
}

bool ChartWindow::ts_in_range(std::chrono::milliseconds ts) const
{
    const bool has_start_ts = m_start_ts.count() != 0;
    const bool has_end_ts = m_end_ts.count() != 0;

    // Logger::logf<LogLevel::Debug>("ts_in_range: {} < {} < {}", m_start_ts.count(), ts.count(), m_end_ts.count());

    if (!has_start_ts && !has_end_ts) {
        return true;
    }
    if (has_start_ts && ts < m_start_ts) {
        return false;
    }
    if (has_end_ts && m_end_ts < ts) {
        return false;
    }
    return true;
}

void ChartWindow::subscribe_to_strategy()
{
    UNWRAP_RET_VOID(str_instance, m_strategy_instance.lock());

    m_sub.subscribe(
            str_instance.candle_channel(), [this](const auto & vec) {
                std::list<Candle> candles;
                for (const auto & [ts, candle] : vec) {
                    if (!ts_in_range(ts)) {
                        continue;
                    }
                    candles.push_back(candle);
                }
                auto & plot = get_or_create_chart(m_price_chart_name);
                plot.push_candle_vector(candles); }, [&](std::chrono::milliseconds ts, const Candle & candle) {
                if (!ts_in_range(ts)) {
                    return;
                }
                auto & plot = get_or_create_chart(m_price_chart_name);
                plot.push_candle(candle); });

    m_sub.subscribe(
            str_instance.price_levels_channel(),
            [this](const auto & vec) {
                std::vector<std::pair<std::chrono::milliseconds, double>> fee_profit;
                std::vector<std::pair<std::chrono::milliseconds, double>> no_loss;
                std::vector<std::pair<std::chrono::milliseconds, double>> fee_loss;
                for (const auto & [ts, l] : vec) {
                    if (!ts_in_range(ts)) {
                        continue;
                    }
                    fee_profit.push_back({ts, l.fee_profit_price});
                    no_loss.push_back({ts, l.no_loss_price});
                    fee_loss.push_back({ts, l.fee_loss_price});
                }
                auto & plot = get_or_create_chart(m_price_chart_name);
                plot.push_scatter_series_vector("fee_profit_price", fee_profit);
                plot.push_scatter_series_vector("no_loss_price", no_loss);
                // plot.push_scatter_series_vector("fee_loss_price", fee_loss); // there will be a trade marker here
            },
            [&](std::chrono::milliseconds ts, const ProfitPriceLevels &) {
                if (!ts_in_range(ts)) {
                    return;
                }
                // TODO impelment
            });

    m_sub.subscribe(
            str_instance.tpsl_channel(),
            [this](const std::list<std::pair<std::chrono::milliseconds, TpslFullPos::Prices>> & input_vec) {
                std::vector<std::pair<std::chrono::milliseconds, double>> tp, sl;
                for (const auto & [ts, tpsl] : input_vec) {
                    if (!ts_in_range(ts)) {
                        continue;
                    }
                    tp.emplace_back(ts, tpsl.take_profit_price);
                    sl.emplace_back(ts, tpsl.stop_loss_price);
                }
                auto & plot = get_or_create_chart(m_price_chart_name);
                plot.push_scatter_series_vector("take_profit", tp);
                plot.push_scatter_series_vector("stop_loss", sl);
            },
            [&](std::chrono::milliseconds ts, const TpslFullPos::Prices & tpsl) {
                if (!ts_in_range(ts)) {
                    return;
                }
                get_or_create_chart(m_price_chart_name).push_tpsl(ts, tpsl);
            });
    m_sub.subscribe(
            str_instance.trailing_stop_channel(),
            [this](const std::list<std::pair<std::chrono::milliseconds, StopLoss>> & input_vec) {
                std::vector<std::pair<std::chrono::milliseconds, double>> tsl_vec;
                tsl_vec.reserve(input_vec.size());
                for (const auto & [ts, tsl] : input_vec) {
                    if (!ts_in_range(ts)) {
                        continue;
                    }
                    tsl_vec.emplace_back(ts, tsl.stop_price());
                }
                auto & plot = get_or_create_chart(m_price_chart_name);
                plot.push_scatter_series_vector("trailing_stop_loss", tsl_vec);
            },
            [&](std::chrono::milliseconds ts, const StopLoss & stop_loss) {
                if (!ts_in_range(ts)) {
                    return;
                }
                get_or_create_chart(m_price_chart_name).push_stop_loss(ts, stop_loss.stop_price());
            });
    m_sub.subscribe(
            str_instance.strategy_internal_data_channel(),
            [this](const std::list<
                    std::pair<
                            std::chrono::milliseconds,
                            StrategyInternalData>> & vec) {
                // chart_name -> series_name -> timestamp, value
                std::map<std::string,
                         std::map<std::string,
                                  std::list<std::pair<std::chrono::milliseconds, double>>>>
                        vec_map;

                for (const auto & [ts, v] : vec) {
                    if (!ts_in_range(ts)) {
                        continue;
                    }
                    const auto & [chart_name, series_name, value] = v;
                    vec_map[std::string{chart_name}][std::string{series_name}].emplace_back(ts, value);
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
                    const StrategyInternalData & data_pair) {
                if (!ts_in_range(ts)) {
                    return;
                }
                const auto & [chart_name, name, data] = data_pair;
                get_or_create_chart(std::string{chart_name}).push_series_value(std::string{name}, ts, data);
            });
    m_sub.subscribe(
            str_instance.trade_channel(),
            [this](const std::list<std::pair<std::chrono::milliseconds, Trade>> & input_vec) {
                std::vector<std::pair<std::chrono::milliseconds, double>> buy, sell;
                for (const auto & [ts, trade] : input_vec) {
                    if (!ts_in_range(ts)) {
                        continue;
                    }

                    switch (trade.side().value()) {
                    case SideEnum::Buy: {
                        buy.emplace_back(ts, trade.price());
                        break;
                    }
                    case SideEnum::Sell: {
                        sell.emplace_back(ts, trade.price());
                        break;
                    }
                    }
                }
                auto & plot = get_or_create_chart(m_price_chart_name);
                plot.push_scatter_series_vector("buy_trade", buy);
                plot.push_scatter_series_vector("sell_trade", sell); },
            [&](std::chrono::milliseconds ts, const Trade & trade) {
                if (!ts_in_range(ts)) {
                    return;
                }
                get_or_create_chart(m_price_chart_name).push_trade(trade); });

    if (!m_render_depo) {
        return;
    }

    m_sub.subscribe(
            str_instance.depo_channel(),
            [this](const auto & vec) {
                std::list<std::pair<std::chrono::milliseconds, double>> res;
                for (const auto & [ts, value] : vec) {
                    if (!ts_in_range(ts)) {
                        continue;
                    }
                    res.emplace_back(ts, value);
                }
                auto & plot = get_or_create_chart(m_depo_chart_name);
                plot.push_series_vector("depo", res);
            },
            [&](std::chrono::milliseconds ts, double depo) {
                if (!ts_in_range(ts)) {
                    return;
                }
                get_or_create_chart(m_depo_chart_name).push_series_value("depo", ts, depo);
            });

    const auto update_trend_callback = [this](const StrategyResult & str_res) {
        auto & plot = get_or_create_chart(m_depo_chart_name);
        OLS::PriceRegressionFunction depo_trend{str_res.depo_trend_coef, str_res.depo_trend_const};
        plot.override_depo_trend(
                {str_res.first_position_closed_ts, depo_trend(str_res.first_position_closed_ts)},
                {str_res.last_position_closed_ts, depo_trend(str_res.last_position_closed_ts)});
    };
    const auto res = str_instance.strategy_result_channel().get();
    update_trend_callback(res);
    m_sub.subscribe(
            str_instance.strategy_result_channel(),
            update_trend_callback);
}

void ChartWindowEventConsumer::push(LambdaEvent value)
{
    m_cw.signal_lambda(std::move(value.func));
}

void ChartWindow::on_lambda(const std::function<void()> & lambda)
{
    lambda();
}
