#pragma once

#include <QtCharts>

class DragableChart : public QChartView
{
    Q_OBJECT

public:
    DragableChart(QChart * chart, QWidget * parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QPointF m_lastMousePos;
};
