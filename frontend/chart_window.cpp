#include "chart_window.h"

#include "ConditionalOrders.h"
#include "Enums.h"
#include "Logger.h"
#include "OrdinaryLeastSquares.h"
#include "ui_chart_window.h"

#include <QSignalBlocker>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>

namespace {

std::chrono::milliseconds seconds_to_ms(double seconds)
{
    return std::chrono::milliseconds{std::llround(seconds * 1000.)};
}

std::chrono::milliseconds now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
}

std::optional<std::chrono::milliseconds> parse_window_size(const std::string & text)
{
    std::istringstream stream(text);
    std::string token;
    std::chrono::milliseconds total{0};
    bool any_token = false;
    while (stream >> token) {

        if (token.size() < 2) {
            return std::nullopt;
        }

        int64_t value = 0;

        const auto * number_end = token.data() + token.size() - 1;
        const auto [ptr, ec] = std::from_chars(token.data(), number_end, value);

        if (ec != std::errc{} || ptr != number_end || value < 0) {
            return std::nullopt;
        }

        switch (token.back()) {
        case 'h': total += std::chrono::hours{value}; break;
        case 'd': total += std::chrono::hours{value * 24}; break;
        case 'w': total += std::chrono::hours{value * 24 * 7}; break;
        default: return std::nullopt;
        }

        any_token = true;
    }

    if (!any_token) {
        return std::nullopt;
    }

    return total;
}

std::string format_window_size(std::chrono::milliseconds size)
{
    const auto total_hours = std::chrono::duration_cast<std::chrono::hours>(size).count();
    const auto weeks = total_hours / (24 * 7);
    const auto days = (total_hours % (24 * 7)) / 24;
    const auto hours = total_hours % 24;

    std::string result;
    const auto append = [&](int64_t value, char unit) {
        if (value == 0) {
            return;
        }
        if (!result.empty()) {
            result += ' ';
        }
        result += std::to_string(value) + unit;
    };
    append(weeks, 'w');
    append(days, 'd');
    append(hours, 'h');
    return result;
}

} // namespace

ChartWindow::ChartWindow(
        std::string window_name,
        const std::weak_ptr<StrategyInstance> & strategy,
        std::optional<std::chrono::milliseconds> right_border,
        bool live,
        bool render_depo,
        QWidget * parent)
    : QWidget(parent)
    , m_render_depo(render_depo)
    , ui(new Ui::ChartWindow)
    , m_holder_widget(new QWidget(ui->scrollArea))
    , m_layout(new QVBoxLayout(m_holder_widget))
    , m_event_consumer(*this)
    , m_strategy_instance(strategy)
    , m_sub(m_event_consumer)
{
    ui->setupUi(this);

    ui->scrollArea->setWidget(m_holder_widget);
    ui->scrollArea->setWidgetResizable(true);
    this->setWindowTitle(window_name.c_str());
    connect(this, &ChartWindow::signal_lambda, this, &ChartWindow::on_lambda);

    m_zoom_timer.setSingleShot(true);
    m_zoom_timer.setInterval(zoom_load_delay);

    connect(&m_zoom_timer, &QTimer::timeout, this, &ChartWindow::load_window);
    connect(ui->sb_time, &QScrollBar::valueChanged, this, &ChartWindow::handle_scrollbar_value_changed);
    connect(ui->sb_time, &QScrollBar::sliderReleased, this, &ChartWindow::load_window);
    connect(ui->le_window_size, &QLineEdit::editingFinished, this, &ChartWindow::handle_window_size_edited);
    connect(ui->dt_right_border, &QDateTimeEdit::editingFinished, this, &ChartWindow::handle_right_border_edited);
    connect(ui->cb_live, &QCheckBox::toggled, this, &ChartWindow::handle_live_toggled);

    get_or_create_chart(m_price_chart_name);
    set_live(live);
    const auto initial_right_border = right_border.has_value() ? right_border : get_newest_timestamp();
    set_window(initial_right_border.value_or(now_ms()), initial_window_size);
    subscribe_to_strategy();
    load_window();
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

    if (m_price_chart_name != chart_name) {
        new_chart->legend->setVisible(true);
    }

    new_chart->set_time_range(window_left(), m_window_right);
    connect(new_chart->xAxis,
            qOverload<const QCPRange &>(&QCPAxis::rangeChanged),
            this,
            [this, new_chart](const QCPRange & range) { handle_plot_range_changed(*new_chart, range); });
    connect(new_chart,
            &QCustomPlot::mouseRelease,
            this,
            [this, new_chart](QMouseEvent *) { handle_plot_mouse_released(*new_chart); });
    connect(new_chart,
            &QCustomPlot::mouseWheel,
            this,
            [this](QWheelEvent * event) { handle_plot_wheel(event); });

    m_charts[chart_name] = new_chart;
    m_layout->addWidget(new_chart);
    return *new_chart;
}

