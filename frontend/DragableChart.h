#pragma once

#include "Signal.h"
#include "Tpsl.h"

#include <QtCharts>
#include <chrono>
#include <qtmetamacros.h>

class DragableChart : public QChartView
{
    Q_OBJECT

public:
    DragableChart(QWidget * parent = nullptr);

    void clear();

    void push_series_value(const std::string & series_name,
                           std::chrono::milliseconds ts,
                           double data);
    void push_signal(Signal signal);
    void push_tpsl(std::chrono::milliseconds ts, Tpsl tpsl);

    void set_title(const std::string & title);

private slots:
    void update_axes();

protected:
    void mousePressEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;

private:
    void update_axes_values(std::chrono::milliseconds x, double y);
    void setup_series(QLineSeries & series);

private:
    QPointF m_lastMousePos;

    int64_t x_min = std::numeric_limits<int64_t>::max();
    int64_t x_max = std::numeric_limits<int64_t>::min();

    double y_min = std::numeric_limits<double>::max();
    double y_max = std::numeric_limits<double>::min();

    QTimer m_axis_update_timer;
    QDateTimeAxis * axisX = nullptr;
    QValueAxis * price_axis = nullptr;
    QLineSeries * prices = nullptr;
    QScatterSeries * buy_signals = nullptr;
    QScatterSeries * sell_signals = nullptr;
    QScatterSeries * close_signals = nullptr;

    QScatterSeries * take_profits = nullptr;
    QScatterSeries * stop_losses = nullptr;

    std::map<std::string, QLineSeries *> m_internal_series;
};
