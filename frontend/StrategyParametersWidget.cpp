#include "StrategyParametersWidget.h"

#include <print>
#include <qboxlayout.h>
#include <qlabel.h>

StrategyParametersWidget::StrategyParametersWidget(QWidget * parent)
    : QGroupBox(parent)
{
    setTitle("Entry parameters");
}

void StrategyParametersWidget::setup_widget(const JsonStrategyMetaInfo & strategy_parameters)
{
    qDeleteAll(children());
    m_strategy_parameters_spinboxes.clear();

    if (!strategy_parameters.got_parameters()) {
        return;
    }
    const auto params = strategy_parameters.get()["parameters"].get<std::vector<nlohmann::json>>();
    auto * top_layout = new QVBoxLayout();

    auto * name_layout = new QHBoxLayout();
    auto * strategy_name_label = new QLabel(strategy_parameters.get()["strategy_name"].get<std::string>().c_str());
    name_layout->addWidget(strategy_name_label);
    top_layout->addItem(name_layout);
    for (const auto & param : params) {
        const auto name = param["name"].get<std::string>();
        const auto max_value = param["max_value"].get<double>();
        const auto step = param["step"].get<double>();
        const auto min_value = param["min_value"].get<double>();

        auto * layout = new QHBoxLayout();
        auto * label = new QLabel(name.c_str());
        auto * double_spin_box = new QDoubleSpinBox();
        double_spin_box->setMinimum(min_value);
        double_spin_box->setSingleStep(step);
        double_spin_box->setMaximum(max_value);

        double_spin_box->setValue(min_value);
        m_strategy_parameters_spinboxes[name] = double_spin_box;

        label->setText(name.c_str());
        layout->addWidget(label);
        layout->addWidget(double_spin_box);
        top_layout->addItem(layout);
    }
    setLayout(top_layout);
}

void StrategyParametersWidget::setup_values(const JsonStrategyConfig & config)
{
    for (auto it = config.get().begin(); it != config.get().end(); ++it) {
        auto spinbox_it = m_strategy_parameters_spinboxes.find(it.key());
        if (spinbox_it == m_strategy_parameters_spinboxes.end()) {
            std::println("ERROR: Could not find spinbox for {}", it.key());
            continue;
        }
        m_strategy_parameters_spinboxes.at(it.key())->setValue(it.value().get<double>());
    }
}

JsonStrategyConfig StrategyParametersWidget::get_config()
{
    nlohmann::json config;
    for (const auto & [name, spinbox_ptr] : m_strategy_parameters_spinboxes) {
        config[name] = spinbox_ptr->value();
    }
    return config;
}
