#pragma once

#include "Candle.h"
#include "ConditionalOrders.h"
#include "MarketState.h"
#include "Trade.h"
#include "qcustomplot.h"

class MultiSeriesChart : public QCustomPlot
{
public:
    MultiSeriesChart(QWidget * parent = nullptr);
    ~MultiSeriesChart() override = default;

    // none of the push methods replots or rescales
    void push_candle(const Candle & c);
    void push_line_point(const std::string & series_name, std::chrono::milliseconds ts, double value);
    void push_marker_point(const std::string & series_name, std::chrono::milliseconds ts, double value);
    void push_trade(const Trade & trade);
    void push_tpsl(std::chrono::milliseconds ts, TpslPrices tpsl);
    void push_stop_loss(std::chrono::milliseconds ts, double stop_price);
    void push_market_state(std::chrono::milliseconds ts, MarketStateRenderObject state);

    void clear_all_points();
    void drop_points_outside(std::chrono::milliseconds start, std::chrono::milliseconds end);
    void extend_step_lines_to(std::chrono::milliseconds ts);

    void set_time_range(std::chrono::milliseconds start, std::chrono::milliseconds end);
    void rescale_value_axes();

    void set_title(const std::string & title); // TODO implement

    void override_depo_trend(std::pair<std::chrono::milliseconds, double> first, std::pair<std::chrono::milliseconds, double> last);

private:
    struct ChannelGraphs
    {
        QCPGraph * upper;
        QCPGraph * lower;
    };

    void create_candle_graphs(std::chrono::milliseconds timeframe);
    QCPGraph * get_graph_for_series(std::string_view series_name, QCPGraph::LineStyle line_style);
    ChannelGraphs get_channel_graphs(MarketState state);

private:
    static constexpr double value_margin_factor = 0.05;
    const std::chrono::milliseconds limit_interval = std::chrono::minutes{1};
    std::unordered_map<std::string, std::chrono::milliseconds> m_last_points; // to limit the rate

    QCPFinancial * m_candle_graph = nullptr;
    QCPAxisRect * m_volumeAxisRect = nullptr;
    QCPBars * m_volume_long_bars = nullptr;
    QCPBars * m_volume_short_bars = nullptr;
    QCPMarginGroup * m_marginGroup = nullptr;
    QCPGraph * m_depo_trend_graph = nullptr;

    std::map<std::string, QCPGraph *> m_series_graphs;
    std::map<QCPGraph *, double> m_step_line_extension_keys;

    MarketState m_prev_market_state = MarketState::None;
};
