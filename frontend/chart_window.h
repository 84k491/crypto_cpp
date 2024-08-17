#ifndef CHART_WINDOW_H
#define CHART_WINDOW_H

#include "Events.h"
#include "MultiSeriesChart.h"
#include "StrategyInstance.h"

#include <QWidget>

namespace Ui {
class ChartWindow;
}

class ChartWindow
    : public QWidget
    , public IEventConsumer<LambdaEvent>
{
    Q_OBJECT

public:
    ChartWindow(const std::shared_ptr<StrategyInstance> & strategy, QWidget * parent = nullptr);
    ~ChartWindow() override;

    MultiSeriesChart & get_or_create_chart(const std::string & chart_name);

signals:
    void signal_lambda(std::function<void()> lambda);

private slots:
    void on_lambda(const std::function<void()> & lambda);

private:
    // IEventConsumer<LambdaEvent>
    bool push_to_queue(std::any value) override;
    bool push_to_queue_delayed(std::chrono::milliseconds delay, const std::any value) override;

private:
    void subscribe_to_strategy();

private:
    Ui::ChartWindow * ui;

    const std::string m_price_chart_name = "prices";
    const std::string m_depo_chart_name = "depo";
    std::map<std::string, MultiSeriesChart *> m_charts;

    std::weak_ptr<StrategyInstance> m_strategy_instance;
    std::list<std::shared_ptr<ISubsription>> m_subscriptions;
};

#endif // CHART_WINDOW_H
