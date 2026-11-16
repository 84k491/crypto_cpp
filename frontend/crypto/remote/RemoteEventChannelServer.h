#pragma once

#include "EventChannel.h"
#include "RemoteChannelServerPart.h"
#include "RemoteDataDecoding.h"
#include "RemoteMessagesJsonCodec.h"

template <typename EventT>
class RemoteEventChannelServer final : public RemoteChannelServerPart
{
public:
    RemoteEventChannelServer(EventChannel<EventT> & channel, ILambdaAcceptor & event_loop)
        : RemoteChannelServerPart(event_loop)
        , m_channel(channel)
    {
    }

    void handle_request_throw(const RemoteRequest & request, const std::shared_ptr<RemoteConnection> & connection) override
    {
        if (request.operation == RemoteOperation::push) {
            m_channel.push(decode_remote_data_throw<EventT>(request.arguments.at("data")));
            connection->send(RemoteResponse{.request_id = request.request_id, .kind = RemoteResponseKind::reply, .data = {}});
            return;
        }

        if (request.operation == RemoteOperation::subscribe) {
            add_subscriber(connection, request.request_id)
                    .subscribe(
                            m_channel,
                            [connection, request_id = request.request_id](const EventT & event) {
                                connection->send(RemoteResponse{
                                        request_id,
                                        RemoteResponseKind::update,
                                        JsonCodec<EventT>::encode(event)});
                            });
            connection->send(RemoteResponse{.request_id = request.request_id, .kind = RemoteResponseKind::subscribed, .data = {}});
            return;
        }

        handle_unsubscribe_request_throw(request, connection);
    }

private:
    EventChannel<EventT> & m_channel;
};
