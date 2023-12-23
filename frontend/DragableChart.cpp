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
    , depo(new QLineSeries())
    , depo_axis(new QValueAxis)
    , price_axis(new QValueAxis)
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
    axisX->setTitleText("Time");
    depo_axis->setTickCount(10);
    depo_axis->setTitleText("Depo");
    price_axis->setTickCount(10);
    price_axis->setTitleText("Price");

    chart()->legend()->hide();
    chart()->addAxis(axisX, Qt::AlignBottom);
    chart()->addAxis(depo_axis, Qt::AlignRight);
    chart()->addAxis(price_axis, Qt::AlignLeft);

    setup_series(*prices);

    chart()->addSeries(buy_signals);
    chart()->addSeries(sell_signals);
    chart()->addSeries(depo);

    buy_signals->attachAxis(axisX);
    buy_signals->attachAxis(price_axis);
    sell_signals->attachAxis(axisX);
    sell_signals->attachAxis(price_axis);
    depo->attachAxis(axisX);
    depo->attachAxis(depo_axis);

    depo->setColor(QColor(240, 170, 240));
    prices->setColor(QColor(0, 0, 0));

    chart()->setTitle("Simple line chart() example");

    connect(&m_axis_update_timer, &QTimer::timeout, this, &DragableChart::update_axes);
    m_axis_update_timer.setSingleShot(true);
}

void DragableChart::setup_series(QLineSeries & series)
{
    chart()->addSeries(&series);
    series.attachAxis(axisX);
    series.attachAxis(price_axis);
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

void DragableChart::update_axes_values(std::chrono::milliseconds x, double y)
{
    if (x.count() < x_min || x.count() > x_max) {
        if (x.count() < x_min) {
            x_min = x.count();
        }
        if (x.count() > x_max) {
            x_max = x.count();
        }
    }

    if (y < y_min || y > y_max) {
        if (y < y_min) {
            y_min = y;
        }
        if (y > y_max) {
            y_max = y;
        }
    }
}

void DragableChart::update_axes()
{
    axisX->setRange(QDateTime::fromMSecsSinceEpoch(x_min), QDateTime::fromMSecsSinceEpoch(x_max));
    price_axis->setRange(y_min, y_max);
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
    auto * new_series = new QLineSeries();
    auto [it, success] = m_internal_series.try_emplace(name, new_series);
    if (!success) {
        delete new_series;
    }
    else {
        setup_series(*new_series);
    }
    auto & series = *it->second;
    series.append(static_cast<double>(ts.count()), data);
}

void DragableChart::on_push_price(std::chrono::milliseconds ts, double price)
{
    prices->append(static_cast<double>(ts.count()), price);
    update_axes_values(ts, price);
    m_axis_update_timer.start(500);
}

void DragableChart::clear()
{
    x_min = std::numeric_limits<int64_t>::max();
    x_max = std::numeric_limits<int64_t>::min();
    y_min = std::numeric_limits<double>::max();
    y_max = std::numeric_limits<double>::min();
    depo_min = std::numeric_limits<double>::max();
    depo_max = std::numeric_limits<double>::min();

    axisX->setRange(QDateTime::fromMSecsSinceEpoch(x_min), QDateTime::fromMSecsSinceEpoch(x_max));
    prices->clear();
    buy_signals->clear();
    sell_signals->clear();
    for (auto & [_, series_ptr] : m_internal_series) {
        delete series_ptr;
    }
    m_internal_series.clear();
    depo->clear();
}

void DragableChart::on_push_depo(std::chrono::milliseconds ts, double value)
{
    depo->append(static_cast<double>(ts.count()), value);
    update_depo_axis(value);
}

void DragableChart::update_depo_axis(double depo)
{
    if (depo < depo_min || depo > depo_max) {
        if (depo < depo_min) {
            depo_min = depo;
        }
        if (depo > depo_max) {
            depo_max = depo;
        }
        depo_axis->setRange(depo_min, depo_max);
    }
}
