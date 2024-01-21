#include "ByBitTradingGateway.h"

#include "ohlc.h"

#include <future>
#include <list>

struct OrderResponse
{
    std::string category;
    std::string symbol;
    std::string orderId;
    std::string orderLinkId;
    std::string side;
    std::string orderStatus;  // ":"Filled",
    std::string cancelType;   // ":"UNKNOWN",
    std::string rejectReason; // ":"EC_NoError",
    std::string timeInForce;  // ":"IOC",
    double price;             // ":"40323.7",
    double qty;               // ":"0.002",
    double leavesQty;         // ":"0",
    double cumExecQty;        // ":"0.002",
    double cumExecFee;        // ":"0 .046684",
    std::string orderType;    // ":"Market",
    std::string updatedTime;  // ":"1705336167611",
    // "blockTradeId":"",
    // "positionIdx":0,
    // "isLeverage":"",
    // "avgPrice":"42440",
    // "leavesValue":"0",
    // "cumExecValue":"84.88",
    // "stopOrderType":"",
    // "orderIv":"",
    // "triggerPrice":"",
    // "takeProfit":"",
    // "stopLoss":"",
    // "triggerBy":"",
    // "tpTriggerBy":"",
    // "slTriggerBy":"",
    // "triggerDirect ion":0,
    // "placeType":"",
    // "lastPriceOnCreated":"42446",
    // "closeOnTrigger":false,
    // "reduceOnly":false,
    // "smpGroup":0,
    // "smpType":"None",
    // "smpOrderId":"",
    // "slLimitPrice":"0",
    // "tpLimitPrice": "0",
    // "tpslMode":"UNKNOWN",
    // "createType":"CreateByUser",
    // "marketUnit":"",
    // "createdTime":"1705336167608",
    // "feeCurrency":""
};

struct OrderResponseResult
{
    std::string id;
    uint64_t creationTime;

    std::list<OrderResponse> orders;
};

void from_json(const json & j, OrderResponse & order)
{
    j.at("category").get_to(order.category);
    j.at("symbol").get_to(order.symbol);
    j.at("orderId").get_to(order.orderId);
    j.at("orderLinkId").get_to(order.orderLinkId);
    j.at("side").get_to(order.side);
    j.at("orderStatus").get_to(order.orderStatus);
    j.at("cancelType").get_to(order.cancelType);
    j.at("rejectReason").get_to(order.rejectReason);
    j.at("timeInForce").get_to(order.timeInForce);
    j.at("price").get_to(order.price);
    j.at("qty").get_to(order.qty);
    j.at("leavesQty").get_to(order.leavesQty);
    j.at("cumExecQty").get_to(order.cumExecQty);
    j.at("cumExecFee").get_to(order.cumExecFee);
    j.at("orderType").get_to(order.orderType);
    j.at("updatedTime").get_to(order.updatedTime);
}

void from_json(const json & j, OrderResponseResult & order)
{
    j.at("id").get_to(order.id);
    j.at("creationTime").get_to(order.creationTime);

    for (const auto & item : j.at("data")) {
        OrderResponse order_response;
        item.get_to(order_response);
        order.orders.push_back(order_response);
    }
}

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
            {"category", "linear"},
            {"symbol", order.symbol()},
            {"side", order.side_str()},
            {"orderType", "Market"},
            {"qty", std::to_string(order.unsigned_volume())},
            {"timeInForce", "IOC"},
            {"orderLinkId", order_id},
            {"orderFilter", "Order"},
    };

    const auto request = json_order.dump();

    const auto [it, success] = m_pending_orders.try_emplace(order_id, std::promise<OrderResponse>(), order);
    if (!success) {
        std::cout << "Trying to send order while order_id already exists: " << order_id << std::endl;
        return false;
    }
    const std::string url = "https://api-testnet.bybit.com/v5/order/create";
    auto request_future = rest_client.request_auth_async(url, request, m_api_key, m_secret_key);
    request_future.wait();
    const std::string request_result = request_future.get();
    std::cout << "REST Trading Response: " << request_result << std::endl;

    auto response_future = it->second.first.get_future();
    if (response_future.wait_for(ws_wait_timeout) == std::future_status::timeout) {
        std::cout << "Timeout waiting for response" << std::endl;
        return false;
    }
    const auto response = response_future.get();

    // TODO calculate slippage, check volume

    return true;
}

void ByBitTradingGateway::on_ws_message_received(const std::string & message)
{
    json j = json::parse(message);
    std::cout << "WS message received: " << message << std::endl;
    OrderResponse response;
    from_json(j, response);

    const auto it = m_pending_orders.find(response.orderLinkId);
    if (it != m_pending_orders.end()) {
        // it->second.on_response(response);
        m_pending_orders.erase(it);
    }
}
