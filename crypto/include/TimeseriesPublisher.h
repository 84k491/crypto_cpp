#pragma once

#include "ISubsription.h"
#include <chrono>
#include <crossguid2/crossguid/guid.hpp>
#include <functional>
#include <map>
#include <memory>
#include <vector>

template <typename ObjectT>
class TimeseriesPublisher;

template <typename ObjectT>
class TimeseriesSubsription final : public ISubsription
{
public:
    TimeseriesSubsription(TimeseriesPublisher<ObjectT> & publisher, xg::Guid guid)
        : m_publisher(publisher)
        , m_guid(guid)
    {
    }

    ~TimeseriesSubsription() override
    {
        m_publisher.unsubscribe(m_guid);
    }

private:
    TimeseriesPublisher<ObjectT> & m_publisher;
    xg::Guid m_guid;
};

template <typename ObjectT>
class TimeseriesPublisher
{
public:
    using TimeT = std::chrono::milliseconds;

    TimeseriesPublisher() = default;
    ~TimeseriesPublisher();

    void push(TimeT timestamp, const ObjectT & object);
    std::unique_ptr<TimeseriesSubsription<ObjectT>> subscribe(
            std::function<void(const std::vector<std::pair<TimeT, const ObjectT>> &)> && snapshot_callback,
            std::function<void(TimeT, const ObjectT &)> && increment_callback);

    void unsubscribe(xg::Guid guid);

private:
    std::vector<std::pair<TimeT, const ObjectT>> m_data;
    std::map<xg::Guid, std::function<void(TimeT, const ObjectT &)>> m_increment_callbacks;
};

template <typename ObjectT>
void TimeseriesPublisher<ObjectT>::push(TimeseriesPublisher::TimeT timestamp, const ObjectT & object)
{
    m_data.emplace_back(timestamp, object);
    for (const auto & [uuid, cb] : m_increment_callbacks) {
        cb(timestamp, object);
    }
}

template <typename ObjectT>
std::unique_ptr<TimeseriesSubsription<ObjectT>> TimeseriesPublisher<ObjectT>::subscribe(
        std::function<void(const std::vector<std::pair<TimeT, const ObjectT>> &)> && snapshot_callback,
        std::function<void(TimeT, const ObjectT &)> && increment_callback)
{
    snapshot_callback(m_data);
    const auto [it, success] = m_increment_callbacks.try_emplace(xg::newGuid(), std::move(increment_callback));
    if (!success) {
        std::cout << "ERROR subscriber with uuid " << it->first << " already exists" << std::endl;
    }

    return std::make_unique<TimeseriesSubsription<ObjectT>>(*this, it->first);
}

template <typename ObjectT>
void TimeseriesPublisher<ObjectT>::unsubscribe(xg::Guid guid)
{
    m_increment_callbacks.erase(guid);
}

template <typename ObjectT>
TimeseriesPublisher<ObjectT>::~TimeseriesPublisher()
{
    if (!m_increment_callbacks.empty()) {
        std::cout << "ERROR got " << m_increment_callbacks.size() << " active subscribers left on destruction" << std::endl;
    }
    m_increment_callbacks.clear();
}
