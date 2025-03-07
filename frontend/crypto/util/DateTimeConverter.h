#pragma once

#include <chrono>
#include <string>

namespace DateTimeConverter {

std::string date_time(std::chrono::milliseconds ts);
std::string date(std::chrono::milliseconds ts);

}
