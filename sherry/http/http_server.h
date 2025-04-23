#ifndef _SHERRY_HTTPSERVER_H__
#define _SHERRY_HTTPSERVER_H__

#include "../sherry.h"
#include "../socket.h"
#include "../scheduler.h"
#include "../iomanager.h"

#include <memory.h>
#include <atomic>

namespace sherry{

class HttpServer{
public:
    typedef std::shared_ptr<HttpServer> ptr;
    HttpServer(IOManager::ptr io_mgr);
    ~HttpServer();

    // 启动监听服务
    bool start(uint16_t port);
    void stop();

private:
    void handleClient(Socket::ptr client);  // 每个连接的处理

private:
    std::atomic<bool> m_running;  // 控制server是否运行
    IOManager::ptr m_io_mgr;
    Socket::ptr m_listenSocket;       // 保存监听socket
};

}

#endif