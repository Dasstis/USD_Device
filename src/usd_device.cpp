#include "usd_device.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

namespace USD {

    static constexpr size_t MESSAGE_SIZE = 8;
    static constexpr int CONNECTION_TIMEOUT = 5000;

    USDDevice::USDDevice()
        : server_socket_(INVALID_SOCKET)
        , client_socket_(INVALID_SOCKET)
        , server_ip_("")
        , server_port_(0)
        , running_(false)
        , stop_requested_(false)
        , winsock_initialized_(false) {

        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            winsock_initialized_ = true;
        }

        memset(&device_addr_, 0, sizeof(device_addr_));
    }

    USDDevice::~USDDevice() {
        stop();
        disconnect();
        if (server_socket_ != INVALID_SOCKET) {
            closesocket(server_socket_);
            server_socket_ = INVALID_SOCKET;
        }
        if (winsock_initialized_) {
            WSACleanup();
        }
    }

    bool USDDevice::init(const std::string& server_ip, uint16_t server_port) {
        server_ip_ = server_ip;
        server_port_ = server_port;

        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ == INVALID_SOCKET) {
            if (callbacks_.on_error) {
                callbacks_.on_error("Failed to create server socket");
            }
            return false;
        }

        int opt = 1;
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(server_port);

        if (bind(server_socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            if (callbacks_.on_error) {
                callbacks_.on_error("Failed to bind server socket");
            }
            closesocket(server_socket_);
            server_socket_ = INVALID_SOCKET;
            return false;
        }

        if (listen(server_socket_, 5) < 0) {
            if (callbacks_.on_error) {
                callbacks_.on_error("Failed to listen on server socket");
            }
            closesocket(server_socket_);
            server_socket_ = INVALID_SOCKET;
            return false;
        }

        return true;
    }

    void USDDevice::setCallbacks(const Callbacks& callbacks) {
        callbacks_ = callbacks;
    }

    void USDDevice::start() {
        if (running_.load()) {
            return;
        }

        running_ = true;
        stop_requested_ = false;

        server_thread_ = std::make_unique<std::thread>(&USDDevice::serverThreadFunction, this);
    }

    void USDDevice::stop() {
        if (!running_.load()) {
            return;
        }

        stop_requested_ = true;
        running_ = false;

        if (server_thread_ && server_thread_->joinable()) {
            server_thread_->join();
        }
        server_thread_.reset();
    }

    void USDDevice::serverThreadFunction() {
        fd_set readfds;
        struct timeval tv;

        while (!stop_requested_.load()) {
            FD_ZERO(&readfds);
            FD_SET(server_socket_, &readfds);

            tv.tv_sec = 1;
            tv.tv_usec = 0;

            int activity = select(0, &readfds, nullptr, nullptr, &tv);

            if (activity < 0 || activity == 0) {
                continue;
            }

            if (FD_ISSET(server_socket_, &readfds)) {
                struct sockaddr_in client_addr;
                int addr_len = sizeof(client_addr);

                SOCKET client_fd = accept(server_socket_, (struct sockaddr*)&client_addr, &addr_len);
                if (client_fd == INVALID_SOCKET) {
                    continue;
                }

                std::vector<uint8_t> message(MESSAGE_SIZE);
                int received = recv(client_fd, (char*)message.data(), MESSAGE_SIZE, 0);

                if (received == MESSAGE_SIZE) {
                    handleMessage(message);
                }

                closesocket(client_fd);
            }
        }
    }

    bool USDDevice::handleMessage(const std::vector<uint8_t>& message) {
        if (message.size() < MESSAGE_SIZE) {
            return false;
        }

        uint8_t signature = message[6];
        uint8_t msg_type = message[7];

        if (signature != 0xFF) {
            return false;
        }

        if (msg_type == 1) {
            MessageType1 msg;
            if (parseMessageType1(message, msg)) {
                if (callbacks_.on_message_type1) {
                    callbacks_.on_message_type1(msg);
                }
                return true;
            }
        }
        else if (msg_type == 2) {
            MessageType2 msg;
            if (parseMessageType2(message, msg)) {
                if (callbacks_.on_message_type2) {
                    callbacks_.on_message_type2(msg);
                }
                return true;
            }
        }

        return false;
    }

    bool USDDevice::parseMessageType1(const std::vector<uint8_t>& data, MessageType1& msg) {
        if (data.size() < 8) return false;
        if (data[6] != 0xFF || data[7] != 1) return false;

        msg.timestamp = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        msg.inputs.input1 = (data[4] & 0x01) != 0;
        msg.inputs.input2 = (data[4] & 0x02) != 0;
        msg.inputs.input3 = (data[4] & 0x04) != 0;
        msg.inputs.input4 = (data[4] & 0x08) != 0;
        msg.inputs.reserved = (data[4] & 0xF0) >> 4;

        return true;
    }

    bool USDDevice::parseMessageType2(const std::vector<uint8_t>& data, MessageType2& msg) {
        if (data.size() < 8) return false;
        if (data[6] != 0xFF || data[7] != 2) return false;

        msg.timestamp = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);

        uint8_t status_low = data[4];
        uint8_t status_high = data[5];

        msg.status.s1_pressed = (status_low & 0x01) != 0;
        msg.status.s2_pressed = (status_low & 0x02) != 0;
        msg.status.s3_pressed = (status_low & 0x04) != 0;
        msg.status.s4_pressed = (status_low & 0x08) != 0;
        msg.status.s5_pressed = (status_low & 0x10) != 0;
        msg.status.battery_ok = (status_low & 0x20) != 0;
        msg.status.fifo_reinit = (status_low & 0x40) != 0;
        msg.status.power_on = (status_low & 0x80) != 0;
        msg.status.time_sync_counter = status_high;

        return true;
    }

    bool USDDevice::connectToDevice() {
        if (client_socket_ != INVALID_SOCKET) {
            closesocket(client_socket_);
            client_socket_ = INVALID_SOCKET;
        }

        client_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket_ == INVALID_SOCKET) {
            return false;
        }

        struct timeval tv;
        tv.tv_sec = CONNECTION_TIMEOUT / 1000;
        tv.tv_usec = (CONNECTION_TIMEOUT % 1000) * 1000;
        setsockopt(client_socket_, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
        setsockopt(client_socket_, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));

        if (connect(client_socket_, (struct sockaddr*)&device_addr_, sizeof(device_addr_)) < 0) {
            closesocket(client_socket_);
            client_socket_ = INVALID_SOCKET;
            return false;
        }

        return true;
    }

    void USDDevice::disconnect() {
        if (client_socket_ != INVALID_SOCKET) {
            closesocket(client_socket_);
            client_socket_ = INVALID_SOCKET;
        }
    }

    bool USDDevice::sendData(const std::vector<uint8_t>& data) {
        if (client_socket_ == INVALID_SOCKET) {
            return false;
        }

        int sent = send(client_socket_, (const char*)data.data(), (int)data.size(), 0);
        return sent == static_cast<int>(data.size());
    }

    bool USDDevice::receiveData(std::vector<uint8_t>& data) {
        if (client_socket_ == INVALID_SOCKET) {
            return false;
        }

        data.resize(MESSAGE_SIZE);
        int received = recv(client_socket_, (char*)data.data(), (int)MESSAGE_SIZE, 0);

        if (received != MESSAGE_SIZE) {
            data.clear();
            return false;
        }

        return true;
    }

    uint32_t USDDevice::getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        return static_cast<uint32_t>(time_t_now);
    }

    bool USDDevice::setTime() {
        uint32_t timestamp = getCurrentTimestamp();
        std::vector<uint8_t> data(6, 0);
        data[0] = timestamp & 0xFF;
        data[1] = (timestamp >> 8) & 0xFF;
        data[2] = (timestamp >> 16) & 0xFF;
        data[3] = (timestamp >> 24) & 0xFF;

        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE);
        command[0] = data[0];
        command[1] = data[1];
        command[2] = data[2];
        command[3] = data[3];
        command[4] = 0;
        command[5] = 0;
        command[6] = 0;
        command[7] = 2;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::requestTime() {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[7] = 0;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::requestAnalogValues() {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[7] = 1;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::requestDiscreteInputs() {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[7] = 21;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::requestUSDStatus() {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[7] = 22;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::restartDevice() {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[7] = 10;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::setSubnetMask(uint32_t mask) {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[0] = mask & 0xFF;
        command[1] = (mask >> 8) & 0xFF;
        command[2] = (mask >> 16) & 0xFF;
        command[3] = (mask >> 24) & 0xFF;
        command[7] = 3;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::setDeviceIP(uint32_t ip) {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[0] = ip & 0xFF;
        command[1] = (ip >> 8) & 0xFF;
        command[2] = (ip >> 16) & 0xFF;
        command[3] = (ip >> 24) & 0xFF;
        command[7] = 4;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::setDevicePort(uint16_t port) {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[0] = port & 0xFF;
        command[1] = (port >> 8) & 0xFF;
        command[7] = 5;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::setGatewayIP(uint32_t ip) {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[0] = ip & 0xFF;
        command[1] = (ip >> 8) & 0xFF;
        command[2] = (ip >> 16) & 0xFF;
        command[3] = (ip >> 24) & 0xFF;
        command[7] = 6;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::setServerIP(uint32_t ip) {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[0] = ip & 0xFF;
        command[1] = (ip >> 8) & 0xFF;
        command[2] = (ip >> 16) & 0xFF;
        command[3] = (ip >> 24) & 0xFF;
        command[7] = 7;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::setServerPort(uint16_t port) {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[0] = port & 0xFF;
        command[1] = (port >> 8) & 0xFF;
        command[7] = 8;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::setMACAddress(const uint8_t mac[6]) {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[0] = mac[0];
        command[1] = mac[1];
        command[2] = mac[2];
        command[3] = mac[3];
        command[4] = mac[4];
        command[5] = mac[5];
        command[7] = 9;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::initFIFO() {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[7] = 23;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::getDataForApp() {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[7] = 15;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::getDataForBoot() {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[7] = 16;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

    bool USDDevice::getDataForNov() {
        if (!connectToDevice()) return false;

        std::vector<uint8_t> command(MESSAGE_SIZE, 0);
        command[7] = 17;

        bool result = sendData(command);
        if (result) {
            std::vector<uint8_t> response;
            result = receiveData(response);
        }

        disconnect();
        return result;
    }

} // namespace USD