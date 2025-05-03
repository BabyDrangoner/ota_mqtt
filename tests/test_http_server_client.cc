#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>

#include "../include/json/json.hpp"
#include "../sherry/sherry.h"

static sherry::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Socket creation failed\n";
        return -1;
    }

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        std::cerr << "Connect failed\n";
        close(sockfd);
        return -1;
    }

    std::string body = R"({"command":"query","device_type":1,"device_no":1,"action":"getVersion"})";

    while (true) {
        std::string http_request;
        http_request += "POST /api/ota/cmd HTTP/1.1\r\n";
        http_request += "Host: 127.0.0.1:8080\r\n";
        http_request += "Content-Type: application/json\r\n";
        http_request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        http_request += "Connection: keep-alive\r\n";  // 使用长连接
        http_request += "\r\n";
        http_request += body;

        int send_ret = send(sockfd, http_request.c_str(), http_request.size(), 0);
        if (send_ret <= 0) {
            std::cerr << "Send failed or connection closed\n";
            break;
        }
        std::cout << "Sent HTTP command.\n";

        char buffer[4096] = {0};
        int len = recv(sockfd, buffer, sizeof(buffer) - 1, 0);

        if (len > 0) {
            buffer[len] = '\0';
            std::cout << "Server response:\n" << buffer << "\n";
        } else if (len == 0) {
            std::cout << "Server closed the connection.\n";
            break;
        } else {
            std::cerr << "recv failed: " << strerror(errno) << "\n";
            break;
        }

        sleep(1);
    }

    close(sockfd);
    return 0;
}
