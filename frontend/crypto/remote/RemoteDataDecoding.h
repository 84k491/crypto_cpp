#pragma once

#include "JsonCodec.h"

#include <stdexcept>

template <class ObjectT>
ObjectT decode_remote_data_throw(const nlohmann::json & data)
{
    const auto object_opt = JsonCodec<ObjectT>::decode(data);
    if (!object_opt.has_value()) {
        throw std::runtime_error("Can't decode remote data: " + data.dump());
    }
    return object_opt.value();
}
