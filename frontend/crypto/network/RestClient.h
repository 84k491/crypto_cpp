#pragma once

#include "restincurl.h"

#include <future>

class RestClient
{
public:
    std::future<std::string> request_async(const std::string & request);
    std::future<std::string> request_auth_async(
            const std::string & url,
            const std::string & request,
            const std::string & api_key,
            const std::string & secret_key,
            std::optional<std::chrono::milliseconds> timeout);

private:
    restincurl::Client client; // TODO use mrtazz/restclient-cpp
};