std::chrono::milliseconds ChartWindow::window_left() const
{
    return m_window_right - m_window_size;
}

std::chrono::milliseconds ChartWindow::load_start() const
{
    return window_left() - std::chrono::milliseconds{std::llround(static_cast<double>(m_window_size.count()) * margin_factor)};
}

std::chrono::milliseconds ChartWindow::load_end() const
{
    return m_window_right + std::chrono::milliseconds{std::llround(static_cast<double>(m_window_size.count()) * margin_factor)};
}

// CLAUDE, "F" and "f" is a bad naming
template <class F>
void ChartWindow::for_each_channel(StrategyInstance & strategy_instance, F && f)
{
    f(strategy_instance.market_state_channel());
    f(strategy_instance.candle_channel());
    f(strategy_instance.price_levels_channel());
    f(strategy_instance.tpsl_channel());
    f(strategy_instance.trailing_stop_channel());

    for (const auto & name : strategy_instance.chart_series_list_channel().get().series_names) {
        UNWRAP_CONTINUE(own_series_channel, strategy_instance.find_own_series_channel(name));
        f(own_series_channel);
    }

    f(strategy_instance.trade_channel());

    if (m_render_depo) {
        f(strategy_instance.depo_channel());
    }
}

std::optional<std::pair<std::chrono::milliseconds, std::chrono::milliseconds>> ChartWindow::get_data_span()
{
    UNWRAP_RET_EMPTY(str_instance, m_strategy_instance.lock());

    std::optional<std::pair<std::chrono::milliseconds, std::chrono::milliseconds>> span;
    for_each_channel(str_instance, [&](auto & channel) {
        const auto first_and_last = channel.first_and_last_timestamps();
        if (!first_and_last.has_value()) {
            return;
        }
        if (!span.has_value()) {
            span = first_and_last;
            return;
        }
        span->first = std::min(span->first, first_and_last->first);
        span->second = std::max(span->second, first_and_last->second);
    });
    return span;
}

std::optional<std::chrono::milliseconds> ChartWindow::get_newest_timestamp()
{
    const auto span = get_data_span();
    if (!span.has_value()) {
        return std::nullopt;
    }
    return span->second;
}

void ChartWindow::extend_data_span(std::chrono::milliseconds ts)
{
    if (!m_data_span.has_value()) {
        m_data_span = std::make_pair(ts, ts);
        return;
    }
    m_data_span->first = std::min(m_data_span->first, ts);
    m_data_span->second = std::max(m_data_span->second, ts);
}

void ChartWindow::set_window(std::chrono::milliseconds right_border, std::chrono::milliseconds size)
{
    m_window_right = right_border;
    m_window_size = std::clamp(size, min_window_size, max_window_size);
    apply_window_to_plots();
    update_controls();
}

void ChartWindow::apply_window_to_plots()
{
    for (auto & [name, chart] : m_charts) {
        chart->set_time_range(window_left(), m_window_right);
        chart->replot(QCustomPlot::rpQueuedReplot);
    }
}

void ChartWindow::update_controls()
{
    const QSignalBlocker size_blocker(ui->le_window_size);
    const QSignalBlocker border_blocker(ui->dt_right_border);
    if (!ui->le_window_size->hasFocus()) {
        ui->le_window_size->setText(QString::fromStdString(format_window_size(m_window_size)));
    }
    ui->dt_right_border->setDateTime(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(m_window_right.count())));
    update_scrollbar();
}

