#pragma once

#include "JsonStrategyConfig.h"

#include <QWidget>
#include <qgroupbox.h>
#include <qspinbox.h>

class StrategyParametersWidget : public QGroupBox
{
    Q_OBJECT

public:
    StrategyParametersWidget(QWidget * parent = nullptr);

    void setup_widget(const JsonStrategyMetaInfo & strategy_parameters);
    void setup_values(const JsonStrategyConfig & config);
    JsonStrategyConfig get_config();

private:
    std::map<std::string, QDoubleSpinBox *> m_strategy_parameters_spinboxes;
};
