#pragma once

#include "ISubsription.h"

#include <chrono>
#include <crossguid2/crossguid/guid.hpp>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <vector>

template <typename ObjectT>
class TimeseriesPublisher;

template <typename ObjectT>
class TimeseriesSubsription final : public ISubsription
{
    friend class TimeseriesPublisher<ObjectT>;

public:
    TimeseriesSubsription(TimeseriesPublisher<ObjectT> & publisher, xg::Guid guid)
        : m_publisher(&publisher)
        , m_guid(guid)
    {
    }

    ~TimeseriesSubsription() override
    {
        if (m_publisher) {
            m_publisher->unsubscribe(m_guid);
        }
    }

private:
    TimeseriesPublisher<ObjectT> * m_publisher;
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
    [[nodiscard]] std::shared_ptr<TimeseriesSubsription<ObjectT>> subscribe(
            std::function<void(const std::vector<std::pair<TimeT, const ObjectT>> &)> && snapshot_callback,
            std::function<void(TimeT, const ObjectT &)> && increment_callback);

    void unsubscribe(xg::Guid guid);

private:
    std::vector<std::pair<TimeT, const ObjectT>> m_data;
    std::list<std::tuple<
            xg::Guid,
            std::function<void(TimeT, const ObjectT &)>,
            std::weak_ptr<TimeseriesSubsription<ObjectT>>>>
            m_increment_callbacks;
};

template <typename ObjectT>
void TimeseriesPublisher<ObjectT>::push(TimeseriesPublisher::TimeT timestamp, const ObjectT & object)
{
    m_data.emplace_back(timestamp, object);
    for (const auto & [uuid, cb, wptr] : m_increment_callbacks) {
        cb(timestamp, object);
    }
}

template <typename ObjectT>
std::shared_ptr<TimeseriesSubsription<ObjectT>> TimeseriesPublisher<ObjectT>::subscribe(
        std::function<void(const std::vector<std::pair<TimeT, const ObjectT>> &)> && snapshot_callback,
        std::function<void(TimeT, const ObjectT &)> && increment_callback)
{
    const auto guid = xg::newGuid();
    auto sptr = std::make_shared<TimeseriesSubsription<ObjectT>>(*this, guid);

    m_increment_callbacks.emplace_back(guid, std::move(increment_callback), std::weak_ptr{sptr});
    snapshot_callback(m_data);

    return sptr;
}

template <typename ObjectT>
void TimeseriesPublisher<ObjectT>::unsubscribe(xg::Guid guid)
{
    for (auto it = m_increment_callbacks.begin(); it != m_increment_callbacks.end(); ++it) {
        if (std::get<xg::Guid>(*it) == guid) {
            m_increment_callbacks.erase(it);
            break;
        }
    }
}

template <typename ObjectT>
TimeseriesPublisher<ObjectT>::~TimeseriesPublisher()
{
    for (auto & [uuid, _, wptr] : m_increment_callbacks) {
        if (auto sptr = wptr.lock()) {
            sptr->m_publisher = nullptr;
        }
    }
    m_increment_callbacks.clear();
}
