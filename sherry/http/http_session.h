#ifndef _SHERRY_HTTPSESSION_H__
#define _SHERRY_HTTPSESSION_H__

#include "../socket.h"
#include "./http_server.h"
#include "http_parser.h"
#include "http_buffer.h"

#include <functional>

namespace sherry{
class HttpServer;

class HttpSession : public std::enable_shared_from_this<HttpSession>{
public:
    typedef std::shared_ptr<HttpSession> ptr;
    typedef Mutex MutexType;
    HttpSession(Socket::ptr socket, HttpServer::ptr server);
    ~HttpSession(){}

    void send(char* p_data, size_t size, HttpServer::ptr server, std::shared_ptr<char[]> data);
    
    bool write_buffer(char* buffer, size_t size);
    bool write_buffer(const std::string& buffer);
    void regist_event(HttpServer::Event event, HttpServer::ptr server);

    void update_buffer_size(size_t len, HttpServer::ptr server);

    void set_close() { m_isClose = true;}
    bool isClose() const { return m_isClose;}

    void onPipeReadable(HttpServer::ptr server);
    
private:
    MutexType m_mutex;
    bool m_isClose;
    Socket::ptr m_socket;    
    HttpParser::ptr m_parser;

public:
    HttpBuffer::ptr m_read_buffer;
    HttpBuffer::ptr m_write_buffer;
    std::atomic<bool> m_send_flag;
    int m_pipe_read_fd;
    int m_pipe_write_fd;
    Fiber::ptr m_write_fiber;
};

}
#endif