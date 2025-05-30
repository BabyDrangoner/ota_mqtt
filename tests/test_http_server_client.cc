#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>

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

void send_http_post_command(int sockfd, const std::string& cmd, const std::string& body) {
    std::string http_request;
    http_request += "POST /api/ota/";
    http_request += cmd;
    http_request += " HTTP/1.1\r\n";
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

void send_http_get_command(int sockfd, const std::string& cmd, const std::string& detail, const std::string& save_path) {
    std::string http_request;
    http_request += "GET /download/ota/";
    http_request += detail;
    http_request += cmd;
    http_request += " HTTP/1.1\r\n";
    http_request += "Host: 127.0.0.1:8080\r\n";
    http_request += "Connection: close\r\n\r\n";

    if (send(sockfd, http_request.c_str(), http_request.size(), 0) <= 0) {
        std::cerr << "Send failed\n";
        close(sockfd);
        return;
    }

    const int BUF_SIZE = 4096;
    char buffer[BUF_SIZE];
    std::string response;
    std::string::size_type header_end_pos = std::string::npos;

    // 1. 接收 header（直到 \r\n\r\n）
    while (true) {
        int len = recv(sockfd, buffer, BUF_SIZE, 0);
        if (len <= 0) {
            std::cerr << "recv failed or closed\n";
            close(sockfd);
            return;
        }
        response.append(buffer, len);
        header_end_pos = response.find("\r\n\r\n");
        if (header_end_pos != std::string::npos) break;
    }

    std::string header = response.substr(0, header_end_pos);
    std::string body_start = response.substr(header_end_pos + 4);  // first part of file
    std::cout << "Header:\n" << header << "\n";

    // 2. 提取 Content-Length
    size_t content_length = 0;
    std::istringstream hss(header);
    std::string line;
    while (std::getline(hss, line)) {
        if (line.find("Content-Length:") != std::string::npos) {
            content_length = std::stoul(line.substr(line.find(":") + 1));
            break;
        }
    }

    if (content_length == 0) {
        std::cerr << "No valid Content-Length found\n";
        close(sockfd);
        return;
    }

    // 3. 打开输出文件
    std::ofstream outfile(save_path, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Failed to open file: " << save_path << "\n";
        close(sockfd);
        return;
    }

    // 4. 写入 body_start（header后收到的内容）
    outfile.write(body_start.data(), body_start.size());
    size_t total_received = body_start.size();

    // 5. 持续接收直到读满 Content-Length
    while (total_received < content_length) {
        int len = recv(sockfd, buffer, BUF_SIZE, 0);
        if (len <= 0) {
            std::cerr << "Connection closed unexpectedly\n";
            break;
        }
        outfile.write(buffer, len);
        total_received += len;
    }

    outfile.close();
    close(sockfd);
    std::cout << "File saved to: " << save_path << " (" << total_received << " bytes)\n";
}



void test_query(int device_type, int device_count) {
    for (int device_no = 1; device_no <= device_count; ++device_no) {
        nlohmann::json j;
        j["command"] = "query";
        j["device_type"] = device_type;
        j["device_no"] = device_no;
        j["action"] = "getVersion";

        int sockfd = connect_to_server("127.0.0.1", 8080);
        if (sockfd >= 0) {
            send_http_post_command(sockfd, "query", j.dump());
        }

        sleep(1);
    }
}

void test_notify(int device_type, const std::string& name, const std::string& version) {
    nlohmann::json j;
    j["command"] = "notify";
    j["device_type"] = device_type;
    j["name"] = name;
    j["version"] = version;

    int sockfd = connect_to_server("127.0.0.1", 8080);
    if (sockfd >= 0) {
        send_http_post_command(sockfd, "notify", j.dump());
    }
}

void test_stop_notify(int device_type, const std::string& name, const std::string& version) {
    nlohmann::json j;
    j["command"] = "stop_notify";
    j["device_type"] = device_type;
    j["name"] = name;
    j["version"] = version;

    int sockfd = connect_to_server("127.0.0.1", 8080);
    if (sockfd >= 0) {
        send_http_post_command(sockfd, "stop_notify", j.dump());
    }
}

void test_query_download(int device_type, int device_no, std::string& name){
    nlohmann::json j;
    j["command"] = "query_download";
    j["device_type"] = device_type;
    j["device_no"] = device_no;
    j["name"] = name;

    int sockfd = connect_to_server("127.0.0.1", 8080);
    if(sockfd >= 0){
        send_http_post_command(sockfd, "query_download", j.dump());
    }
}

void test_file_download(int device_type, const std::string& name, const std::string& version){
    std::string detail;
    detail += std::to_string(device_type);
    detail += "/" + name + "/" + version + "/";

    int sockfd = connect_to_server("127.0.0.1", 8080);
    if(sockfd >= 0){
        send_http_get_command(sockfd, "file_download", detail, "./tests/download_file/file.zip");
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: ./test_client [query device_type device_count] | [notify device_type device_no version]\n";
        return 0;
    }

    std::string cmd = argv[1];
    if (cmd == "query") {
        if (argc != 4) {
            std::cerr << "Usage: ./test_client query device_type device_count\n";
            return 1;
        }
        int device_type = std::stoi(argv[2]);
        int device_count = std::stoi(argv[3]);
        test_query(device_type, device_count);
    } else if (cmd == "notify") {
        if (argc != 5) {
            std::cerr << "Usage: ./test_client notify device_type device_no version\n";
            return 1;
        }
        int device_type = std::stoi(argv[2]);
        std::string name = argv[3];
        std::string version = argv[4];
        test_notify(device_type, name, version);
    } else if (cmd == "stop_notify") {
        if (argc != 5) {
            std::cerr << "Usage: ./test_client stop_notify device_type name version\n";
            return 1;
        }
        int device_type = std::stoi(argv[2]);
        std::string name = argv[3];
        std::string version = argv[4];
        test_stop_notify(device_type, name, version);
    } else if (cmd == "query_download"){
        if (argc != 5) {
            std::cerr << "Usage: ./test_client query device_type device_no name\n";
            return 1;
        }
        int device_type = std::stoi(argv[2]);
        int device_no = std::stoi(argv[3]);
        std::string name = argv[4];
        test_query_download(device_type, device_no, name);
    } else if (cmd == "file_download"){
        if(argc != 5){
            std::cerr << "Usage: ./test_http_server_client file_download device_type name version";
        }
        int device_type = std::stoi(argv[2]);
        std::string name = argv[3];
        std::string version = argv[4];
        test_file_download(device_type, name, version);
    
    } else {
        std::cout << "Unknown command: " << cmd << "\n";
    }

    return 0;
}
