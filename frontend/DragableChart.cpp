#include "DragableChart.h"

#include <iostream>
#include <qcoreevent.h>
#include <qnamespace.h>

DragableChart::DragableChart(QWidget * parent)
    : QChartView(new QChart(), parent)
    , axisX(new QDateTimeAxis)
    , price_axis(new QValueAxis)
    , prices(new QLineSeries())
    , buy_signals(new QScatterSeries())
    , sell_signals(new QScatterSeries())
    , close_signals(new QScatterSeries())
{
    setDragMode(QGraphicsView::NoDrag);
    this->setMouseTracking(true);

    setRenderHint(QPainter::Antialiasing);

    buy_signals->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    buy_signals->setMarkerSize(15.0);
    sell_signals->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
    sell_signals->setMarkerSize(11.0);
    close_signals->setMarkerShape(QScatterSeries::MarkerShapeStar);
    close_signals->setMarkerSize(20.0);

    axisX->setTickCount(10);
    axisX->setFormat("hh:mm:ss");
    axisX->setTitleText("Time");
    price_axis->setTickCount(10);
    price_axis->setTitleText("Values");

    chart()->legend()->hide();
    chart()->addAxis(axisX, Qt::AlignBottom);
    chart()->addAxis(price_axis, Qt::AlignLeft);

    setup_series(*prices);

    chart()->addSeries(buy_signals);
    chart()->addSeries(sell_signals);
    chart()->addSeries(close_signals);

    buy_signals->attachAxis(axisX);
    buy_signals->attachAxis(price_axis);
    sell_signals->attachAxis(axisX);
    sell_signals->attachAxis(price_axis);
    close_signals->attachAxis(axisX);
    close_signals->attachAxis(price_axis);

    prices->setColor(QColor(0, 0, 0));

    chart()->setTitle("Empty title");

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

void DragableChart::push_signal(Signal signal)
{
    switch (signal.side) {
    case Side::Buy: {
        buy_signals->append(static_cast<double>(signal.timestamp.count()), signal.price);
        break;
    }
    case Side::Sell: {
        sell_signals->append(static_cast<double>(signal.timestamp.count()), signal.price);
        break;
    }
    case Side::Close: {
        close_signals->append(static_cast<double>(signal.timestamp.count()), signal.price);
        break;
    }
    }
}

void DragableChart::push_series_value(
        const std::string & series_name,
        std::chrono::milliseconds ts,
        double data)
{
    QLineSeries * series = nullptr;
    auto it = m_internal_series.find(series_name);
    if (it != m_internal_series.end()) {
        series = it->second;
    }
    else {
        series = new QLineSeries();
        setup_series(*series);
        m_internal_series[series_name] = series;
    }

    series->append(static_cast<double>(ts.count()), data);
    update_axes_values(ts, data);
    m_axis_update_timer.start(500);
}

void DragableChart::clear()
{
    x_min = std::numeric_limits<int64_t>::max();
    x_max = std::numeric_limits<int64_t>::min();
    y_min = std::numeric_limits<double>::max();
    y_max = std::numeric_limits<double>::min();

    axisX->setRange(QDateTime::fromMSecsSinceEpoch(x_min), QDateTime::fromMSecsSinceEpoch(x_max));
    prices->clear();
    buy_signals->clear();
    sell_signals->clear();
    close_signals->clear();
    for (auto & [_, series_ptr] : m_internal_series) {
        delete series_ptr;
    }
    m_internal_series.clear();
}

void DragableChart::set_title(const std::string & title)
{
    chart()->setTitle(title.c_str());
}
