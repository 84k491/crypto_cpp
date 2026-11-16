#pragma once

#include "EventObjectChannel.h"
#include "RemoteChannelServerPart.h"
#include "RemoteMessagesJsonCodec.h"

template <typename ObjectT>
class RemoteEventObjectChannelServer final : public RemoteChannelServerPart
{
public:
    RemoteEventObjectChannelServer(EventObjectChannel<ObjectT> & channel, ILambdaAcceptor & event_loop)
        : RemoteChannelServerPart(event_loop)
        , m_channel(channel)
    {
    }

    void handle_request_throw(const RemoteRequest & request, const std::shared_ptr<RemoteConnection> & connection) override
    {
        if (request.operation == RemoteOperation::get) {
            connection->send(RemoteResponse{
                    request.request_id,
                    RemoteResponseKind::reply,
                    JsonCodec<ObjectT>::encode(m_channel.get())});
            return;
        }

        if (request.operation == RemoteOperation::subscribe) {
            add_subscriber(connection, request.request_id)
                    .subscribe(
                            m_channel,
                            [connection, request_id = request.request_id](const ObjectT & object) {
                                connection->send(RemoteResponse{
                                        request_id,
                                        RemoteResponseKind::update,
                                        JsonCodec<ObjectT>::encode(object)});
                            });
            connection->send(RemoteResponse{.request_id = request.request_id, .kind = RemoteResponseKind::subscribed, .data = {}});
            return;
        }

        handle_unsubscribe_request_throw(request, connection);
    }

private:
    EventObjectChannel<ObjectT> & m_channel;
};
