#include "http_parser.h"

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

HttpParser::HttpParser(size_t buffer_size)
    :m_buffer_size(buffer_size)
    ,m_check_state(CHECK_STATE::CHECK_STATE_REQUESTLINE){
    m_request_body = nullptr;
    m_uri = nullptr;
    m_keep_alive = false;
    m_host = nullptr;

    m_method = NONE;

}

HttpParser::HTTP_CODE HttpParser::process_read(char* read_buffer){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    
    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || 
           (line_status = parse_line(read_buffer)) == LINE_OK){
        char* text = read_buffer + m_start_line;
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
    m_uri = strpbrk(cur, " \t");
    if(!m_uri){
        return BAD_REQUEST;
    }

    m_uri += strspn(m_uri, " \t");

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
    char* http_version = strpbrk(m_uri, " \t");
    if(!http_version){
        return BAD_REQUEST;
    }

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

        m_host = cur;
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
        m_request_body = text;
        return GET_REQUEST;
    }

    return NO_REQUEST;
}

HttpParser::LINE_STATUS HttpParser::parse_line(char* read_buffer){
    char temp;
    for(;m_checked_idx < m_read_idx; ++m_checked_idx){
        temp = read_buffer[m_checked_idx];

        // 1. 读到一个完整行的倒数第二个字符
        if(temp == '\r'){
            if((m_checked_idx + 1) == m_read_idx){
                return LINE_OPEN;
            }

            // 读到了完整行
            if(read_buffer[m_checked_idx + 1] == '\n'){
                read_buffer[m_checked_idx++] = '\0';
                read_buffer[m_checked_idx++] = '\0';
                return LINE_OK;
            }

            return LINE_BAD;
        }

        // 读到一个完整行的最后一个字符
        else if(temp == '\n'){
            if(m_checked_idx > 1 && read_buffer[m_checked_idx - 1] == '\r'){
                read_buffer[m_checked_idx++] = '\0';
                read_buffer[m_checked_idx++] = '\0';
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
    return std::string(m_uri);
}

std::string HttpParser::get_body(){
    if(m_request_body){
        return std::string(m_request_body);
    }
    return "";
}

}
