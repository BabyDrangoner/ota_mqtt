#include "http_send_buffer.h"
#include "../log.h"

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

void HttpSocketBuffer::addData(const std::string& data){
    MutexType::Lock lock(m_mutex);
    m_data.mergeFromJson(data);
}

std::string HttpSocketBuffer::getData(){
    MutexType::Lock lock(m_mutex);
    std::string ret = m_data.to_string();
    m_data = OTAData();
    return ret;
}

void HttpSendBuffer::addData(int key, const std::string data){
    MutexType::Lock lock(m_mutex);
    auto it = m_mp.find(key);
    if(it == m_mp.end()) {
        m_mp[key] = std::make_shared<HttpSocketBuffer>(); 
        SYLAR_LOG_DEBUG(g_logger) << "addData: " << key;

    }
    m_mp[key]->addData(data);

}

std::string HttpSendBuffer::getData(int key){
    MutexType::Lock lock(m_mutex);
    SYLAR_LOG_DEBUG(g_logger) << "getData key: " << key;
    auto it = m_mp.find(key);
    if(it == m_mp.end()) {
        SYLAR_LOG_DEBUG(g_logger) << "getData: it == m_mp.end()";
        return "";
    }
    SYLAR_LOG_DEBUG(g_logger) << "getData: it != m_mp.end()";
    return (*it).second->getData();
    
}


}