#include "../sherry/http/http_server.h"
#include "../sherry/iomanager.h"
#include "../sherry/ota_manager.h"
int port = 8080;

std::string protocol = "tcp";
std::string host = "localhost";
int server_port = 1883;

uint16_t device_type = 1;


void test_http_server(){
    sherry::IOManager::ptr io_mgr(new sherry::IOManager(3));
    sherry::OTAManager::ptr ota_mgr = std::make_shared<sherry::OTAManager>(protocol, host, server_port);
    ota_mgr->add_device(device_type, 1);
    sherry::HttpServer::ptr http_server(new sherry::HttpServer(io_mgr, ota_mgr));

    http_server->start(port);

    while(true);
}

int main(){

    test_http_server();
    return 0;
}