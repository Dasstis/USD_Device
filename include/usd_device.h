#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace USD {

    struct DiscreteInputs {
        bool input1 : 1;
        bool input2 : 1;
        bool input3 : 1;
        bool input4 : 1;
        uint8_t reserved : 4;
    };

    struct USDStatus {
        bool s1_pressed : 1;
        bool s2_pressed : 1;
        bool s3_pressed : 1;
        bool s4_pressed : 1;
        bool s5_pressed : 1;
        bool battery_ok : 1;
        bool fifo_reinit : 1;
        bool power_on : 1;
        uint16_t time_sync_counter;
    };

    struct MessageType1 {
        uint32_t timestamp;
        DiscreteInputs inputs;
    };

    struct MessageType2 {
        uint32_t timestamp;
        USDStatus status;
    };

    struct Callbacks {
        std::function<void(const MessageType1&)> on_message_type1;
        std::function<void(const MessageType2&)> on_message_type2;
        std::function<void(const std::string&)> on_error;
    };

    class USDDevice {
    public:
        USDDevice();
        ~USDDevice();

        bool init(const std::string& server_ip, uint16_t server_port);
        void setCallbacks(const Callbacks& callbacks);
        void start();
        void stop();
        bool isRunning() const { return running_.load(); }

        // === ÎÑÍÎÂÍÛÅ ÊÎÌÀÍÄÛ ===
        bool setTime();
        bool requestTime();
        bool requestAnalogValues();
        bool requestDiscreteInputs();
        bool requestUSDStatus();
        bool restartDevice();

        // === ÍÀÑÒĞÎÉÊÀ ÑÅÒÈ (ÊÎÌÀÍÄÛ 3-9) ===
        bool setSubnetMask(uint32_t mask);
        bool setDeviceIP(uint32_t ip);
        bool setDevicePort(uint16_t port);
        bool setGatewayIP(uint32_t ip);
        bool setServerIP(uint32_t ip);
        bool setServerPort(uint16_t port);
        bool setMACAddress(const uint8_t mac[6]);

        // === ÓÏĞÀÂËÅÍÈÅ Î×ÅĞÅÄÜŞ (ÊÎÌÀÍÄÀ 23) ===
        bool initFIFO();

        // === ĞÀÁÎÒÀ Ñ ÏĞÎØÈÂÊÎÉ (ÊÎÌÀÍÄÛ 15-17) ===
        bool getDataForApp();
        bool getDataForBoot();
        bool getDataForNov();

    private:
        bool connectToDevice();
        void disconnect();
        bool sendData(const std::vector<uint8_t>& data);
        bool receiveData(std::vector<uint8_t>& data);

        void serverThreadFunction();
        bool handleMessage(const std::vector<uint8_t>& message);

        bool parseMessageType1(const std::vector<uint8_t>& data, MessageType1& msg);
        bool parseMessageType2(const std::vector<uint8_t>& data, MessageType2& msg);

        uint32_t getCurrentTimestamp();

    private:
        SOCKET server_socket_;
        SOCKET client_socket_;

        std::string server_ip_;
        uint16_t server_port_;
        struct sockaddr_in device_addr_;

        std::unique_ptr<std::thread> server_thread_;
        std::atomic<bool> running_;
        std::atomic<bool> stop_requested_;

        Callbacks callbacks_;

        bool winsock_initialized_;
    };

} // namespace USD