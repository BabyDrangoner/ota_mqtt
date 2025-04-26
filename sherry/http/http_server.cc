#include "http_server.h"
#include "../address.h"

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

HttpServer::HttpServer(IOManager::ptr io_mgr, OTAManager::ptr ota_mgr)
    :m_running(false)
    ,m_io_mgr(io_mgr)
    ,m_connection_id(0){
    m_dispatcher = std::make_shared<OTAHttpCommandDispatcher>(ota_mgr);
    
}

HttpServer::~HttpServer(){
    stop();
    for(auto it : m_sockets){
        it.second->close();
    }
}

bool HttpServer::start(uint16_t port){
    if(m_running){
        SYLAR_LOG_WARN(g_logger) << "HttpServer already running.";
        return false;
    }

    m_listenSocket = Socket::CreateTCPSocket();
    auto addr = IPAddress::Create("0.0.0.0", port);
    SYLAR_LOG_INFO(g_logger) << "listen fd = " << m_listenSocket->getSocket();

    if(!m_listenSocket->bind(addr)){
        SYLAR_LOG_ERROR(g_logger) << "HttpServer bind failed on port " << port
                                  << ".";
        return false;
    }

    if(!m_listenSocket->listen()){
        SYLAR_LOG_ERROR(g_logger) << "HttpServer listen failed.";
        return false;
    }

    SYLAR_LOG_INFO(g_logger) << "HttpServer started on port " << port
                             << ".";
    m_running = true;

    while(m_running){
        auto client = m_listenSocket->accept();
        if(client){
            SYLAR_LOG_DEBUG(g_logger) << "client is not null";
            uint32_t client_id = m_connection_id++;
            m_sockets[client_id] = client;
            m_io_mgr->schedule([this, client_id, client](){
                this->handleClient(client_id, client);
            });
            
        } 
    }
    return true;
}

void HttpServer::del_socket(uint32_t client_id){
    MutexType::Lock lock(m_mutex);
    auto it = m_sockets.find(client_id);
    if(it == m_sockets.end()) return;
    if(m_sockets[client_id]->isConnected()) {
        m_sockets[client_id]->close();
    }
    m_sockets.erase(it);
}

void HttpServer::stop(){
    if(!m_running) return;
    m_running = false;
    if(m_listenSocket){
        m_listenSocket->close();
        m_listenSocket.reset();
    }

    SYLAR_LOG_INFO(g_logger) << "HttpServer stopped.";
}

void HttpServer::handleClient(uint32_t client_id, Socket::ptr client) {
    if (!client) {
        SYLAR_LOG_ERROR(g_logger) << "handleClient: client is null";
        return;
    }

    char buffer[4096] = {0};
    int len = client->recv(buffer, sizeof(buffer) - 1);
    if (len <= 0) {
        SYLAR_LOG_WARN(g_logger) << "handleClient: recv failed, len=" << len;
        client->close();
        del_socket(client_id);
        return;
    }

    std::string request(buffer);
    SYLAR_LOG_INFO(g_logger) << "Received:\n" << request;

    try {
        // 先找 HTTP 报文头和 body 的分隔
        size_t pos = request.find("\r\n\r\n");
        if (pos == std::string::npos) {
            SYLAR_LOG_ERROR(g_logger) << "handleClient: Invalid HTTP request format, missing header-body separator.";
            client->close();
            del_socket(client_id);
            return;
        }

        // 提取 body
        std::string body = request.substr(pos + 4); // 跳过 "\r\n\r\n"
        SYLAR_LOG_INFO(g_logger) << "Extracted body:\n" << body;

        // 解析 body成JSON对象
        nlohmann::json json_request = nlohmann::json::parse(body);

        // 调用 dispatcher 处理 JSON命令
        m_dispatcher->handle_command(json_request, client_id);

    } catch (const std::exception& e) {
        SYLAR_LOG_ERROR(g_logger) << "handleClient: JSON parse error: " << e.what();
        client->close();
        del_socket(client_id);
        return;
    }
}





}