void ChartWindow::update_scrollbar()
{
    const QSignalBlocker blocker(ui->sb_time);

    if (!m_data_span.has_value() || m_data_span->second - m_data_span->first <= m_window_size) {
        ui->sb_time->setRange(0, 0);
        ui->sb_time->setEnabled(false);
        return;
    }

    const auto to_sec = [](std::chrono::milliseconds ms) {
        return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(ms).count());
    };
    const int span_sec = to_sec(m_data_span->second - m_data_span->first);
    const int window_sec = to_sec(m_window_size);
    const int left_sec = to_sec(window_left() - m_data_span->first);

    ui->sb_time->setEnabled(true);
    ui->sb_time->setRange(0, span_sec - window_sec);
    ui->sb_time->setPageStep(window_sec);
    ui->sb_time->setSingleStep(std::max(1, window_sec / 10));
    ui->sb_time->setValue(std::clamp(left_sec, 0, span_sec - window_sec));
}

void ChartWindow::set_live(bool live)
{
    m_live = live;
    const QSignalBlocker blocker(ui->cb_live);
    ui->cb_live->setChecked(live);
    ui->dt_right_border->setEnabled(!live);
}

void ChartWindow::load_window()
{
    UNWRAP_RET_VOID(str_instance, m_strategy_instance.lock());

    const auto start = load_start();
    const auto end = load_end();

    for (auto & [name, chart] : m_charts) {
        chart->clear_all_points();
    }

    auto & price_plot = get_or_create_chart(m_price_chart_name);
    for (const auto & [ts, state] : str_instance.market_state_channel().data_copy_in_range(start, end, true)) {
        price_plot.push_market_state(ts, state);
    }
    for (const auto & [ts, candle] : str_instance.candle_channel().data_copy_in_range(start, end)) {
        price_plot.push_candle(candle);
    }
    for (const auto & [ts, levels] : str_instance.price_levels_channel().data_copy_in_range(start, end)) {
        price_plot.push_marker_point("fee_profit_price", ts, levels.fee_profit_price);
        price_plot.push_marker_point("no_loss_price", ts, levels.no_loss_price);
        // fee_loss_price is not rendered, there will be a trade marker here
    }
    for (const auto & [ts, tpsl] : str_instance.tpsl_channel().data_copy_in_range(start, end)) {
        price_plot.push_tpsl(ts, tpsl);
    }
    for (const auto & [ts, stop_loss] : str_instance.trailing_stop_channel().data_copy_in_range(start, end)) {
        price_plot.push_stop_loss(ts, stop_loss.stop_price());
    }
    for (const auto & name : str_instance.chart_series_list_channel().get().series_names) {
        UNWRAP_CONTINUE(own_series_channel, str_instance.find_own_series_channel(name));
        auto & own_series_plot = get_or_create_chart(name.chart_name);
        for (const auto & [ts, value] : own_series_channel.data_copy_in_range(start, end, true)) {
            own_series_plot.push_line_point(name.series_name, ts, value);
        }
    }
    for (const auto & [ts, trade] : str_instance.trade_channel().data_copy_in_range(start, end)) {
        price_plot.push_trade(trade);
    }
    if (m_render_depo) {
        auto & depo_plot = get_or_create_chart(m_depo_chart_name);
        for (const auto & [ts, depo] : str_instance.depo_channel().data_copy_in_range(start, end, true)) {
            depo_plot.push_line_point("depo", ts, depo);
        }
        update_depo_trend();
    }

    for (auto & [name, chart] : m_charts) {
        chart->extend_step_lines_to(end);
        chart->rescale_value_axes();
        chart->replot(QCustomPlot::rpQueuedReplot);
    }

    m_data_span = get_data_span();
    update_controls();
}

void ChartWindow::update_depo_trend()
{
    UNWRAP_RET_VOID(str_instance, m_strategy_instance.lock());

    const auto str_res = str_instance.strategy_result_channel().get();
    auto & plot = get_or_create_chart(m_depo_chart_name);
    OLS::PriceRegressionFunction depo_trend{str_res.depo_trend_coef, str_res.depo_trend_const};
    plot.override_depo_trend(
            {str_res.first_position_closed_ts, depo_trend(str_res.first_position_closed_ts)},
            {str_res.last_position_closed_ts, depo_trend(str_res.last_position_closed_ts)});
}

