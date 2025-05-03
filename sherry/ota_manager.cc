#include "sherry.h"
#include "ota_manager.h"
#include "iomanager.h"
#include "../include/json/json.hpp"
#include "util.h"
#include "./http/http_send_buffer.h"

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

OTAManager::OTAManager(const std::string& protocol, const std::string& host, int port, std::shared_ptr<HttpSendBuffer> buffer)
    :m_protocol(protocol)
    ,m_host(host)
    ,m_port(port),
    m_http_send_buffer(buffer){
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
    topic << "/ota/" << device_type << "/notify";

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

void OTAManager::ota_query(uint16_t device_type, uint32_t device_no, const std::string& action, int connect_id){
    std::stringstream pub_stream, sub_stream;
    pub_stream = FormatOtaPrex(device_type, device_no);
    sub_stream = FormatOtaPrex(device_type, device_no);

    pub_stream << "/query/";
    sub_stream << "/responder/";

    std::string pub_topic = std::move(pub_stream.str());
    std::string sub_topic = std::move(sub_stream.str());

    OTAQueryResponder::ptr oqr = std::make_shared<OTAQueryResponder>(device_type, device_no, m_client_mgr);
    m_callback_mgr->regist_callback(sub_topic
                                    ,[this, oqr, pub_topic, device_type, device_no, action, connect_id]
                                    (const std::string& topic, const std::string& payload){

        oqr->subscribe_on_success(pub_topic, topic);
        try{
            SYLAR_LOG_DEBUG(g_logger) << payload;

            nlohmann::json json_response = nlohmann::json::parse(payload);
            if(!json_response.contains("query_details") 
                      || !json_response.contains("device_type")
                      || !json_response.contains("device_no")){
                SYLAR_LOG_WARN(g_logger) << "Query device:" << device_type
                                         << "/" << device_no 
                                         << " details is not satisfied.";
                return;
            }
            
            if(!json_response["query_details"].contains("action") || json_response["query_details"]["action"] != action){
                SYLAR_LOG_WARN(g_logger) << "Query device:" << device_type
                                          << "/" << device_no
                                          << " response action: " << json_response["action"]
                                          << " is not same with query action: " << action;
                return;
            } 
            if(json_response["device_type"] != device_type){
                SYLAR_LOG_WARN(g_logger) << "Query device:" << device_type
                                          << "/" << device_no
                                          << " response device_type: " << json_response["device_type"]
                                          << " is not same with query device_type: " << device_type;
                return;
            } 
            if(json_response["device_no"] != device_no){
                SYLAR_LOG_WARN(g_logger) << "Query device:" << device_type
                                          << "/" << device_no
                                          << " response device_no: " << json_response["device_no"]
                                          << " is not same with query device_no: " << device_no;
                return;
            }
            m_http_send_buffer->addData(connect_id, payload);

        } catch(const std::exception& e){
            SYLAR_LOG_WARN(g_logger) << "Query device:" << device_type
                                         << "/" << device_no 
                                         << "error, error is " << e.what();
            return;
        }
        

    });
    oqr->publish_query(action, 1, true);
    oqr->subscribe_responder(pub_topic, sub_topic, 1);

}

void OTAManager::submit(uint16_t device_type, uint32_t device_no, const std::string& command, int client_id, const std::string& detail){
    if (command == "XXX"){
        SYLAR_LOG_INFO(g_logger) << "device_type = " << device_type
                                 << ", device_no = " << device_no
                                 << ", command = " << command
                                 << ", client_id = " << client_id;
    }else if(command == "notify"){
            // this->ota_notify(device_type, )
        SYLAR_LOG_INFO(g_logger) << "device_type = " << device_type
                                 << ", device_no = " << device_no
                                 << ", command = " << command
                                 << ", client_id = " << client_id;
        OTAMessage msg;
        if(!get_notify_message(device_type, detail, msg)){
            SYLAR_LOG_WARN(g_logger) << "device_type = " << device_type
                                     << ", device_no = " << device_no
                                     << ", command = " << command
                                     << ", client_id = " << client_id
                                     << ", has no version = " << detail;
            return;
        }
        ota_notify(device_type, msg);
    } else if(command == "query"){
        ota_query(device_type, device_no, detail, client_id);
    }
}

bool OTAManager::get_notify_message(uint16_t device_type, const std::string& version, struct OTAMessage& msg){
    
    msg.name = std::to_string(device_type);
    msg.version = std::move(version);
    msg.time = getCurrentTimeString();
    msg.file_name = "agsspds_20241110.zip";
    msg.file_size = 6773120;
    msg.url_path = "http://127.0.0.1:18882/download/ota/agsspds";
    msg.md5_value = "ed076287532e86365e841e92bfc50d8c";
    msg.launch_mode = 0;
    msg.upgrade_mode = 1;

    return true;

}


}