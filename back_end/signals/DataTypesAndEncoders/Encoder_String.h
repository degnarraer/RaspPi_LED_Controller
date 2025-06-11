#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

// Helper trait to detect std::vector
template<typename T>
struct is_std_vector : std::false_type {};

template<typename T, typename A>
struct is_std_vector<std::vector<T, A>> : std::true_type {};

// to_string for scalar types
template <typename T>
inline typename std::enable_if<!is_std_vector<T>::value, std::string>::type
to_string(const T& value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

// to_string for vector<T>
template <typename T>
inline std::string to_string(const std::vector<T>& vec)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        if (i != 0)
            oss << ", ";
        oss << to_string(vec[i]);
    }
    oss << "]";
    return oss.str();
}

// from_string for scalar types (disable for vectors)
template <typename T>
typename std::enable_if<!is_std_vector<T>::value && !std::is_same<T, std::string>::value, T>::type
from_string(const std::string& str)
{
    std::istringstream iss(str);
    T value;
    if (!(iss >> value))
    {
        throw std::invalid_argument("Failed to parse scalar from string: " + str);
    }
    return value;
}

inline std::string from_string(const std::string& str)
{
    return str;
}

// Helper: trim whitespace
inline std::string trim(const std::string& s)
{
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

// from_string for vector<T>
template <typename T>
typename std::enable_if<is_std_vector<T>::value, T>::type
from_string(const std::string& str)
{
    using ElemT = typename T::value_type;
    T result;

    std::string s = trim(str);

    // Optional: strip brackets if present
    if (!s.empty() && s.front() == '[' && s.back() == ']')
    {
        s = s.substr(1, s.size() - 2);
    }

    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, ','))
    {
        token = trim(token);
        if (!token.empty())
        {
            result.push_back(from_string<ElemT>(token));
        }
    }

    return result;
}