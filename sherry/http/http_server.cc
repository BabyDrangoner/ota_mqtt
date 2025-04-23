#include "http_server.h"
#include "../address.h"


namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

HttpServer::HttpServer(IOManager::ptr io_mgr)
    :m_running(false){
    m_io_mgr = io_mgr;
}

HttpServer::~HttpServer(){
    stop();
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
    
            m_io_mgr->schedule([this, client](){
                this->handleClient(client);
            });
            
        } 
    }
    return true;
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

void HttpServer::handleClient(Socket::ptr client){
    if (!client) {
        SYLAR_LOG_ERROR(g_logger) << "handleClient: client is null";
        return;
    }

    char buffer[4096] = {0};
    int len = client->recv(buffer, sizeof(buffer) - 1);
    if(len <= 0){
        SYLAR_LOG_WARN(g_logger) << "handleClient: recv failed, len=" << len;
        client->close();
        return;
    }

    buffer[len] = '\0';  // ⚠️ 确保字符串安全终止（可选）
    std::string request(buffer);
    SYLAR_LOG_INFO(g_logger) << "Received:\n" << request;

    if(request.find("GET") == 0){
        std::string body = "<html><body><h1>Hello from HttpServer (Fiber)!</h1></body></html>";
        std::stringstream ss;
        ss << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: text/html\r\n"
           << "Content-Length: " << body.size() << "\r\n"
           << "Connection: close\r\n\r\n"
           << body;

        std::string response = ss.str();
        int sent = client->send(response.c_str(), response.size());
        SYLAR_LOG_INFO(g_logger) << "Sent response: " << sent << " bytes";
    } else {
        std::string err = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
        client->send(err.c_str(), err.size());
    }

    client->close();
}




}