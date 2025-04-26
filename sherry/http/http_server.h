#ifndef _SHERRY_HTTPSERVER_H__
#define _SHERRY_HTTPSERVER_H__

#include "../sherry.h"
#include "../socket.h"
#include "../scheduler.h"
#include "../iomanager.h"
#include "../ota_http_command_dispatcher.h"

#include <memory.h>
#include <atomic>

namespace sherry{

class HttpServer{
public:
    typedef std::shared_ptr<HttpServer> ptr;
    typedef Mutex MutexType;

    HttpServer(IOManager::ptr io_mgr, OTAManager::ptr ota_mgr=nullptr);
    ~HttpServer();

    // 启动监听服务
    bool start(uint16_t port);
    void stop();

private:
    void handleClient(uint32_t client_id, Socket::ptr client);  // 每个连接的处理
    void del_socket(uint32_t client_id);

private:
    MutexType m_mutex;
    std::atomic<bool> m_running;  // 控制server是否运行
    IOManager::ptr m_io_mgr;
    OTAHttpCommandDispatcher::ptr m_dispatcher;
    Socket::ptr m_listenSocket;       // 保存监听socket
    std::atomic<uint32_t> m_connection_id;
    std::unordered_map<uint32_t, Socket::ptr> m_sockets;

};

}

#endif