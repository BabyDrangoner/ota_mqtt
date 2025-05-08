#include "../sherry/http/http_server.h"
#include "../sherry/iomanager.h"
#include "../sherry/ota_manager.h"
#include "../sherry/http/http_send_buffer.h"

int port = 8080;

std::string protocol = "tcp";
std::string host = "localhost";
int server_port = 1883;
size_t file_size = 4096;
uint16_t device_type = 1;
int device_count = 10;  // ← 可按需修改数量

void test_http_server(){
    sherry::OTAManager::ptr ota_mgr = std::make_shared<sherry::OTAManager>(file_size, protocol, host, server_port, 1.1, "./file/");

    for (int device_no = 1; device_no <= device_count; ++device_no) {
        ota_mgr->add_device(device_type, device_no);
    }

    sherry::HttpServer::ptr http_server(new sherry::HttpServer(3, true, "test_http_server", "1.1", "application/json", ota_mgr));
    ota_mgr->set_http_server(http_server);

    http_server->start(port);

    while(true);
}

int main(){
    test_http_server();
    return 0;
}
