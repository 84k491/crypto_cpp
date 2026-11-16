#pragma once

#include "ContainerJsonCodec.h"
#include "EventTimeseriesChannel.h"
#include "RemoteChannelServerPart.h"
#include "RemoteDataDecoding.h"
#include "RemoteMessagesJsonCodec.h"
#include "TimeseriesPointJsonCodec.h"

template <typename ObjectT>
class RemoteEventTimeseriesChannelServer final : public RemoteChannelServerPart
{
    using TimeT = std::chrono::milliseconds;
    using PointsT = std::deque<std::pair<TimeT, ObjectT>>;

public:
    RemoteEventTimeseriesChannelServer(EventTimeseriesChannel<ObjectT> & channel, ILambdaAcceptor & event_loop)
        : RemoteChannelServerPart(event_loop)
        , m_channel(channel)
    {
    }

    void handle_request_throw(const RemoteRequest & request, const std::shared_ptr<RemoteConnection> & connection) override
    {
        if (request.operation == RemoteOperation::data_copy) {
            connection->send(RemoteResponse{
                    request.request_id,
                    RemoteResponseKind::reply,
                    JsonCodec<PointsT>::encode(m_channel.data_copy())});
            return;
        }

        if (request.operation == RemoteOperation::data_copy_in_range) {
            const auto points = m_channel.data_copy_in_range(
                    decode_remote_data_throw<TimeT>(request.arguments.at("start")),
                    decode_remote_data_throw<TimeT>(request.arguments.at("end")),
                    request.arguments.at("with_last_point_before_start").get<bool>());
            connection->send(RemoteResponse{
                    request.request_id,
                    RemoteResponseKind::reply,
                    JsonCodec<PointsT>::encode(points)});
            return;
        }

        if (request.operation == RemoteOperation::subscribe) {
            subscribe_connection_throw(request, connection);
            return;
        }

        handle_unsubscribe_request_throw(request, connection);
    }

private:
    void subscribe_connection_throw(const RemoteRequest & request, const std::shared_ptr<RemoteConnection> & connection)
    {
        typename EventTimeseriesChannel<ObjectT>::SnapshotCallbackT send_snapshot;
        if (request.arguments.at("with_snapshot").get<bool>()) {
            send_snapshot = [connection, request_id = request.request_id](const PointsT & points) {
                connection->send(RemoteResponse{
                        request_id,
                        RemoteResponseKind::snapshot,
                        JsonCodec<PointsT>::encode(points)});
            };
        }

        add_subscriber(connection, request.request_id)
                .subscribe(
                        m_channel,
                        std::move(send_snapshot),
                        [connection, request_id = request.request_id](TimeT ts, const ObjectT & object) {
                            connection->send(RemoteResponse{
                                    request_id,
                                    RemoteResponseKind::update,
                                    JsonCodec<std::pair<TimeT, ObjectT>>::encode({ts, object})});
                        });
        connection->send(RemoteResponse{request.request_id, RemoteResponseKind::subscribed, {}});
    }

private:
    EventTimeseriesChannel<ObjectT> & m_channel;
};
