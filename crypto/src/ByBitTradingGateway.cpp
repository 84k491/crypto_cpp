#include "ByBitTradingGateway.h"

#include "ohlc.h"

ByBitTradingGateway::ByBitTradingGateway()
    : m_url("wss://stream-testnet.bybit.com/v5/private")
    , m_api_key("eGa7t3mip2WNVd9XiR")
    , m_secret_key("X6MIgs0BT5wAalKgOLMubF9SLcUfr51UWoai")
{
    subscribe();
}

void ByBitTradingGateway::subscribe()
{
    std::thread t([this]() {
        std::cout << "websocket thread start" << std::endl;

        // Set logging to be pretty verbose (everything except message payloads)
        client.set_access_channels(websocketpp::log::alevel::all);
        client.clear_access_channels(websocketpp::log::alevel::frame_payload);

        // Initialize ASIO
        client.init_asio();
        client.set_tls_init_handler([](const auto &) -> context_ptr {
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
        client.set_message_handler([this](auto, auto msg_ptr) {
            const auto payload_string = msg_ptr->get_payload();
            on_ws_message_received(payload_string);
        });
        client.set_open_handler([this](auto con_ptr) {
            std::cout << "Sending message" << std::endl;
            std::string auth_msg = build_auth_message();
            std::string subscription_message = R"({"op": "subscribe", "args": ["order"]})";
            try {
                client.send(con_ptr, auth_msg, websocketpp::frame::opcode::text);
                client.send(con_ptr, subscription_message, websocketpp::frame::opcode::text);
            }
            catch (std::exception & e) {
                std::cout << e.what() << std::endl;
                return 0;
            }
            return 0;
        });

        websocketpp::lib::error_code ec;
        m_connection = client.get_connection(m_url, ec);
        if (ec) {
            std::cout << "could not create connection because: " << ec.message() << std::endl;
            return 0;
        }

        // Note that connect here only requests a connection. No network messages are
        // exchanged until the event loop starts running in the next line.
        client.connect(m_connection);

        // Start the ASIO io_service run loop
        // this will cause a single connection to be made to the server. c.run()
        // will exit when this connection is closed.
        std::cout << "Running WS client" << std::endl;
        client.run();

        std::cout << "websocket client stopped" << std::endl;
        return 0;
    });
    t.detach();
}

std::string ByBitTradingGateway::sign_message(const std::string & message, const std::string & secret)
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

std::string ByBitTradingGateway::build_auth_message() const
{
    int64_t expires = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count() +
            1000;

    std::string val = "GET/realtime" + std::to_string(expires);

    std::string signature = sign_message(val, m_secret_key);

    json auth_msg = {
            {"op", "auth"},
            {"args", {m_api_key, expires, signature}}};

    std::cout << "Auth message: " << auth_msg.dump() << std::endl;
    return auth_msg.dump();
}

bool ByBitTradingGateway::send_order_sync(const MarketOrder & order)
{
    int64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

    const std::string order_id = std::to_string(ts);
    json json_order = {
            {"category", "spot"},
            {"symbol", order.symbol()},
            {"side", order.side_str()},
            {"orderType", "Market"},
            {"marketUnit","quoteCoin"},
            {"qty", std::to_string(order.unsigned_volume())},
            {"timeInForce", "IOC"},
            {"orderLinkId", order_id},
            {"isLeverage", 0},
            {"orderFilter", "Order"},
    };

    const auto request = json_order.dump();

    const auto [it, success] = m_pending_orders.try_emplace(order_id, order);
    if (!success) {
        std::cout << "Trying to send order while order_id already exists: " << order_id << std::endl;
        return false;
    }
    const std::string url = "https://api-testnet.bybit.com/v5/order/create";
    auto response_future = rest_client.request_auth_async(url, request, m_api_key, m_secret_key);
    response_future.wait();
    const std::string response = response_future.get();
    std::cout << "REST Trading Response: " << response << std::endl;

    return true;
}

void ByBitTradingGateway::on_ws_message_received(const std::string & message)
{
    std::cout << "WS message received: " << message << std::endl;
}
