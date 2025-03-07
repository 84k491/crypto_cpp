#include "PositionResultView.h"

#include "DateTimeConverter.h"
#include "ui_PositionResultView.h"

PositionResultView::PositionResultView(QWidget * parent)
    : QWidget(parent)
    , ui(new Ui::PositionResultView)
{
    ui->setupUi(this);
}

void PositionResultView::update(PositionResult position_result)
{
    ui->lb_guid->setText(QString::fromStdString(position_result.guid));
    ui->lb_pnl->setText(QString::number(position_result.pnl_with_fee));
    if (position_result.pnl_with_fee > 0.) {
        ui->lb_pnl->setStyleSheet("color: green");
    }
    else {
        ui->lb_pnl->setStyleSheet("color: red");
    }
    const auto opened_time_str = std::to_string(static_cast<double>(position_result.opened_time().count()) / 60000);
    ui->lb_open_time->setText(DateTimeConverter::date_time(position_result.open_ts).c_str());
    ui->lb_opened_time_m->setText(opened_time_str.c_str());
}

PositionResultView::~PositionResultView()
{
    delete ui;
}
