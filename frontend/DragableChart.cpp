#include "DragableChart.h"

#include <qcoreevent.h>
#include <qnamespace.h>

DragableChart::DragableChart(QChart * chart, QWidget * parent)
    : QChartView(chart, parent)
{
    setDragMode(QGraphicsView::NoDrag);
    this->setMouseTracking(true);
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
    chart()->zoom(factor);
    event->accept();
    QChartView::wheelEvent(event);
}
