#pragma once

#include "JsonStrategyConfig.h"

#include <QWidget>
#include <qcheckbox.h>
#include <qgroupbox.h>
#include <set>

class OptimizerParametersWidget : public QGroupBox
{
    Q_OBJECT

public:
    OptimizerParametersWidget(QWidget * parent = nullptr);

    void setup_widget(const JsonStrategyMetaInfo & entry_parameters, const JsonStrategyMetaInfo & exit_parameters);
    std::set<std::string> optimizable_parameters();

private:
    std::map<std::string, QCheckBox *> m_checkboxes;
};
