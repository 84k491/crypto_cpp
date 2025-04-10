#include "Optimizer.h"

#include "BacktestTradingGateway.h"
#include "JsonStrategyConfig.h"
#include "LogLevel.h"
#include "OrdinaryLeastSquares.h"
#include "ScopeExit.h"
#include "StrategyFactory.h"
#include "StrategyInstance.h"

#include <mutex>
#include <vector>

std::vector<DoubleJsonStrategyConfig> OptimizerParser::get_possible_configs()
{
    const auto entry_configs = get_possible_configs(m_inputs.entry_strategy);
    const auto exit_configs = get_possible_configs(m_inputs.exit_strategy);

    std::vector<DoubleJsonStrategyConfig> outputs;
    for (const auto & entry_config : entry_configs) {
        for (const auto & exit_config : exit_configs) {
            outputs.emplace_back(entry_config, exit_config);
        }
    }
    return outputs;
}

std::vector<JsonStrategyConfig> OptimizerParser::get_possible_configs(const StrategyOptimizerInputs & strategy_optimizer_inputs)
{
    std::vector<JsonStrategyConfig> ouput_jsons;

    std::map<std::string, std::tuple<double, double, double>> limits_map;
    std::map<std::string, double> current_values_map;
    const auto params = strategy_optimizer_inputs.meta.get()["parameters"].get<std::vector<nlohmann::json>>();
    for (const auto & param : params) {
        const auto name = param["name"].get<std::string>();
        if (strategy_optimizer_inputs.optimizable_parameters.contains(name)) {
            const auto max_value = param["max_value"].get<double>();
            const auto step = param["step"].get<double>();
            const auto min_value = param["min_value"].get<double>();
            limits_map[name] = {min_value, step, max_value};
            current_values_map[name] = min_value;
        }
        else {
            const auto step = param["step"].get<double>();
            const auto min_value = strategy_optimizer_inputs.current_config.get()[name].get<double>();
            const auto max_value = min_value + step;
            limits_map[name] = {min_value, step, max_value};
            current_values_map[name] = min_value;
        }
    }

    const auto increment_current_param = [&] {
        for (auto & [param_name, param_value] : current_values_map) {
            const auto [min, step, max] = limits_map[param_name];
            param_value += step;
            if (param_value < max) {
                return true;
            }
            param_value = min;
        }
        return false;
    };

    for (bool f = true; f; f = increment_current_param()) {
        nlohmann::json json;
        for (const auto & [name, value] : current_values_map) {
            json[name] = value;
        }
        ouput_jsons.emplace_back(std::move(json));
    }

    return ouput_jsons;
}

double best_profit_criteria(const StrategyResult & result)
{
    return result.final_profit;
}

double min_deviation_criteria(const StrategyResult & result)
{
    OLS::PriceRegressionFunction depo_trend{result.depo_trend_coef, result.depo_trend_const};
    if (depo_trend(result.last_position_closed_ts) <= 0.) {
        return -1.;
    }
    if (result.depo_standard_deviation <= 0.) {
        return -1.;
    }

    const double score = 1. / result.depo_standard_deviation;
    return score;
}

std::optional<DoubleJsonStrategyConfig> Optimizer::optimize()
{
    Logger::log<LogLevel::Status>("Starting optimizer");
    OptimizerParser parser(m_optimizer_inputs);

    const std::vector<DoubleJsonStrategyConfig> configs = parser.get_possible_configs();

    Guarded<OptimizerCollector> collector{
            [](const auto & res) {
                return min_deviation_criteria(res);
            }};

    Logger::log<LogLevel::Debug>("Logs will be suppressed during optimization"); // TODO push as event
    Logger::set_min_log_level(LogLevel::Warning);
    ScopeExit se{[]() {
        Logger::set_min_log_level(LogLevel::Debug);
    }};

    std::atomic<size_t> output_iter = 0;
    std::atomic<size_t> input_iter = 0;
    const auto thread_callback = [&] {
        for (auto i = input_iter.fetch_add(1);
             i < configs.size();
             i = input_iter.fetch_add(1)) {

            const auto & [entry_config, exit_config] = configs[i];
            const auto strategy_opt = StrategyFactory::build_strategy(m_strategy_name, entry_config);
            if (!strategy_opt.has_value() || !strategy_opt.value() || !strategy_opt.value()->is_valid()) {
                continue;
            }

            HistoricalMDRequestData md_request_data = {.start = m_timerange.start(), .end = m_timerange.end()};

            BacktestTradingGateway tr_gateway;
            StrategyInstance strategy_instance(
                    m_symbol,
                    md_request_data,
                    strategy_opt.value(),
                    m_exit_strategy_name,
                    exit_config,
                    m_gateway,
                    tr_gateway);
            tr_gateway.set_price_source(strategy_instance.price_channel());
            strategy_instance.set_channel_capacity(std::chrono::milliseconds{});
            strategy_instance.run_async();
            strategy_instance.finish_future().wait();
            const auto result = strategy_instance.strategy_result_channel().get();

            auto lref = collector.lock();
            if (lref.get().push(configs[i], result)) {
                Logger::logf<LogLevel::Warning>("New best config: {}, {}", result.to_json(), configs[i].to_json());
            }
            m_on_passed_check(output_iter.fetch_add(1), configs.size());
        }
    };

    m_on_passed_check(0, configs.size());
    std::list<std::thread> thread_pool;
    for (unsigned i = 0; i < m_thread_count; ++i) {
        thread_pool.emplace_back(thread_callback);
    }

    for (auto & t : thread_pool) {
        t.join();
    }

    m_on_passed_check(configs.size(), configs.size());
    return collector.lock().get().get_best();
}

bool OptimizerCollector::push(
        DoubleJsonStrategyConfig strategy_config,
        const StrategyResult & result)
{
    const auto score = m_criteria(result);
    if (score < 0.) {
        return false;
    }

    if (score <= m_best_score) {
        return false;
    }

    m_best_score = score;
    m_best = std::move(strategy_config);
    return true;
}
