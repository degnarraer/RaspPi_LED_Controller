#include <iostream>
#include "signal.h"
#include "i2s_microphone.h"

void Microphone_Callback(const std::vector<int32_t>& data)
{
    std::cout << "Callback!" << std::endl;
}

int main() {
    std::cout << "Hello, World!" << std::endl;
    I2SMicrophone mic = I2SMicrophone("Microphone", 44100, 2, 100);
    mic.StartReading(Microphone_Callback);
    return 0;
}
