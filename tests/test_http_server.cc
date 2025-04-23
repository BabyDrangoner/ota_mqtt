#include "../sherry/http/http_server.h"
#include "../sherry/iomanager.h"

int port = 8080;

void test_http_server(){
    sherry::IOManager::ptr io_mgr(new sherry::IOManager(2));
    sherry::HttpServer::ptr http_server(new sherry::HttpServer(io_mgr));

    http_server->start(port);

    while(true);
}

int main(){

    test_http_server();
    return 0;
}