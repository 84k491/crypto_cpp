#include "RemoteChannelServerPart.h"

#include "RemoteMessagesJsonCodec.h"

#include <stdexcept>

RemoteChannelServerPart::RemoteChannelServerPart(ILambdaAcceptor & event_loop)
    : m_event_loop(event_loop)
{
}

RemoteChannelServerPart::~RemoteChannelServerPart()
{
    for (auto & remote_subscriber : m_subscribers) {
        remote_subscriber.subscriber.reset();
        m_event_loop.push(LambdaEvent{
                xg::newGuid(),
                [connection = remote_subscriber.connection, request_id = remote_subscriber.request_id] {
                    connection->send(RemoteResponse{request_id, RemoteResponseKind::closed, {}});
                },
                Priority::Normal});
    }
}

void RemoteChannelServerPart::forget_connection(xg::Guid connection_guid)
{
    m_subscribers.remove_if([&](const RemoteSubscriber & remote_subscriber) {
        return remote_subscriber.connection->guid() == connection_guid;
    });
}

EventSubcriber & RemoteChannelServerPart::add_subscriber(const std::shared_ptr<RemoteConnection> & connection, uint64_t request_id)
{
    m_subscribers.push_back({connection, request_id, std::make_unique<EventSubcriber>(m_event_loop)});
    return *m_subscribers.back().subscriber;
}

void RemoteChannelServerPart::handle_unsubscribe_request_throw(const RemoteRequest & request, const std::shared_ptr<RemoteConnection> & connection)
{
    if (request.operation != RemoteOperation::unsubscribe) {
        throw std::runtime_error("Unsupported operation: " + request.operation);
    }

    const auto subscription_request_id = request.arguments.at("subscription_request_id").get<uint64_t>();
    m_subscribers.remove_if([&](const RemoteSubscriber & remote_subscriber) {
        return remote_subscriber.connection->guid() == connection->guid() && remote_subscriber.request_id == subscription_request_id;
    });
    connection->send(RemoteResponse{request.request_id, RemoteResponseKind::reply, {}});
}
