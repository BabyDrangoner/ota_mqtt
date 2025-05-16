#include "http_session.h"
#include "http_server.h"
#include "../log.h"
#include "../sherry.h"
#include "../../include/json/json.hpp"

#include <fcntl.h>

static size_t buffer_size = 4096;

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

HttpSession::HttpSession(Socket::ptr socket, HttpServer::ptr server)
    :m_isClose(false)
    ,m_socket(socket)
    ,m_parser(std::make_shared<HttpParser>(buffer_size))
    ,m_read_buffer(std::make_shared<HttpBuffer>(buffer_size))
    ,m_write_buffer(std::make_shared<HttpBuffer>(buffer_size))
    ,m_send_flag(true){
    int pipefd[2];
    if(pipe2(pipefd, O_NONBLOCK) == -1){
        SYLAR_LOG_ERROR(g_logger) << "Http Session fd = " << m_socket->getSocket()
                                  << "pipe create failed: " << strerror(errno);
    }
    m_pipe_read_fd = pipefd[0];
    m_pipe_write_fd = pipefd[1];
    
    server->addEvent(m_pipe_read_fd, HttpServer::READ, m_socket, [this, server](){
        this->onPipeReadable(server);
    });

}

void HttpSession::regist_event(HttpServer::Event event, HttpServer::ptr server){
    
    if(event & HttpServer::READ){
        server->addEvent(m_socket->getSocket(), event, m_socket, [this, server](){
            size_t size = this->m_read_buffer->get_available_size();
            char buffer[size];
            SYLAR_LOG_DEBUG(g_logger) << "read_buffer available size = " << size;
            size_t len = 0;

            len = m_socket->recv(buffer, size);
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
            SYLAR_LOG_DEBUG(g_logger) << "recv len = " << len;
            SYLAR_LOG_DEBUG(g_logger) << "recv:\n" << std::string(buffer);
            this->m_read_buffer->write(buffer, len);
            this->update_buffer_size(len, server);
        }, false);
    } else if(event & HttpServer::WRITE) {
        HttpSession::MutexType::Lock lock(this->m_mutex);

        size_t size = this->m_write_buffer->get_data_size();
        if(size == 0){
            SYLAR_LOG_WARN(g_logger) << "session fd = " << m_socket->getSocket()  
                                     << ", write_buffer len = 0.";
            // m_write_buffer->print_infos();
            if(m_write_fiber){
                server->schedule(m_write_fiber);
                m_write_fiber = nullptr;
            }
            server->addEvent(m_pipe_read_fd, HttpServer::READ, m_socket, [this, server](){
                this->onPipeReadable(server);
            });
            return;
        }
        // SYLAR_LOG_DEBUG(g_logger) << "write size = " << size;
        std::shared_ptr<char[]> data = this->m_write_buffer->read_by_size(size);
        m_send_flag = false;
        // SYLAR_LOG_DEBUG(g_logger) << "regist send: m_send_flag = false";
        if(m_write_fiber){
            server->schedule(m_write_fiber);
            m_write_fiber = nullptr;
        }

        server->addEvent(m_socket->getSocket(), event, m_socket, [this, data, size, server](){
            this->send(data.get(), size, server, data);
        }, false);

    }

}

void HttpSession::send(char* p_data, size_t size, HttpServer::ptr server, std::shared_ptr<char[]> data){
    if(size == 0){
        return;
    }
    server->addEvent(m_socket->getSocket(), HttpServer::WRITE, m_socket, [this, p_data, data, size, server](){
        int len = m_socket->send(p_data, size);
        if(len == -1){
            return;
        }
        if((size_t)len < size){
            this->send(p_data + len, size - len, server, data);
            return;
        }

        HttpSession::MutexType::Lock lock(m_mutex);

        m_send_flag = true;
        // SYLAR_LOG_DEBUG(g_logger) << "send: m_send_flag = true";
        server->addEvent(m_pipe_read_fd, HttpServer::READ, m_socket, [this, server](){
            this->onPipeReadable(server);
        });

    });
}

void HttpSession::update_buffer_size(size_t len, HttpServer::ptr server){
    m_parser->update_buffer_size(len);
    HttpParser::HTTP_CODE status = m_parser->process_read(m_read_buffer);
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

bool HttpSession::write_buffer(char* buffer, size_t size){
    if(size == 0){
        return false;
    }

    while(!m_write_buffer->write(buffer, size)){
        // SYLAR_LOG_DEBUG(g_logger) << "write buffer failed";
        // int start_idx = m_write_buffer->get_start_idx(), end_idx = m_write_buffer->get_end_idx();
        // SYLAR_LOG_DEBUG(g_logger) << "start_idx = " << start_idx
        //                           << ", end_idx = " << end_idx;
        m_write_fiber = Fiber::GetThis();
        Fiber::YieldToHold();
    }
    // SYLAR_LOG_DEBUG(g_logger) << "write buffer start to lock";

    MutexType::Lock lock(m_mutex);
    if(m_send_flag){
        // SYLAR_LOG_DEBUG(g_logger) << "write buffer: m_send_flag = true, wirte pipe.";
        write(m_pipe_write_fd, "T", 1);
    }
    return true;
}

bool HttpSession::write_buffer(const std::string& buffer){
    if(buffer.size() == 0){
        return false;
    }

    while(!m_write_buffer->write(buffer.data(), buffer.size())){
        // SYLAR_LOG_DEBUG(g_logger) << "write buffer failed";
        m_write_fiber = Fiber::GetThis();
        Fiber::YieldToReady();
    }
    // SYLAR_LOG_DEBUG(g_logger) << "write buffer start to lock";
    MutexType::Lock lock(m_mutex);    

    if(m_send_flag){
        write(m_pipe_write_fd, "T", 1);
    }
    return true;
}

void HttpSession::onPipeReadable(HttpServer::ptr server) {
    {
        HttpSession::MutexType::Lock lock(this->m_mutex);
        char buffer[10];

        while (true) {
            ssize_t len = ::read(m_pipe_read_fd, buffer, sizeof(buffer));
            if (len > 0) {
                continue;
            } else if (len == -1 && errno == EAGAIN) {
                break;
            } else if (len == 0) {
                break;
            } else {
                SYLAR_LOG_ERROR(g_logger) << "session fd = " << m_socket->getSocket()
                                        << ", pipe read error.";
                break;
            }
        }
    }

    while (!m_send_flag) {
        // SYLAR_LOG_DEBUG(g_logger) << "m_send_flag = false, regist yield to ready.";
        Fiber::YieldToReady();
    }
    // SYLAR_LOG_DEBUG(g_logger) << "regist send";

    this->regist_event(HttpServer::WRITE, server);
}



}