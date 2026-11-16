#ifndef CHART_WINDOW_H
#define CHART_WINDOW_H

#include "Events.h"
#include "MultiSeriesChart.h"
#include "StrategyInstance.h"

#include <QTimer>
#include <QWidget>
#include <optional>

namespace Ui {
class ChartWindow;
}

class ChartWindow;
class ChartWindowEventConsumer : public ILambdaAcceptor
{
public:
    ChartWindowEventConsumer(ChartWindow & cw)
        : m_cw(cw)
    {
    }

private:
    // IEventConsumer<LambdaEvent>
    void push(LambdaEvent value) override;
    void discard_subscriber_events(xg::Guid) override {}

private:
    ChartWindow & m_cw;
};

class ChartWindow : public QWidget
{
    Q_OBJECT

public:
    ChartWindow(
            std::string window_name,
            const std::weak_ptr<StrategyInstance> & strategy,
            std::optional<std::chrono::milliseconds> right_border,
            bool live,
            bool render_depo,
            QWidget * parent = nullptr);
    ~ChartWindow() override;

    MultiSeriesChart & get_or_create_chart(const std::string & chart_name);

    void resizeEvent(QResizeEvent * event) override;

signals:
    void signal_lambda(std::function<void()> lambda);

private slots:
    void on_lambda(const std::function<void()> & lambda);

private:
    void subscribe_to_strategy();
    void subscribe_to_own_series(StrategyInstance & strategy_instance, const ChartSeriesName & name, bool draw_existing_points);
    void load_window();
    void update_depo_trend();

    template <class PushF>
    void handle_new_point(const std::string & chart_name, std::chrono::milliseconds ts, PushF && push);

    void set_window(std::chrono::milliseconds right_border, std::chrono::milliseconds size);
    void apply_window_to_plots();
    void update_controls();
    void update_scrollbar();
    void set_live(bool live);
    void extend_data_span(std::chrono::milliseconds ts);

    std::chrono::milliseconds window_left() const;
    std::chrono::milliseconds load_start() const;
    std::chrono::milliseconds load_end() const;

    template <class F>
    void for_each_channel(StrategyInstance & strategy_instance, F && f);
    std::optional<std::pair<std::chrono::milliseconds, std::chrono::milliseconds>> get_data_span();
    std::optional<std::chrono::milliseconds> get_newest_timestamp();

    void handle_plot_range_changed(MultiSeriesChart & source, const QCPRange & range);
    void handle_plot_mouse_released(MultiSeriesChart & source);
    void handle_plot_wheel(QWheelEvent * event);
    void handle_scrollbar_value_changed(int value);
    void handle_window_size_edited();
    void handle_right_border_edited();
    void handle_live_toggled(bool live);

private:
    static constexpr double height_factor = 0.7;
    static constexpr double margin_factor = 0.1;
    static constexpr double zoom_step_factor = 0.85;
    static constexpr std::chrono::milliseconds initial_window_size = std::chrono::hours{24};
    static constexpr std::chrono::milliseconds min_window_size = std::chrono::hours{1};
    static constexpr std::chrono::milliseconds max_window_size = std::chrono::hours{24 * 7};
    static constexpr std::chrono::milliseconds zoom_load_delay{150};

    std::chrono::milliseconds m_window_right = {};
    std::chrono::milliseconds m_window_size = initial_window_size;
    bool m_live = false;
    std::optional<std::pair<std::chrono::milliseconds, std::chrono::milliseconds>> m_data_span;
    bool m_render_depo = false;

    Ui::ChartWindow * ui;
    QWidget * m_holder_widget = nullptr;
    QVBoxLayout * m_layout = nullptr;
    QTimer m_zoom_timer;
    ChartWindowEventConsumer m_event_consumer;

    const std::string m_price_chart_name = "prices";
    const std::string m_depo_chart_name = "depo";
    std::map<std::string, MultiSeriesChart *> m_charts;

    std::weak_ptr<StrategyInstance> m_strategy_instance;
    std::set<ChartSeriesName> m_subscribed_own_series;
    EventSubcriber m_sub;
};

#endif // CHART_WINDOW_H
