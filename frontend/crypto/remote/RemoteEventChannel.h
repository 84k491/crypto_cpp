#pragma once

#include "JsonCodec.h"
#include "Priority.h"
#include "RemoteChannelClient.h"
#include "RemoteSubscription.h"

#include <functional>
#include <memory>

template <typename EventT>
class RemoteEventChannel
{
public:
    RemoteEventChannel(RemoteChannelClient & client, std::string channel_name)
        : m_client(client)
        , m_channel_name(std::move(channel_name))
    {
    }

    void push(const EventT & object)
    {
        m_client.request_throw(m_channel_name, RemoteOperation::push, {{"data", JsonCodec<EventT>::encode(object)}});
    }

    [[nodiscard]] std::shared_ptr<RemoteSubscription> subscribe(
            ILambdaAcceptor & consumer,
            xg::Guid subscriber_guid,
            std::function<void(const EventT &)> && update_callback,
            Priority priority = Priority::Normal)
    {
        const auto request_id = m_client.subscribe_throw(
                m_channel_name,
                nlohmann::json::object(),
                [&consumer,
                 &client = m_client,
                 subscriber_guid,
                 priority,
                 callback = std::move(update_callback)](const RemoteResponse & response) {
                    if (response.kind == RemoteResponseKind::closed) {
                        client.push_stream_closed_notification(consumer, subscriber_guid, priority);
                        return;
                    }
                    const auto event_opt = JsonCodec<EventT>::decode(response.data);
                    if (!event_opt.has_value()) {
                        return;
                    }
                    consumer.push(LambdaEvent{
                            subscriber_guid,
                            [callback, event = event_opt.value()] { callback(event); },
                            priority});
                });
        return std::make_shared<RemoteSubscription>(m_client, m_channel_name, request_id);
    }

private:
    RemoteChannelClient & m_client;
    std::string m_channel_name;
};
