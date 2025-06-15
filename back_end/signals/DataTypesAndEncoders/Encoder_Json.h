#pragma once

#include <nlohmann/json.hpp>
using json = nlohmann::json;

template<typename T>
using JsonEncoder = std::function<std::string(const std::string&, const T&)>;