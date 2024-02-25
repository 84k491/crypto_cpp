#include "RestClient.h"

#include <chrono>
#include <openssl/evp.h>
#include <openssl/hmac.h>

std::future<std::string> RestClient::request_async(const std::string & request)
{
    return std::async(
            std::launch::async,
            [this, &request]() {
                std::condition_variable cv;
                std::mutex m;

                std::cout << "REST request: " << request << std::endl;
                std::string string_result;
                client
                        .Build()
                        ->Get(request)
                        .WithCompletion([&](const restincurl::Result & result) {
                            std::lock_guard lock(m);
                            string_result = result.body;
                            cv.notify_all();
                        })
                        .Execute();

                std::unique_lock lock(m);
                cv.wait(lock, [&] { return !string_result.empty(); });

                return string_result;
            });
}

// TODO move it to separate file along with ByBitTradingGateway::sign_message
std::string sign_message(const std::string & header_data, const std::string & message, const std::string & secret)
{
    const std::string whole_data = header_data + message;
    std::cout << "Signing message: " << whole_data << std::endl;
    unsigned char * digest = HMAC(
            EVP_sha256(),
            secret.c_str(),
            static_cast<int>(secret.size()),
            reinterpret_cast<const unsigned char *>(whole_data.c_str()),
            whole_data.size(),
            nullptr,
            nullptr);

    std::ostringstream os;
    for (size_t i = 0; i < 32; ++i) {
        os << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return os.str();
}

std::future<std::string> RestClient::request_auth_async(
        const std::string & url,
        const std::string & request,
        const std::string & api_key,
        const std::string & secret_key)
{
    return std::async(
            std::launch::async,
            [this, request, api_key, secret_key, url]() {
                std::condition_variable cv;
                std::mutex m;

                std::string timestamp_str = std::to_string(
                        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

                const std::string window_str = "5000";
                const std::string header_data = timestamp_str + api_key + window_str;
                std::string sign = sign_message(header_data, request, secret_key);
                std::cout << "Sign: <" << sign << ">" << std::endl;

                std::cout << "REST request: " << request << std::endl;
                std::string string_result;
                client
                        .Build()
                        ->Post(url)
                        .Header("X-BAPI-SIGN", sign)
                        .Header("X-BAPI-API-KEY", api_key)
                        .Header("X-BAPI-TIMESTAMP", timestamp_str)
                        .Header("X-BAPI-RECV-WINDOW", window_str)
                        .Header("Content-Type", "application/json")
                        .SendData(request)
                        .WithCompletion([&](const restincurl::Result & result) {
                            std::lock_guard lock(m);
                            string_result = result.body;
                            cv.notify_all();
                        })
                        .Execute();

                std::unique_lock lock(m);
                cv.wait(lock, [&] { return !string_result.empty(); });

                return string_result;
            });
}
