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
    std::shared_ptr<TimeseriesSubsription<ObjectT>> subscribe(
            std::function<void(const std::vector<std::pair<TimeT, const ObjectT>> &)> && snapshot_callback,
            std::function<void(TimeT, const ObjectT &)> && increment_callback);

    void unsubscribe(xg::Guid guid);

private:
    std::vector<std::pair<TimeT, const ObjectT>> m_data;
    std::map<
            xg::Guid,
            std::pair<std::function<void(TimeT, const ObjectT &)>,
                      std::weak_ptr<TimeseriesSubsription<ObjectT>>>>
            m_increment_callbacks;
};

template <typename ObjectT>
void TimeseriesPublisher<ObjectT>::push(TimeseriesPublisher::TimeT timestamp, const ObjectT & object)
{
    m_data.emplace_back(timestamp, object);
    for (const auto & [uuid, cb_wptr_pair] : m_increment_callbacks) {
        auto & [cb, wptr] = cb_wptr_pair;
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

    const auto [it, success] = m_increment_callbacks.try_emplace(guid, std::move(increment_callback), std::weak_ptr{sptr});
    if (!success) {
        std::cout << "ERROR subscriber with uuid " << it->first << " already exists" << std::endl;
    }
    snapshot_callback(m_data);

    return sptr;
}

template <typename ObjectT>
void TimeseriesPublisher<ObjectT>::unsubscribe(xg::Guid guid)
{
    m_increment_callbacks.erase(guid);
}

template <typename ObjectT>
TimeseriesPublisher<ObjectT>::~TimeseriesPublisher()
{
    for (auto & [uuid, cb_wptr_pair] : m_increment_callbacks) {
        auto & [_, wptr] = cb_wptr_pair;
        if (auto sptr = wptr.lock()) {
            sptr->m_publisher = nullptr;
        }
    }
    m_increment_callbacks.clear();
}
