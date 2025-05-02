#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

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
    servaddr.sin_port = htons(8080); // 你的服务器端口
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // 你的服务器地址

    if (connect(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        std::cerr << "Connect failed\n";
        close(sockfd);
        return -1;
    }

    // 要发送的 JSON body
    std::string body = R"({"command":"query","device_type":1,"device_no":1,"action":"getVersion"})";
    SYLAR_LOG_INFO(g_logger) << "Extracted body:\n" << body;
    nlohmann::json j = nlohmann::json::parse(body);
    // 构造完整HTTP请求
    std::string http_request;
    http_request += "POST /api/ota/cmd HTTP/1.1\r\n";
    http_request += "Host: 127.0.0.1:8080\r\n";
    http_request += "Content-Type: application/json\r\n";
    http_request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    http_request += "Connection: close\r\n";
    http_request += "\r\n";  // 头部结束
    http_request += body;    // 加上实际 body

    // 发送
    send(sockfd, http_request.c_str(), http_request.size(), 0);
    std::cout << "Sent HTTP formatted command.\n";

    // 接收服务器响应（可选）
    char buffer[4096] = {0};
    int len = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (len > 0) {
        buffer[len] = '\0';
        std::cout << "Server response:\n" << buffer << "\n";
    } else {
        std::cout << "No response or recv failed.\n";
    }

    close(sockfd);
    return 0;
}
