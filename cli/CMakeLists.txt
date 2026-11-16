cmake_minimum_required(VERSION 3.5)

add_executable(crypto-cli
    main.cpp
)

target_link_libraries(crypto-cli PRIVATE
    nlohmann_json::nlohmann_json
    crossguid
    remote
    codec
    strategy
    trading_primitives
    util
)
