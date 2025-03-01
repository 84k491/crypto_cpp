#include "OptimizerParamsWidget.h"

#include <qboxlayout.h>

OptimizerParametersWidget::OptimizerParametersWidget(QWidget *)
{
}

void OptimizerParametersWidget::setup_widget(const JsonStrategyMetaInfo & entry_parameters, const JsonStrategyMetaInfo & exit_parameters)
{
    qDeleteAll(children());
    m_checkboxes.clear();

    auto * top_layout = new QVBoxLayout();

    const auto fill_gb = [&](const std::string & gb_name, const auto & params) {
        if (!params.empty()) {
            auto * gb = new QGroupBox(gb_name.c_str());
            auto * lo = new QVBoxLayout();
            gb->setLayout(lo);

            for (const auto & param : params) {
                const auto name = param["name"].template get<std::string>();
                auto * checkbox = new QCheckBox(name.c_str());
                checkbox->setChecked(true);
                m_checkboxes[name] = checkbox;
                gb->layout()->addWidget(checkbox);
            }
            top_layout->addWidget(gb);
        }
    };

    const auto entry_params = entry_parameters.get()["parameters"].get<std::vector<nlohmann::json>>();
    const auto exit_params = exit_parameters.get()["parameters"].get<std::vector<nlohmann::json>>();
    fill_gb("Entry", entry_params);
    fill_gb("Exit", exit_params);

    setLayout(top_layout);
}

std::vector<std::string> OptimizerParametersWidget::optimizable_parameters()
{
    std::vector<std::string> output;
    for (const auto & [name, checkbox] : m_checkboxes) {
        if (checkbox->isChecked()) {
            output.push_back(name);
        }
    }
    return output;
}
