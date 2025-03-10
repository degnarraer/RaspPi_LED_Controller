#include <iostream>
#include "signal.h"
#include "i2s_microphone.h"

int main() {
    std::cout << "Hello, World!" << std::endl;
    I2SMicrophone mic = I2SMicrophone("Microphone", 44100, 2, 100);
    return 0;
} 