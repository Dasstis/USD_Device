// examples/example_usage.cpp
#include "usd_device.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <windows.h>

using namespace USD;

bool g_running = true;

BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        g_running = false;
        return TRUE;
    }
    return FALSE;
}

int main() {
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    std::cout << "=== USD Device Example ===" << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl << std::endl;

    USDDevice device;

    
    Callbacks callbacks;
    callbacks.on_message_type1 = [](const MessageType1& msg) {
        std::cout << "[Message Type 1] Timestamp: " << msg.timestamp
            << " Inputs: "
            << (msg.inputs.input1 ? "1" : "0")
            << (msg.inputs.input2 ? "1" : "0")
            << (msg.inputs.input3 ? "1" : "0")
            << (msg.inputs.input4 ? "1" : "0") << std::endl;
        };

    callbacks.on_message_type2 = [](const MessageType2& msg) {
        std::cout << "[Message Type 2] Timestamp: " << msg.timestamp
            << " PowerOn: " << (msg.status.power_on ? "YES" : "NO")
            << " Battery: " << (msg.status.battery_ok ? "OK" : "LOW") << std::endl;
        };

    callbacks.on_error = [](const std::string& error) {
        std::cerr << "[Error] " << error << std::endl;
        };

    device.setCallbacks(callbacks);

    
    std::cout << "Initializing device..." << std::endl;
    if (!device.init("127.0.0.1", 60000)) {
        std::cerr << "Failed to initialize device" << std::endl;
        return 1;
    }

    
    std::cout << "Starting device..." << std::endl;
    device.start();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Отправка команд
    std::cout << "\nSending commands..." << std::endl;

    std::cout << "1. Setting time..." << std::endl;
    device.setTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "2. Requesting time..." << std::endl;
    device.requestTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "3. Requesting analog values..." << std::endl;
    device.requestAnalogValues();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "4. Requesting discrete inputs..." << std::endl;
    device.requestDiscreteInputs();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "5. Requesting USD status..." << std::endl;
    device.requestUSDStatus();

    std::cout << "\nWaiting for messages... Press Ctrl+C to stop" << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nStopping device..." << std::endl;
    device.stop();

    std::cout << "Done." << std::endl;
    return 0;
}