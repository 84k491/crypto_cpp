#pragma once

#include "ContainerJsonCodec.h"
#include "JsonCodec.h"
#include "Priority.h"
#include "RemoteChannelClient.h"
#include "RemoteDataDecoding.h"
#include "RemoteSubscription.h"
#include "TimeseriesPointJsonCodec.h"

#include <chrono>
#include <deque>
#include <functional>
#include <memory>

template <typename ObjectT>
class RemoteEventTimeseriesChannel
{
public:
    using TimeT = std::chrono::milliseconds;
    using SnapshotCallbackT = std::function<void(const std::deque<std::pair<TimeT, ObjectT>> &)>;

    RemoteEventTimeseriesChannel(RemoteChannelClient & client, std::string channel_name)
        : m_client(client)
        , m_channel_name(std::move(channel_name))
    {
    }

    std::deque<std::pair<TimeT, ObjectT>> data_copy()
    {
        return decode_remote_data_throw<std::deque<std::pair<TimeT, ObjectT>>>(
                m_client.request_throw(m_channel_name, RemoteOperation::data_copy, nlohmann::json::object()));
    }

    std::deque<std::pair<TimeT, ObjectT>> data_copy_in_range(TimeT start, TimeT end, bool with_last_point_before_start = false)
    {
        return decode_remote_data_throw<std::deque<std::pair<TimeT, ObjectT>>>(
                m_client.request_throw(
                        m_channel_name,
                        RemoteOperation::data_copy_in_range,
                        {
                                {"start", JsonCodec<TimeT>::encode(start)},
                                {"end", JsonCodec<TimeT>::encode(end)},
                                {"with_last_point_before_start", with_last_point_before_start},
                        }));
    }

    [[nodiscard]] std::shared_ptr<RemoteSubscription> subscribe(
            ILambdaAcceptor & consumer,
            xg::Guid subscriber_guid,
            SnapshotCallbackT && snapshot_callback,
            std::function<void(TimeT, const ObjectT &)> && increment_callback)
    {
        const bool with_snapshot = static_cast<bool>(snapshot_callback);
        const auto request_id = m_client.subscribe_throw(
                m_channel_name,
                {{"with_snapshot", with_snapshot}},
                [&consumer,
                 &client = m_client,
                 subscriber_guid,
                 snapshot_callback = std::move(snapshot_callback),
                 increment_callback = std::move(increment_callback)](const RemoteResponse & response) {
                    if (response.kind == RemoteResponseKind::closed) {
                        client.push_stream_closed_notification(consumer, subscriber_guid, Priority::Normal);
                        return;
                    }
                    if (response.kind == RemoteResponseKind::snapshot) {
                        const auto points_opt = JsonCodec<std::deque<std::pair<TimeT, ObjectT>>>::decode(response.data);
                        if (!points_opt.has_value()) {
                            return;
                        }
                        consumer.push(LambdaEvent{
                                subscriber_guid,
                                [snapshot_callback, points = points_opt.value()] { snapshot_callback(points); },
                                Priority::Normal});
                        return;
                    }
                    const auto point_opt = JsonCodec<std::pair<TimeT, ObjectT>>::decode(response.data);
                    if (!point_opt.has_value()) {
                        return;
                    }
                    consumer.push(LambdaEvent{
                            subscriber_guid,
                            [increment_callback, point = point_opt.value()] {
                                increment_callback(point.first, point.second);
                            },
                            Priority::Normal});
                });
        return std::make_shared<RemoteSubscription>(m_client, m_channel_name, request_id);
    }

private:
    RemoteChannelClient & m_client;
    std::string m_channel_name;
};
