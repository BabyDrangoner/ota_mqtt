#ifndef _SHERRY_HTTPSESSION_H__
#define _SHERRY_HTTPSESSION_H__

#include "../socket.h"
#include "./http_server.h"
#include "http_parser.h"

#include <functional>

namespace sherry{
class HttpServer;

class HttpSession : public std::enable_shared_from_this<HttpSession>{
public:
    typedef std::shared_ptr<HttpSession> ptr;
    HttpSession(Socket::ptr socket);
    ~HttpSession(){}
    bool recv_request(char* buffer);
    bool send_response(char* buffer, size_t buffer_size);
    void regist_event(HttpServer::Event event, HttpServer::ptr server);

    void update_buffer_size(size_t len, HttpServer::ptr server);

    void set_close() { m_isClose = true;}
    bool isClose() const { return m_isClose;}
    

private:
    bool m_isClose;
    Socket::ptr m_socket;    
    HttpParser::ptr m_parser;
    // OTAHttpCommandDispatcher::ptr m_dispatcher;
    // std::function<void()> m_read_cb;
    // std::function<void()> m_write_cb;
public:
    std::unique_ptr<char[]> m_read_buffer;
};

}
#endif