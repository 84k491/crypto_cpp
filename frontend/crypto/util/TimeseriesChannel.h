#pragma once

#include "ISubsription.h"

#include <chrono>
#include <crossguid/guid.hpp>
#include <functional>
#include <list>
#include <memory>
#include <vector>

template <typename ObjectT>
class TimeseriesChannel;

template <typename ObjectT>
class TimeseriesSubsription final : public ISubsription
{
    friend class TimeseriesChannel<ObjectT>;

public:
    TimeseriesSubsription(TimeseriesChannel<ObjectT> & channel, xg::Guid guid)
        : m_channel(&channel)
        , m_guid(guid)
    {
    }

    ~TimeseriesSubsription() override
    {
        if (m_channel) {
            m_channel->unsubscribe(m_guid);
        }
    }

private:
    TimeseriesChannel<ObjectT> * m_channel;
    xg::Guid m_guid;
};

template <typename ObjectT>
class TimeseriesChannel
{
public:
    using TimeT = std::chrono::milliseconds;

    TimeseriesChannel() = default;
    ~TimeseriesChannel();

    void push(TimeT timestamp, const ObjectT & object);
    [[nodiscard]] std::shared_ptr<TimeseriesSubsription<ObjectT>> subscribe(
            std::function<void(const std::vector<std::pair<TimeT, ObjectT>> &)> && snapshot_callback,
            std::function<void(TimeT, const ObjectT &)> && increment_callback);

    void unsubscribe(xg::Guid guid);

private:
    std::vector<std::pair<TimeT, ObjectT>> m_data;
    std::list<std::tuple<
            xg::Guid,
            std::function<void(TimeT, const ObjectT &)>,
            std::weak_ptr<TimeseriesSubsription<ObjectT>>>>
            m_increment_callbacks;
};

template <typename ObjectT>
void TimeseriesChannel<ObjectT>::push(TimeseriesChannel::TimeT timestamp, const ObjectT & object)
{
    m_data.emplace_back(timestamp, object);
    for (const auto & [uuid, cb, wptr] : m_increment_callbacks) {
        cb(timestamp, object);
    }
}

template <typename ObjectT>
std::shared_ptr<TimeseriesSubsription<ObjectT>> TimeseriesChannel<ObjectT>::subscribe(
        std::function<void(const std::vector<std::pair<TimeT, ObjectT>> &)> && snapshot_callback,
        std::function<void(TimeT, const ObjectT &)> && increment_callback)
{
    const auto guid = xg::newGuid();
    auto sptr = std::make_shared<TimeseriesSubsription<ObjectT>>(*this, guid);

    m_increment_callbacks.emplace_back(guid, std::move(increment_callback), std::weak_ptr{sptr});
    snapshot_callback(m_data);

    return sptr;
}

template <typename ObjectT>
void TimeseriesChannel<ObjectT>::unsubscribe(xg::Guid guid)
{
    for (auto it = m_increment_callbacks.begin(); it != m_increment_callbacks.end(); ++it) {
        if (std::get<xg::Guid>(*it) == guid) {
            m_increment_callbacks.erase(it);
            break;
        }
    }
}

template <typename ObjectT>
TimeseriesChannel<ObjectT>::~TimeseriesChannel()
{
    for (auto & [uuid, _, wptr] : m_increment_callbacks) {
        if (auto sptr = wptr.lock()) {
            sptr->m_channel = nullptr;
        }
    }
    m_increment_callbacks.clear();
}
