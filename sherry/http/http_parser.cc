#include "http_parser.h"

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

HttpParser::HttpParser(size_t buffer_size)
    :m_buffer_size(buffer_size)
    ,m_check_state(CHECK_STATE::CHECK_STATE_REQUESTLINE){
    m_request_body = "";
    m_uri = "";
    m_keep_alive = false;
    m_host = "";

    m_method = NONE;

}

HttpParser::HTTP_CODE HttpParser::process_read(HttpBuffer::ptr read_buffer){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    
    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || 
           (line_status = parse_line(read_buffer)) == LINE_OK){
        std::shared_ptr<char[]> data = nullptr;
        if(m_check_state == CHECK_STATE_CONTENT){
            data = read_buffer->read_by_zero();
        } else {
            data = read_buffer->read_by_end(m_checked_idx);
        }
        char* text = data.get();
        
        std::string line(text);
        SYLAR_LOG_INFO(g_logger) << "cur line = " << line;
        
        m_start_line = m_checked_idx;
        switch (m_check_state){
        // 解析请求行
        case CHECK_STATE_REQUESTLINE:
        {   
            ret = parse_request_line(text);
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            }
            break;
        }

        // 解析请求头
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            } else if(ret == GET_REQUEST){
                return GET_REQUEST;
            }
            break;
        }
        // 解析请求体
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if(ret == GET_REQUEST){
                return GET_REQUEST;
            }
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

HttpParser::HTTP_CODE HttpParser::parse_request_line(char* text){
    char* cur = text;
    char* uri = strpbrk(cur, " \t");
    if(!uri){
        return BAD_REQUEST;
    }

    uri += strspn(uri, " \t");

    if(!strncmp(cur, "POST", 4)){
        m_method = POST;
        cur += 4;
    } else if(!strncmp(cur, "GET", 3)){
        m_method = GET;
        cur += 3;
    } else if(!strncmp(cur, "OPTIONS", 7)){
        m_method = OPTIONS;
        cur += 7;
    } else {
        return BAD_REQUEST;
    }

    // 解析http_version
    char* http_version = strpbrk(uri, " \t");
    if(!http_version){
        return BAD_REQUEST;
    }

    m_uri = std::string(uri, http_version - uri);

    http_version += strspn(http_version, " \t");
    if(strncmp(http_version, "HTTP/1.1", 8)){
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HttpParser::HTTP_CODE HttpParser::parse_headers(char* text){
    char* cur = text;
    // 读到空行
    if(cur[0] == '\0'){
        if(m_content_length != 0){
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    // 解析Connection，判断是keep-alive还是close
    else if(!strncmp(cur, "Connection:", 11)){
        cur += 11;
        cur += strspn(cur, " \t");
        m_keep_alive = strcmp(cur, "Close");

        return NO_REQUEST;     
    }
    // 解析Content-length
    else if(!strncmp(cur, "Content-Length:", 15)){
        cur += 15;
        cur += strspn(cur, " \t");
        
        char* end;
        m_content_length = static_cast<int>(strtol(cur, &end, 10));
        if(*end != '\0') m_content_length = 0;

        return NO_REQUEST;
    }
    // 解析Host字段，获取请求主机名
    else if(!strncmp(cur, "Host:", 5)){
        cur += 5;
        cur += strspn(cur, " \t");

        m_host = std::string(cur);
        return NO_REQUEST;
    }
    // 解析Content-type
    else if(!strncmp(cur, "Content-type:", 13)){
        cur += 13;
        cur += strspn(cur, " \t");

        if(strncmp(cur, "application/json", 16)){
            return BAD_REQUEST;
        }
        return NO_REQUEST;
    } else {

    }

    return NO_REQUEST;
}

HttpParser::HTTP_CODE HttpParser::parse_content(char* text){
    if(m_read_idx >= (m_content_length + m_checked_idx)){
        text[m_content_length] = '\0';
        m_request_body = std::string(text);
        return GET_REQUEST;
    }

    return NO_REQUEST;
}

HttpParser::LINE_STATUS HttpParser::parse_line(HttpBuffer::ptr read_buffer){
    char temp;
    int end_idx = read_buffer->get_end_idx();
    for(;m_checked_idx != end_idx; m_checked_idx = read_buffer->next_idx(m_checked_idx)){
        temp = read_buffer->get_val(m_checked_idx);

        // 1. 读到一个完整行的倒数第二个字符
        if(temp == '\r'){
            int next_idx = read_buffer->next_idx(m_checked_idx);
            if(next_idx == end_idx){
                return LINE_OPEN;
            }

            // 读到了完整行
            if(read_buffer->get_val(next_idx) == '\n'){
                read_buffer->set_val('\0', m_checked_idx);
                m_checked_idx = next_idx;
                read_buffer->set_val('\0', m_checked_idx);
                m_checked_idx = read_buffer->next_idx(m_checked_idx);
                return LINE_OK;
            }

            return LINE_BAD;
        }

        // 读到一个完整行的最后一个字符
        else if(temp == '\n'){
            int start_idx = read_buffer->get_start_idx(), pre_idx = read_buffer->pre_idx(m_checked_idx);

            if(m_checked_idx != start_idx && read_buffer->get_val(pre_idx) == '\r'){
                read_buffer->set_val('\0', pre_idx);
                read_buffer->set_val('\0', m_checked_idx);
                m_checked_idx = read_buffer->next_idx(m_checked_idx);
                return LINE_OK;
            }

            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

std::string HttpParser::get_method(){
    if(m_method == POST){
        return "POST";
    } else if(m_method == GET){
        return "GET";
    } else if(m_method == OPTIONS){
        return  "OPTIONS";
    }
    return "";
}

std::string HttpParser::get_uri(){
    return m_uri;
}

std::string HttpParser::get_body(){
    return m_request_body;
}

}
