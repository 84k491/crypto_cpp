#ifndef POSITIONRESULTVIEW_H
#define POSITIONRESULTVIEW_H

#include "PositionManager.h"

#include <QWidget>

namespace Ui {
class PositionResultView;
}

class PositionResultView : public QWidget
{
    Q_OBJECT

public:
    explicit PositionResultView(QWidget * parent = nullptr);
    void update(PositionResult position_result);
    ~PositionResultView() override;

private:
    Ui::PositionResultView * ui;
};

#endif // POSITIONRESULTVIEW_H
