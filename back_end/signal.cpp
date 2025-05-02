#include "signal.h"

std::string encode_signal_name_and_json(const std::string& signal, const json& value)
{
    json j;
    j["type"] = "signal";
    j["signal"] = signal;
    j["value"] = value;
    return j.dump();
}

std::string encode_FFT_Bands(const std::string& signal, const std::vector<float>& values)
{
    std::vector<std::string> labels = {
        "16 Hz", "20 Hz", "25 Hz", "31.5 Hz", "40 Hz", "50 Hz", "63 Hz", "80 Hz", "100 Hz",
        "125 Hz", "160 Hz", "200 Hz", "250 Hz", "315 Hz", "400 Hz", "500 Hz", "630 Hz", "800 Hz", "1000 Hz", "1250 Hz",
        "1600 Hz", "2000 Hz", "2500 Hz", "3150 Hz", "4000 Hz", "5000 Hz", "6300 Hz", "8000 Hz", "10000 Hz", "12500 Hz",
        "16000 Hz", "20000 Hz"
    };

    json valueJson = encode_labels_with_values(labels, values);
    return encode_signal_name_and_json(signal, valueJson);
}

SignalManager& SignalManager::GetInstance()
{
    static SignalManager instance;
    return instance;
}

SignalManager::SignalManager()
{
    logger_ = InitializeLogger("Signal Manager", spdlog::level::info);
}
