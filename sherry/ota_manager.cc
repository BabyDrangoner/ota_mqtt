#include "sherry.h"
#include "ota_manager.h"
#include "iomanager.h"

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

OTAManager::OTAManager(const std::string& protocol, const std::string& host, int port)
    :m_protocol(protocol)
    ,m_host(host)
    ,m_port(port){
    m_timer_mgr = std::make_shared<IOManager>(1, false, "OTA-Timer");
    m_device_types_counts = 0;
    m_device_counts = 0;
    m_callback_mgr = std::make_shared<OTAClientCallbackManager>();
    m_client_mgr = std::make_shared<MqttClientManager>(m_port, m_protocol, m_host, m_callback_mgr);

    m_device_type_nums.clear();
    m_ota_notifier_map.clear();
}

bool OTAManager::add_device(uint16_t device_type, uint32_t device_no){
    RWMutexType::WriteLock lock(m_mutex);
    auto it = m_device_type_nums.find(device_type);
    if(it == m_device_type_nums.end()){
        ++m_device_types_counts;
        ++m_device_counts;
        m_device_type_nums[device_type].insert(device_no);
        SYLAR_LOG_INFO(g_logger) << "device type = " << device_type
                                 << ", device no = " << device_no
                                 << " success add.";
        return true;
    } 

    auto itt = (*it).second.find(device_no);
    if(itt == (*it).second.end()){
        ++m_device_counts;
        (*it).second.insert(device_no);
        SYLAR_LOG_INFO(g_logger) << "device type = " << device_type
                                 << ", device no = " << device_no
                                 << " has been added successfully.";
        return true;
    }

    SYLAR_LOG_WARN(g_logger) << "device type = " << device_type
                                 << ", device no = " << device_no
                                 << " has been already added.";
    return false;
}

bool OTAManager::remove_device(uint16_t device_type, uint32_t device_no){
    {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_device_type_nums.find(device_type);
        if(it == m_device_type_nums.end()){
            SYLAR_LOG_WARN(g_logger) << "device type = " << device_type
                                    << ", device no = " << device_no
                                    << " has not been added.";
            return false;
        } 

        auto itt = (*it).second.find(device_no);
        if(itt == (*it).second.end()){
            ++m_device_counts;
            (*it).second.insert(device_no);
            SYLAR_LOG_INFO(g_logger) << "device type = " << device_type
                                    << ", device no = " << device_no
                                    << " has not been added.";
            return false;
        }
    }

    RWMutexType::WriteLock lock(m_mutex);
    m_device_type_nums[device_type].erase(device_no);
    --m_device_counts;
    if(m_device_type_nums[device_type].size() == 0){
        m_device_type_nums.erase(device_type);
        --m_device_types_counts;
        
        m_ota_notifier_map.erase(device_type);
        SYLAR_LOG_DEBUG(g_logger) << "其他功能写完记得补充";
    }

    SYLAR_LOG_WARN(g_logger) << "device type = " << device_type
                                 << ", device no = " << device_no
                                 << " has been removed successfully.";
    return true;
}

void OTAManager::ota_notify(uint16_t device_type, struct OTAMessage& msg){
    {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_device_type_nums.find(device_type);
        if(it == m_device_type_nums.end()){
            SYLAR_LOG_WARN(g_logger) << "device type = " << device_type
                                     << " has not been registed.";
            return;
        }

        if((*it).second.size() == 0){
            SYLAR_LOG_WARN(g_logger) << "device type = " << device_type
                                     << " device nums = 0.";
            return;
        }

    }

    {
        RWMutexType::ReadLock lock(m_notifier_mutex);
        
        auto it = m_ota_notifier_map.find(device_type);
        if(it != m_ota_notifier_map.end()){
            OTANotifier::ptr notifier = (*it).second;
            notifier->set_message(msg);
            SYLAR_LOG_INFO(g_logger) << "device type = " << device_type
                                     << " start to notify.";
            notifier->start();
            return;
        }
    }

    std::stringstream topic;
    topic << "ota/" << device_type << "/notifier";

    RWMutexType::WriteLock lock(m_notifier_mutex);
    OTANotifier::ptr notifier = std::make_shared<OTANotifier>(device_type, m_timer_mgr, topic.str(), m_client_mgr, 1000);
    m_ota_notifier_map[device_type] = notifier;
    SYLAR_LOG_INFO(g_logger) << "device type = " << device_type
                                     << " start to notify.";
    notifier->set_message(msg);
    notifier->start();

}

void OTAManager::ota_stop_notify(uint16_t device_type){
    RWMutexType::ReadLock lock(m_notifier_mutex);
    auto it = m_ota_notifier_map.find(device_type);
    if(it == m_ota_notifier_map.end()){
        SYLAR_LOG_WARN(g_logger) << "device_type = " << device_type
                                 << " notifier has not existed.";
        return;
    }

    (*it).second->stop();
    SYLAR_LOG_INFO(g_logger) << "device_type = " << device_type
                                 << " notifier has been stopped.";
}

}