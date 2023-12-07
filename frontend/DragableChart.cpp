#include "DragableChart.h"

#include <iostream>
#include <qcoreevent.h>
#include <qnamespace.h>

DragableChart::DragableChart(QWidget * parent)
    : QChartView(new QChart(), parent)
    , axisX(new QDateTimeAxis)
    , prices(new QLineSeries())
    , buy_signals(new QScatterSeries())
    , sell_signals(new QScatterSeries())
    , slow_avg(new QLineSeries())
    , fast_avg(new QLineSeries())
{
    setDragMode(QGraphicsView::NoDrag);
    this->setMouseTracking(true);

    setRenderHint(QPainter::Antialiasing);

    buy_signals->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    buy_signals->setMarkerSize(15.0);
    sell_signals->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
    sell_signals->setMarkerSize(11.0);

    axisX->setTickCount(10);
    axisX->setFormat("hh:mm:ss");
    axisX->setTitleText("Date");

    chart()->legend()->hide();
    // add series to chart before attaching axis
    chart()->addSeries(prices);
    chart()->addSeries(slow_avg);
    chart()->addSeries(fast_avg);
    chart()->addSeries(buy_signals);
    chart()->addSeries(sell_signals);
    chart()->createDefaultAxes();
    chart()->removeAxis(chart()->axisX());
    chart()->addAxis(axisX, Qt::AlignBottom);
    prices->attachAxis(axisX);
    slow_avg->attachAxis(axisX);
    fast_avg->attachAxis(axisX);
    buy_signals->attachAxis(axisX);
    sell_signals->attachAxis(axisX);

    chart()->setTitle("Simple line chart() example");
}

void DragableChart::mousePressEvent(QMouseEvent * event)
{
    switch (event->button()) {
    case Qt::RightButton:
        chart()->zoomReset();
        break;
    case Qt::LeftButton:
        QApplication::setOverrideCursor(QCursor(Qt::ClosedHandCursor));
        m_lastMousePos = event->pos();
        event->accept();
        break;
    default: break;
    }

    QChartView::mousePressEvent(event);
}

void DragableChart::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        QApplication::restoreOverrideCursor();
        event->accept();
    }
    QChartView::mouseReleaseEvent(event);
}

void DragableChart::mouseMoveEvent(QMouseEvent * event)
{
    if ((event->buttons() & Qt::LeftButton) != 0) {
        auto dPos = event->pos() - m_lastMousePos;
        chart()->scroll(-dPos.x(), dPos.y());

        m_lastMousePos = event->pos();
        event->accept();
    }

    QChartView::mouseMoveEvent(event);
}

void DragableChart::wheelEvent(QWheelEvent * event)
{
    qreal factor = event->angleDelta().y() > 0 ? 1.5 : 0.75;
    this->chart()->zoom(factor);
    event->accept();
    QChartView::wheelEvent(event);
}

void DragableChart::update_axes(std::chrono::milliseconds x, double y)
{
    if (x.count() < x_min || x.count() > x_max) {
        if (x.count() < x_min) {
            x_min = x.count();
        }
        if (x.count() > x_max) {
            x_max = x.count();
        }
        axisX->setRange(QDateTime::fromMSecsSinceEpoch(x_min), QDateTime::fromMSecsSinceEpoch(x_max));
    }

    if (y < y_min || y > y_max) {
        if (y < y_min) {
            y_min = y;
        }
        if (y > y_max) {
            y_max = y;
        }
        chart()->axisY()->setRange(y_min, y_max);
    }
}

void DragableChart::on_push_signal(Signal signal)
{
    if (signal.side == Side::Buy) {
        buy_signals->append(static_cast<double>(signal.timestamp.count()), signal.price);
    }
    else {
        sell_signals->append(static_cast<double>(signal.timestamp.count()), signal.price);
    }
}

void DragableChart::on_push_strategy_internal(
        const std::string & name,
        std::chrono::milliseconds ts,
        double data)
{
    if (name == "slow_avg_history") {
        slow_avg->append(static_cast<double>(ts.count()), data);
        return;
    }
    if (name == "fast_avg_history") {
        fast_avg->append(static_cast<double>(ts.count()), data);
        return;
    }
    std::cout << "ERROR: Unknown internal data name: " << name << std::endl;
}

void DragableChart::on_push_price(std::chrono::milliseconds ts, double price)
{
    prices->append(static_cast<double>(ts.count()), price);
    update_axes(ts, price);
}
