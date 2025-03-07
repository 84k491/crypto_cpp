#ifndef POSITIONRESULTVIEW_H
#define POSITIONRESULTVIEW_H

#include "PositionManager.h"
#include "StrategyInstance.h"
#include "chart_window.h"

#include <QWidget>

namespace Ui {
class PositionResultView;
}

class PositionResultView : public QWidget
{
    Q_OBJECT

public:
    explicit PositionResultView(QWidget * parent = nullptr);
    ~PositionResultView() override;

    void update(PositionResult position_result); // TODO use it in c-tor()
    void set_strategy_instance(const std::weak_ptr<StrategyInstance> & strategy_instance) { m_strategy_instance = strategy_instance; }

private slots:
    void on_pb_chart_clicked();

private:
    static constexpr double width_factor = 0.5;

    Ui::PositionResultView * ui;
    PositionResult m_position_result;
    ChartWindow* m_chart_window = nullptr;

    std::weak_ptr<StrategyInstance> m_strategy_instance;
};

#endif // POSITIONRESULTVIEW_H
