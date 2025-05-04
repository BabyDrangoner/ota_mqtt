#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>

#include "../include/json/json.hpp"
#include "../sherry/sherry.h"

static sherry::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

int connect_to_server(const std::string& ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Socket creation failed\n";
        return -1;
    }

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (connect(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        std::cerr << "Connect failed\n";
        close(sockfd);
        return -1;
    }

    return sockfd;
}

void send_http_json_command(int sockfd, const std::string& body) {
    std::string http_request;
    http_request += "POST /api/ota/cmd HTTP/1.1\r\n";
    http_request += "Host: 127.0.0.1:8080\r\n";
    http_request += "Content-Type: application/json\r\n";
    http_request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    http_request += "Connection: close\r\n";
    http_request += "\r\n";
    http_request += body;

    int send_ret = send(sockfd, http_request.c_str(), http_request.size(), 0);
    if (send_ret <= 0) {
        std::cerr << "Send failed or connection closed\n";
        return;
    }

    std::cout << "Sent HTTP command.\n";

    char buffer[4096] = {0};
    int len = recv(sockfd, buffer, sizeof(buffer) - 1, 0);

    if (len > 0) {
        buffer[len] = '\0';
        std::cout << "Server response:\n" << buffer << "\n";
    } else {
        std::cerr << "recv failed or connection closed\n";
    }

    close(sockfd);
}

void test_query() {
    std::string body = R"({
        "command": "query",
        "device_type": 1,
        "device_no": 1,
        "action": "getVersion"
    })";

    int sockfd = connect_to_server("127.0.0.1", 8080);
    if (sockfd >= 0) {
        send_http_json_command(sockfd, body);
    }
}

void test_notify() {
    std::string body = R"({
        "command": "notify",
        "device_type": 1,
        "device_no": 1,
        "version": "6.0.0.1"
    })";

    int sockfd = connect_to_server("127.0.0.1", 8080);
    if (sockfd >= 0) {
        send_http_json_command(sockfd, body);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: ./test_client [query|notify]\n";
        return 0;
    }

    std::string cmd = argv[1];
    if (cmd == "query") {
        test_query();
    } else if (cmd == "notify") {
        test_notify();
    } else {
        std::cout << "Unknown command: " << cmd << "\n";
    }

    return 0;
}
