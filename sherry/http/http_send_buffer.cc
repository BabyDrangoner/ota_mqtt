#include "http_send_buffer.h"
#include "../log.h"

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

void HttpSocketBuffer::addData(std::shared_ptr<OTAData> data){
    MutexType::Lock lock(m_mutex);
    m_buffer.emplace_back(data);
}

std::vector<std::shared_ptr<OTAData>> HttpSocketBuffer::getData(){
    MutexType::Lock lock(m_mutex);
    return std::move(m_buffer);
}

void HttpSendBuffer::addData(uint32_t key, std::shared_ptr<OTAData> data){
    MutexType::Lock lock(m_mutex);
    auto it = m_mp.find(key);
    if(it == m_mp.end()) {
        m_mp[key] = std::make_shared<HttpSocketBuffer>(); 
    }
    m_mp[key]->addData(data);

}

std::vector<std::shared_ptr<OTAData>> HttpSendBuffer::getData(uint32_t key){
    MutexType::Lock lock(m_mutex);
    auto it = m_mp.find(key);
    if(it == m_mp.end()) return {};
    return (*it).second->getData();
    
}


}