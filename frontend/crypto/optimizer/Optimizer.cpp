#include "Optimizer.h"

#include "BacktestTradingGateway.h"
#include "Collector.h"
#include "JsonStrategyConfig.h"
#include "Logger.h"
#include "ScopeExit.h"
#include "StrategyFactory.h"
#include "StrategyInstance.h"

#include <vector>

Optimizer::Optimizer(
        ByBitMarketDataGateway & gateway,
        Symbol symbol,
        Timerange timerange,
        std::string strategy_name,
        std::string exit_strategy_name,
        OptimizerInputs optimizer_data,
        size_t threads)
    : m_gateway(gateway)
    , m_symbol(std::move(symbol))
    , m_timerange(timerange)
    , m_optimizer_inputs(std::move(optimizer_data))
    , m_strategy_name(std::move(strategy_name))
    , m_exit_strategy_name(std::move(exit_strategy_name))
    , m_thread_count(threads)
{
}

void Optimizer::subscribe_for_passed_check(std::function<void(int, int)> && on_passed_checks)
{
    m_on_passed_check = std::move(on_passed_checks);
}

std::optional<DoubleJsonStrategyConfig> Optimizer::optimize()
{
    Logger::log<LogLevel::Status>("Starting optimizer");
    OptimizerParser parser(m_optimizer_inputs);

    const std::vector<DoubleJsonStrategyConfig> configs = parser.get_possible_configs();

    Guarded<OptimizerCollector> collector{
            "MinDeviation",
            std::vector<OptimizerCollector::FilterParams>{
                    {.filter_name = "Apr",
                     .value = 5.}}};

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
            strategy_instance.wait_event_barrier(); // use future to wait
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
