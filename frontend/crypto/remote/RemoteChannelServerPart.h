#pragma once

#include "EventLoopSubscriber.h"
#include "ILambdaAcceptor.h"
#include "RemoteConnection.h"
#include "RemoteMessages.h"

#include <list>
#include <memory>

class RemoteChannelServerPart
{
public:
    explicit RemoteChannelServerPart(ILambdaAcceptor & event_loop);
    virtual ~RemoteChannelServerPart();

    virtual void handle_request_throw(const RemoteRequest & request, const std::shared_ptr<RemoteConnection> & connection) = 0;
    void forget_connection(xg::Guid connection_guid);

protected:
    EventSubcriber & add_subscriber(const std::shared_ptr<RemoteConnection> & connection, uint64_t request_id);
    void handle_unsubscribe_request_throw(const RemoteRequest & request, const std::shared_ptr<RemoteConnection> & connection);

private:
    struct RemoteSubscriber
    {
        std::shared_ptr<RemoteConnection> connection;
        uint64_t request_id = 0;
        std::unique_ptr<EventSubcriber> subscriber;
    };

    ILambdaAcceptor & m_event_loop;
    std::list<RemoteSubscriber> m_subscribers;
};
