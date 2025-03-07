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
    m_position_result = position_result;

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

void PositionResultView::on_pb_chart_clicked()
{
    const auto si = m_strategy_instance.lock();
    if (!si) {
        return;
    }

    const auto opened_time = m_position_result.opened_time();
    const long delta = opened_time.count() * width_factor;
    const auto start_ts = std::chrono::milliseconds{m_position_result.open_ts.count() - opened_time.count()};
    const auto end_ts = std::chrono::milliseconds{m_position_result.close_ts.count() + delta};

    m_chart_window = new ChartWindow(
            m_position_result.guid,
            si,
            start_ts,
            end_ts,
            false,
            nullptr);
    m_chart_window->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_chart_window, &ChartWindow::destroyed, [this](auto) {
        m_chart_window = nullptr;
    });
    m_chart_window->show();
}
