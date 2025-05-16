#ifndef _SHERRY_HTTPPARSER_H__
#define _SHERRY_HTTPPARSER_H__

#include "../sherry.h"
#include "http_server.h"
#include "http_buffer.h"

namespace sherry{

class HttpParser{
public:
    typedef std::shared_ptr<HttpParser> ptr;
    enum CHECK_STATE{
        CHECK_STATE_REQUESTLINE,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };

    enum LINE_STATUS{
        LINE_OK,
        LINE_OPEN,
        LINE_BAD
    };

    enum HTTP_CODE{
        INTERNAL_ERROR,
        BAD_REQUEST,
        NO_REQUEST,
        GET_REQUEST
    };

    enum HTTP_METHOD{
        POST,
        GET,
        OPTIONS,
        NONE
    };

    HttpParser(size_t buffer_size);
    ~HttpParser(){}

    HTTP_CODE process_read(HttpBuffer::ptr read_buffer);

    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_headers(char* text);
    HTTP_CODE parse_content(char* text);

    HttpParser::LINE_STATUS parse_line(HttpBuffer::ptr read_buffer);

    int get_read_idx() const { return m_read_idx;}
    size_t get_buffer_size() const { return m_buffer_size;}
    
    std::string get_method();
    std::string get_uri();
    std::string get_body();

    void update_buffer_size(size_t len) {m_read_idx += len;}

private:
    int m_read_idx = 0;
    int m_checked_idx = 0;
    int m_start_line = 0;

    size_t m_buffer_size = 0;

    std::string m_request_body;
    CHECK_STATE m_check_state;
    
    HTTP_METHOD m_method;
    std::string m_uri;

    int m_content_length = 0;
    bool m_keep_alive;
    std::string m_host;
};
}

#endif