template <class PushF>
void ChartWindow::handle_new_point(const std::string & chart_name, std::chrono::milliseconds ts, PushF && push)
{
    const bool had_data = m_data_span.has_value();
    extend_data_span(ts);

    if (!m_live) {
        update_scrollbar();
        return;
    }

    auto & plot = get_or_create_chart(chart_name);
    push(plot);

    if (ts > m_window_right || !had_data) {
        m_window_right = ts;
        apply_window_to_plots();
        for (auto & [name, chart] : m_charts) {
            chart->drop_points_outside(load_start(), load_end());
            chart->extend_step_lines_to(load_end());
            chart->rescale_value_axes();
            chart->replot(QCustomPlot::rpQueuedReplot);
        }
    }
    else {
        plot.extend_step_lines_to(load_end());
        plot.rescale_value_axes();
        plot.replot(QCustomPlot::rpQueuedReplot);
    }
    update_controls();
}

void ChartWindow::subscribe_to_strategy()
{
    UNWRAP_RET_VOID(str_instance, m_strategy_instance.lock());

    m_sub.subscribe(
            str_instance.market_state_channel(),
            {},
            [this](std::chrono::milliseconds ts, const MarketStateRenderObject & state) {
                handle_new_point(m_price_chart_name, ts, [&](MultiSeriesChart & plot) {
                    plot.push_market_state(ts, state);
                });
            });

    m_sub.subscribe(
            str_instance.candle_channel(),
            {},
            [this](std::chrono::milliseconds ts, const Candle & candle) {
                handle_new_point(m_price_chart_name, ts, [&](MultiSeriesChart & plot) {
                    plot.push_candle(candle);
                });
            });

    m_sub.subscribe(
            str_instance.price_levels_channel(),
            {},
            [this](std::chrono::milliseconds ts, const ProfitPriceLevels & levels) {
                handle_new_point(m_price_chart_name, ts, [&](MultiSeriesChart & plot) {
                    plot.push_marker_point("fee_profit_price", ts, levels.fee_profit_price);
                    plot.push_marker_point("no_loss_price", ts, levels.no_loss_price);
                });
            });

    m_sub.subscribe(
            str_instance.tpsl_channel(),
            {},
            [this](std::chrono::milliseconds ts, const TpslPrices & tpsl) {
                handle_new_point(m_price_chart_name, ts, [&](MultiSeriesChart & plot) {
                    plot.push_tpsl(ts, tpsl);
                });
            });

    m_sub.subscribe(
            str_instance.trailing_stop_channel(),
            {},
            [this](std::chrono::milliseconds ts, const StopLoss & stop_loss) {
                handle_new_point(m_price_chart_name, ts, [&](MultiSeriesChart & plot) {
                    plot.push_stop_loss(ts, stop_loss.stop_price());
                });
            });

    m_sub.subscribe(
            str_instance.chart_series_list_channel(),
            [this](const ChartSeriesList & list) {
                UNWRAP_RET_VOID(strategy_instance, m_strategy_instance.lock());
                for (const auto & name : list.series_names) {
                    subscribe_to_own_series(strategy_instance, name, true);
                }
            });
    for (const auto & name : str_instance.chart_series_list_channel().get().series_names) {
        subscribe_to_own_series(str_instance, name, false);
    }

    m_sub.subscribe(
            str_instance.trade_channel(),
            {},
            [this](std::chrono::milliseconds ts, const Trade & trade) {
                handle_new_point(m_price_chart_name, ts, [&](MultiSeriesChart & plot) {
                    plot.push_trade(trade);
                });
            });

    if (!m_render_depo) {
        return;
    }

    m_sub.subscribe(
            str_instance.depo_channel(),
            {},
            [this](std::chrono::milliseconds ts, const double & depo) {
                handle_new_point(m_depo_chart_name, ts, [&](MultiSeriesChart & plot) {
                    plot.push_line_point("depo", ts, depo);
                });
            });

    m_sub.subscribe(
            str_instance.strategy_result_channel(),
            [this](const StrategyResult &) {
                update_depo_trend();
                get_or_create_chart(m_depo_chart_name).replot(QCustomPlot::rpQueuedReplot);
            });
}

