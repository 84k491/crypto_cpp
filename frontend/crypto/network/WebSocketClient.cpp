#include "WebSocketClient.h"

#include <nlohmann/json.hpp>

WebSocketClient::WebSocketClient(std::string url, std::optional<WsKeys> ws_keys, BusinessLogicCallback callback)
    : m_url(std::move(url))
    , m_keys(std::move(ws_keys))
    , m_callback(std::move(callback))
{
    std::thread t([this]() {
        std::cout << "websocket thread start" << std::endl;

        // Set logging to be pretty verbose (everything except message payloads)
        m_client.set_access_channels(websocketpp::log::alevel::all);
        m_client.clear_access_channels(websocketpp::log::alevel::frame_payload);

        // Initialize ASIO
        m_client.init_asio();
        m_client.set_tls_init_handler([](const auto &) -> context_ptr {
            context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

            try {
                ctx->set_options(boost::asio::ssl::context::default_workarounds |
                                 boost::asio::ssl::context::no_sslv2 |
                                 boost::asio::ssl::context::no_sslv3 |
                                 boost::asio::ssl::context::single_dh_use);
            }
            catch (std::exception & e) {
                std::cout << e.what() << std::endl;
            }
            return ctx;
        });

        // Register our message handler
        m_client.set_message_handler([this](auto, auto msg_ptr) {
            const auto payload_string = msg_ptr->get_payload();
            on_ws_message_received(payload_string);
        });
        m_client.set_open_handler([this](auto con_ptr) {
            std::cout << "Ws connection created" << std::endl;
            if (m_keys.has_value()) {
                std::string auth_msg = build_auth_message();
                try {
                    m_client.send(con_ptr, auth_msg, websocketpp::frame::opcode::text);
                }
                catch (std::exception & e) {
                    std::cout << e.what() << std::endl;
                }
            }
            else {
                on_connected();
            }

            return 0;
        });

        websocketpp::lib::error_code ec;
        m_connection = m_client.get_connection(m_url, ec);
        if (ec) {
            std::cout << "could not create connection because: " << ec.message() << std::endl;
            return 0;
        }

        // Note that connect here only requests a connection. No network messages are
        // exchanged until the event loop starts running in the next line.
        m_client.connect(m_connection);

        // Start the ASIO io_service run loop
        // this will cause a single connection to be made to the server. c.run()
        // will exit when this connection is closed.
        std::cout << "Running WS client" << std::endl;
        m_client.run();

        std::cout << "websocket client stopped" << std::endl;
        return 0;
    });
    t.detach();
}

void WebSocketClient::subscribe(const std::string & topic)
{
    std::cout << "Subscribing to " << topic << std::endl;
    std::stringstream ss;
    ss << R"({"op": "subscribe", "args": [")" << topic << R"("]})";
    m_client.send(m_connection, ss.str(), websocketpp::frame::opcode::text);
}

void WebSocketClient::unsubscribe(const std::string & topic)
{
    std::cout << "Unsubscribing from " << topic << std::endl;
    std::stringstream ss;
    ss << R"({"op": "unsubscribe", "args": [")" << topic << R"("]})";
    m_client.send(m_connection, ss.str(), websocketpp::frame::opcode::text);
}

void WebSocketClient::on_connected()
{
    std::cout << "WS client connected to " << m_url << std::endl;
    m_ready = true;
}

void WebSocketClient::send_ping()
{
    std::string ping_message = R"({"op": "ping"})";
    try {
        m_client.send(m_connection, ping_message, websocketpp::frame::opcode::text);
    }
    catch (std::exception & e) {
        std::cout << "ERROR on sending ping" << e.what() << std::endl;
    }
}

std::string WebSocketClient::sign_message(const std::string & message, const std::string & secret)
{
    unsigned char * digest = HMAC(
            EVP_sha256(),
            secret.c_str(),
            static_cast<int>(secret.size()),
            reinterpret_cast<const unsigned char *>(message.c_str()),
            message.size(),
            nullptr,
            nullptr);

    std::ostringstream os;
    for (size_t i = 0; i < 32; ++i) {
        os << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return os.str();
}

std::string WebSocketClient::build_auth_message() const
{
    if (!m_keys.has_value()) {
        std::cout << "No keys provided" << std::endl;
        return "";
    }

    int64_t expires = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count() +
            1000;

    std::string val = "GET/realtime" + std::to_string(expires);

    std::string signature = sign_message(val, m_keys.value().m_secret_key);

    nlohmann::json auth_msg = {
            {"op", "auth"},
            {"args", {m_keys.value().m_api_key, expires, signature}}};

    std::cout << "Auth message: " << auth_msg.dump() << std::endl;
    return auth_msg.dump();
}

void WebSocketClient::on_sub_response(const nlohmann::json & j)
{
    if (j.at("success") != true) {
        std::cout << "Subscription error: " << j.dump() << std::endl;
        return;
    }
    std::cout << "Subscribed" << std::endl;
    m_ping_worker = std::make_unique<WorkerThreadLoop>(
            [this](const std::atomic_bool & running) -> bool {
                constexpr std::chrono::seconds ping_interval = std::chrono::seconds(20);
                const auto start = std::chrono::steady_clock::now();
                while (running && std::chrono::steady_clock::now() - start < ping_interval) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                send_ping();
                return true;
            });
}

void WebSocketClient::on_ws_message_received(const std::string & message)
{
    nlohmann::json j = json::parse(message);
    // std::cout << "WS message received: " << message << std::endl;
    if (j.find("op") != j.end()) {
        const auto op = j.at("op");
        const std::map<std::string, std::function<void(const json &)>> op_handlers = {
                {"auth", [&](const json & j) { on_auth_response(j); }},
                {"subscribe", [&](const json & j) { on_sub_response(j); }},
                {"pong", [&](const json & j) { on_ping_response(j); }},
        };
        if (const auto it = op_handlers.find(op); it == op_handlers.end()) {
            std::cout << "Unregistered operation: " << j.dump() << std::endl;
            return;
        }
        else {
            it->second(j);
        }
        return;
    }
    m_callback(j);
}

void WebSocketClient::on_ping_response(const json & j)
{
    std::cout << "Pong received: " << j << std::endl;
}

void WebSocketClient::on_auth_response(const json & j)
{
    if (j.at("success") != true) {
        std::cout << "Auth error: " << j.dump() << std::endl;
        return;
    }

    on_connected();
}

bool WebSocketClient::wait_until_ready(std::chrono::milliseconds timeout) const
{
    auto start = std::chrono::steady_clock::now();
    while (!m_ready && std::chrono::steady_clock::now() - start < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return m_ready;
}
