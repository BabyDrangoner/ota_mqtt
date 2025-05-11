#include "http_session.h"
#include "http_server.h"
#include "../log.h"
#include "../sherry.h"
#include "../../include/json/json.hpp"

static size_t buffer_size = 4096;

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

HttpSession::HttpSession(Socket::ptr socket)
    :m_isClose(false)
    ,m_socket(socket)
    ,m_parser(std::make_shared<HttpParser>(buffer_size))
    ,m_read_buffer(std::unique_ptr<char[]>(new char[buffer_size])){
    
}

void HttpSession::regist_event(HttpServer::Event event, HttpServer::ptr server){
    
    if(event & HttpServer::READ){
        server->addEvent(m_socket->getSocket(), event, m_socket, [this, server](){
            size_t read_idx = m_parser->get_read_idx();
            char* buffer = this->m_read_buffer.get() + read_idx;
            size_t buffer_size = m_parser->get_buffer_size() - read_idx;
            size_t len = 0;

            len = m_socket->recv(buffer, buffer_size);
            if (len == 0) {
                SYLAR_LOG_WARN(g_logger) << "handleClient: recv failed, len=" << len;
                int fd = m_socket->getSocket();
                SYLAR_LOG_DEBUG(g_logger) << "close fd = " << fd;
                m_socket->close();
                if(fd < 0){
                    return;
                }
                this->set_close();
                HttpServer::MutexType::Lock lock(server->m_fdCtx_queue_mutex);
                server->m_reset_fdCtx_queue.push(server->m_fdContexts[fd]);
                return;
            } else if(len < 0){
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    return;
                } else {
                    SYLAR_LOG_WARN(g_logger) << "handleClient: recv failed, len=" << len;
                    int fd = m_socket->getSocket();
                    SYLAR_LOG_DEBUG(g_logger) << "close fd = " << fd;
                    m_socket->close();

                    if(fd < 0){
                        return;
                    }
                    this->set_close();
                    HttpServer::MutexType::Lock lock(server->m_fdCtx_queue_mutex);
                    server->m_reset_fdCtx_queue.push(server->m_fdContexts[fd]);
                    return;
                }
            }
            this->update_buffer_size(len, server);

        }, false);
    }

}

void HttpSession::update_buffer_size(size_t len, HttpServer::ptr server){
    m_parser->update_buffer_size(len);
    HttpParser::HTTP_CODE status = m_parser->process_read(m_read_buffer.get());
    if(status == HttpParser::GET_REQUEST){
        nlohmann::json j;

        j["cmd"] = m_parser->get_method();
        j["uri"] =  m_parser->get_uri();
        j["body"] = m_parser->get_body();
        if(!server->m_dispatcher->handle_request(j, m_socket->getSocket())){
            SYLAR_LOG_WARN(g_logger) << "fd = " << m_socket->getSocket()
                                     << ", bad request!";
        }
    } else if (status == HttpParser::BAD_REQUEST){
        SYLAR_LOG_WARN(g_logger) << "fd = " << m_socket->getSocket()
                                     << ", bad request!";
    } else if(status == HttpParser::INTERNAL_ERROR){
        SYLAR_LOG_WARN(g_logger) << "fd = " << m_socket->getSocket()
                                     << ", request error!";
    } else {
    }
    regist_event(HttpServer::READ, server);
    
}

}