void ChartWindow::subscribe_to_own_series(StrategyInstance & strategy_instance, const ChartSeriesName & name, bool draw_existing_points)
{
    UNWRAP_RET_VOID(own_series_channel, strategy_instance.find_own_series_channel(name));
    if (!m_subscribed_own_series.insert(name).second) {
        return;
    }

    const auto draw_point = [this, name](std::chrono::milliseconds ts, const double & value) {
        handle_new_point(name.chart_name, ts, [&](MultiSeriesChart & plot) {
            plot.push_line_point(name.series_name, ts, value);
        });
    };

    EventTimeseriesChannel<double>::SnapshotCallbackT draw_existing_points_callback;
    if (draw_existing_points) {
        draw_existing_points_callback = [draw_point](const std::deque<std::pair<std::chrono::milliseconds, double>> & points) {
            for (const auto & [ts, value] : points) {
                draw_point(ts, value);
            }
        };
    }

    m_sub.subscribe(own_series_channel, std::move(draw_existing_points_callback), draw_point);
}

void ChartWindow::handle_plot_range_changed(MultiSeriesChart & source, const QCPRange & range)
{
    for (auto & [name, chart] : m_charts) {
        if (chart == &source) {
            continue;
        }
        chart->xAxis->setRange(range);
        chart->replot(QCustomPlot::rpQueuedReplot);
    }
}

void ChartWindow::handle_plot_mouse_released(MultiSeriesChart & source)
{
    const auto right_border = seconds_to_ms(source.xAxis->range().upper);
    if (right_border == m_window_right) {
        return;
    }
    set_live(false);
    set_window(right_border, m_window_size);
    load_window();
}

void ChartWindow::handle_plot_wheel(QWheelEvent * event)
{
    const double wheel_steps = event->angleDelta().y() / 120.;
    if (wheel_steps == 0.) {
        return;
    }

    const double factor = std::pow(zoom_step_factor, wheel_steps);
    const auto new_size = std::clamp(
            std::chrono::milliseconds{std::llround(static_cast<double>(m_window_size.count()) * factor)},
            min_window_size,
            max_window_size);
    if (new_size == m_window_size) {
        return;
    }

    set_window(m_window_right, new_size);
    m_zoom_timer.start();
}

void ChartWindow::handle_scrollbar_value_changed(int value)
{
    if (!m_data_span.has_value()) {
        return;
    }
    set_live(false);
    const auto left = m_data_span->first + std::chrono::seconds{value};
    set_window(left + m_window_size, m_window_size);
    if (!ui->sb_time->isSliderDown()) {
        load_window();
    }
}

void ChartWindow::handle_window_size_edited()
{
    const auto parsed = parse_window_size(ui->le_window_size->text().toStdString());
    if (!parsed.has_value()) {
        update_controls();
        return;
    }
    const auto new_size = std::clamp(*parsed, min_window_size, max_window_size);
    if (new_size == m_window_size) {
        update_controls();
        return;
    }
    set_window(m_window_right, new_size);
    load_window();
}

void ChartWindow::handle_right_border_edited()
{
    const auto right_border = std::chrono::milliseconds{ui->dt_right_border->dateTime().toMSecsSinceEpoch()};
    if (right_border == m_window_right) {
        return;
    }
    set_window(right_border, m_window_size);
    load_window();
}

void ChartWindow::handle_live_toggled(bool live)
{
    set_live(live);
    if (!live) {
        return;
    }
    const auto newest = get_newest_timestamp();
    if (newest.has_value()) {
        set_window(*newest, m_window_size);
    }
    load_window();
}

void ChartWindowEventConsumer::push(LambdaEvent value)
{
    m_cw.signal_lambda(std::move(value.func));
}

void ChartWindow::on_lambda(const std::function<void()> & lambda)
{
    lambda();
}
