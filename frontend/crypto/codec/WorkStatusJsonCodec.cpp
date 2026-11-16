#include "WorkStatusJsonCodec.h"

nlohmann::json JsonCodec<WorkStatus>::encode(const WorkStatus & object)
{
    return to_string(object);
}

std::optional<WorkStatus> JsonCodec<WorkStatus>::decode(const nlohmann::json & json)
{
    try {
        const auto text = json.get<std::string>();
        for (const auto status : {WorkStatus::Stopped, WorkStatus::Running, WorkStatus::Panic}) {
            if (to_string(status) == text) {
                return status;
            }
        }
        return std::nullopt;
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
