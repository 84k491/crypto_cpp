#ifndef CHART_WINDOW_H
#define CHART_WINDOW_H

#include "Events.h"
#include "MultiSeriesChart.h"
#include "StrategyInstance.h"

#include <QWidget>

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
    void push_delayed(std::chrono::milliseconds delay, LambdaEvent value) override;

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
            std::chrono::milliseconds start_ts,
            std::chrono::milliseconds end_ts,
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
    bool ts_in_range(std::chrono::milliseconds ts) const;

private:
    static constexpr double height_factor = 0.7;

    std::chrono::milliseconds m_start_ts;
    std::chrono::milliseconds m_end_ts;
    bool m_render_depo = false;

    Ui::ChartWindow * ui;
    QWidget * m_holder_widget = nullptr;
    QVBoxLayout * m_layout = nullptr;
    ChartWindowEventConsumer m_event_consumer;

    const std::string m_price_chart_name = "prices";
    const std::string m_depo_chart_name = "depo";
    std::map<std::string, MultiSeriesChart *> m_charts;

    std::weak_ptr<StrategyInstance> m_strategy_instance;
    EventSubcriber m_sub;
};

#endif // CHART_WINDOW_H
