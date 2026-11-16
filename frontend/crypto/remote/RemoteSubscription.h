#pragma once

#include "ISubsription.h"
#include "RemoteChannelClient.h"

#include <string>

class RemoteSubscription final : public ISubscription
{
public:
    RemoteSubscription(RemoteChannelClient & client, std::string channel_name, uint64_t request_id)
        : m_client(client)
        , m_channel_name(std::move(channel_name))
        , m_request_id(request_id)
    {
    }

    ~RemoteSubscription() override
    {
        m_client.unsubscribe(m_channel_name, m_request_id);
    }

private:
    RemoteChannelClient & m_client;
    std::string m_channel_name;
    uint64_t m_request_id;
};
