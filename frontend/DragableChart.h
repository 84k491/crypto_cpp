#pragma once

#include "Signal.h"

#include <QtCharts>
#include <chrono>
#include <qtmetamacros.h>

class DragableChart : public QChartView
{
    Q_OBJECT

public:
    DragableChart(QWidget * parent = nullptr);

    void clear();

public slots:
    void on_push_price(std::chrono::milliseconds ts, double price);
    void on_push_signal(Signal signal);
    void on_push_strategy_internal(const std::string & name,
                                   std::chrono::milliseconds ts,
                                   double data);
    void on_push_depo(std::chrono::milliseconds ts, double depo);

private slots:
    void update_axes();

protected:
    void mousePressEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;

private:
    void update_axes_values(std::chrono::milliseconds x, double y);
    void update_depo_axis(double y);

private:
    QPointF m_lastMousePos;

    int64_t x_min = std::numeric_limits<int64_t>::max();
    int64_t x_max = std::numeric_limits<int64_t>::min();

    double y_min = std::numeric_limits<double>::max();
    double y_max = std::numeric_limits<double>::min();

    double depo_min = std::numeric_limits<double>::max();
    double depo_max = std::numeric_limits<double>::min();

    QTimer m_axis_update_timer;
    QDateTimeAxis * axisX = nullptr;
    QValueAxis * price_axis = nullptr;
    QValueAxis * depo_axis = nullptr;
    QLineSeries * prices = nullptr;
    QScatterSeries * buy_signals = nullptr;
    QScatterSeries * sell_signals = nullptr;

    QLineSeries * slow_avg = nullptr;
    QLineSeries * fast_avg = nullptr;

    QLineSeries * upper_bb = nullptr;
    QLineSeries * trend_bb = nullptr;
    QLineSeries * lower_bb = nullptr;

    QLineSeries * depo = nullptr;
};
