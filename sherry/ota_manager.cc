#include "sherry.h"
#include "ota_manager.h"
#include "iomanager.h"
#include "../include/json/json.hpp"
#include "util.h"
#include "http/http_server.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

OTAManager::OTAManager(size_t buffer_size, const std::string& protocol, const std::string& host, int port, float http_version, const std::string& file_prev_path)
    :m_protocol(protocol)
    ,m_host(host)
    ,m_port(port)
    ,m_file_prev_path(file_prev_path)
    ,m_buffer_size(buffer_size)
    ,m_http_server(nullptr)
    ,m_ota_http_res_builder(std::make_shared<OTAHttpResBuilder>(http_version)){
    m_timer_mgr = std::make_shared<IOManager>(3, false, "OTA-Timer");
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
        
        SYLAR_LOG_DEBUG(g_logger) << "其他功能写完记得补充";
    }

    SYLAR_LOG_WARN(g_logger) << "device type = " << device_type
                                 << ", device no = " << device_no
                                 << " has been removed successfully.";
    return true;
}

void OTAManager::ota_notify(uint16_t device_type, struct OTAMessage& msg, int connect_id){
    const std::string type = "notify";
    {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_device_type_nums.find(device_type);
        if(it == m_device_type_nums.end()){
            std::stringstream ss;
            ss << "device type = " << device_type
               << " has not been registed.";
            std::string sstr = ss.str();
            SYLAR_LOG_WARN(g_logger) << sstr;

            nlohmann::json j;
            j["msg"] = std::move(sstr);
            response_to_server(connect_id, false, type, j, true);
            return;
        }

        if((*it).second.size() == 0){
            std::stringstream ss;
            ss << "device type = " << device_type
               << " device nums = 0.";
            std::string sstr = ss.str();
            SYLAR_LOG_WARN(g_logger) << sstr;

            nlohmann::json j;
            j["msg"] = std::move(sstr);
            response_to_server(connect_id, false, type, j, true);
            return;
        }

    }

    nlohmann::json j;
    j["msg"] = "notify task submitted.";
    response_to_server(connect_id, true, type, j, true);

    std::stringstream ss;
    ss << "/ota/" << device_type
       << "/" << msg.name 
       << "/" << msg.version
       << "/notify";

    std::string topic = std::move(ss.str());

    {
        RWMutexType::ReadLock lock(m_notifier_mutex);
        
        auto it = m_ota_notifier_map.find(topic);
        if(it != m_ota_notifier_map.end()){
            OTANotifier::ptr notifier = (*it).second;
            notifier->set_message(msg);
            SYLAR_LOG_INFO(g_logger) << "device type = " << device_type
                                     << " start to notify.";
            notifier->start();
            return;
        }
    }

    RWMutexType::WriteLock lock(m_notifier_mutex);
    OTANotifier::ptr notifier = std::make_shared<OTANotifier>(device_type, m_timer_mgr, topic, m_client_mgr, 1000);
    m_ota_notifier_map[topic] = notifier;
    SYLAR_LOG_INFO(g_logger) << "device type = " << device_type
                                     << " start to notify.";
    notifier->set_message(msg);
    notifier->start();
}

void OTAManager::ota_stop_notify(uint16_t device_type, const std::string& name, const std::string& version, int connect_id){
    const std::string type = "stop_notify";

    std::stringstream ss;
    ss << "/ota/" << device_type
       << "/" << name 
       << "/" << version
       << "/notify";

    std::string topic = std::move(ss.str());

    RWMutexType::ReadLock lock(m_notifier_mutex);
    auto it = m_ota_notifier_map.find(topic);
    if(it == m_ota_notifier_map.end()){
        std::stringstream ss;
        ss << "device_type = " << device_type
           << " notifier has not existed.";
        std::string sstr = ss.str(); 
        SYLAR_LOG_WARN(g_logger) << sstr;
        nlohmann::json j;
        j["msg"] = std::move(sstr);
        response_to_server(connect_id, false, type, j, true);
        return;
    }

    (*it).second->stop();
    ss.clear();
    ss << "device_type = " << device_type
       << ", name = " << name
       << ", version = " << version
       << " notifier has been stopped.";
    std::string sstr = std::move(ss.str()); 
    SYLAR_LOG_INFO(g_logger) << sstr;
    nlohmann::json j;
    j["msg"] = std::move(sstr);
    response_to_server(connect_id, true, type, j, true);

}

void OTAManager::ota_query(uint16_t device_type, uint32_t device_no, const std::string& action, int connect_id){
    const std::string type = "query";
    std::stringstream pub_stream, sub_stream;
    pub_stream = FormatOtaPrex(device_type, device_no);
    sub_stream = FormatOtaPrex(device_type, device_no);

    pub_stream << "/query";
    sub_stream << "/responder";

    std::string pub_topic = std::move(pub_stream.str());
    std::string sub_topic = std::move(sub_stream.str());

    OTAQueryResponder::ptr oqr = std::make_shared<OTAQueryResponder>(device_type, device_no, m_client_mgr);
    m_callback_mgr->regist_callback(sub_topic
                                    ,[this, oqr, pub_topic, device_type, device_no, action, connect_id, type]
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
            this->response_to_server(connect_id, true, type, json_response, true);
            

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

void OTAManager::ota_query_download(uint16_t device_type, uint32_t device_no, const std::string& name, int connect_id){
    const std::string type = "query_download";
    if(!check_device(device_type, device_no)){
        std::stringstream ss;
        ss << "device type = " << device_type
            << " has not been registed.";
        std::string sstr = ss.str();
        SYLAR_LOG_WARN(g_logger) << sstr;

        nlohmann::json j;
        j["msg"] = std::move(sstr);
        response_to_server(connect_id, false, type, j, true);

        return;
    }
    std::stringstream ss;
    ss << "/ota/" << device_type
            << "/" << device_no 
            << "/" << name
            << "/query_download";

    std::string topic = ss.str();
    m_callback_mgr->regist_callback(topic, [this, device_type, device_no, name, connect_id, type]
                                    (const std::string& topic, const std::string& payload){

        try{
            SYLAR_LOG_DEBUG(g_logger) << payload;

            nlohmann::json json_response = nlohmann::json::parse(payload);
            if(!json_response.contains("name") 
                      || !json_response.contains("device_type")
                      || !json_response.contains("device_no")){
                SYLAR_LOG_WARN(g_logger) << "Query device:" << device_type
                                         << "/" << device_no 
                                         << " details is not satisfied.";
                return;
            }
            
            if(json_response["name"] != name){
                SYLAR_LOG_WARN(g_logger) << "Query device:" << device_type
                                          << "/" << device_no
                                          << " response name: " << json_response["name"]
                                          << " is not same with query action: " << name;
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
            this->response_to_server(connect_id, true, type, json_response, true);
            

        } catch(const std::exception& e){
            SYLAR_LOG_WARN(g_logger) << "Query device:" << device_type
                                         << "/" << device_no 
                                         << "error, error is " << e.what();
            return;
        }
        
    });

    {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_ota_subscribe_download_map.find(device_type);
        if(it != m_ota_subscribe_download_map.end()){
            auto itt = (*it).second.find(device_no);
            if(itt != (*it).second.end()){
                (*itt).second->subscribe_download(topic);
                SYLAR_LOG_INFO(g_logger) << "device_type = " << device_type
                                         << ", device_no = " << device_no
                                         << ", name = " << name
                                         << " start to subscribe download detail.";
                return;
            }
        }
    }

    OTASubscribeDownload::ptr ota_sd = nullptr;
    {
        RWMutexType::WriteLock lock(m_mutex);
        ota_sd = std::make_shared<OTASubscribeDownload>(topic, device_type, device_no, m_client_mgr, m_callback_mgr);
        m_ota_subscribe_download_map[device_type][device_no] = ota_sd;
    }
    ota_sd->subscribe_download(topic);
    SYLAR_LOG_INFO(g_logger) << "device_type = " << device_type
                             << ", device_no = " << device_no
                             << ", name = " << name
                             << " start to subscribe download detail.";

}

void OTAManager::ota_file_download(uint16_t device_type, const std::string& name, const std::string& version, int connect_id){
    const std::string type = "file_download";
    if(!check_device(device_type)){
        std::stringstream ss;
        ss << "device type = " << device_type
            << " has not been registed.";
        std::string sstr = ss.str();
        SYLAR_LOG_WARN(g_logger) << sstr;

        nlohmann::json j;
        j["msg"] = std::move(sstr);
        response_to_server(connect_id, false, type, j, true);

        return;
    }

    if(!send_file(device_type, name, version, connect_id)){
        SYLAR_LOG_WARN(g_logger) << "fd = " << connect_id
                                 << "get_file failed";
        return;
    }

}

bool OTAManager::send_file(uint16_t device_type, const std::string& name, const std::string& version, int connect_id){
    std::string file_path = m_file_prev_path + "ota_" 
                            + std::to_string(device_type) 
                            + "_" + version 
                            + "_" + name + ".jpg";
    int file_fd = open(file_path.c_str(), O_RDONLY);
    if(file_fd < 0){
        return false;
    }

    struct stat st;
    if (fstat(file_fd, &st) != 0) {
        close(file_fd);
        return false;
    }

    const std::string type = "file_download";
    nlohmann::json j;
    j["file_size"] = st.st_size;
    SYLAR_LOG_DEBUG(g_logger) << "filesize = " << st.st_size;
    response_to_server(connect_id, true, type, j, true);

    char buffer[m_buffer_size];
    ssize_t read_bytes = 0;
    while((read_bytes = read(file_fd, buffer, m_buffer_size))){
        response_to_server(connect_id, true, type, buffer, read_bytes);    
    }
    close(file_fd);

    return true;

}

void OTAManager::submit(uint16_t device_type, uint32_t device_no, const std::string& command, int client_id, std::unordered_map<std::string, std::string>& detail){
    if (command == "OPTIONS"){
        nlohmann::json j;
        response_to_server(client_id, true, command, j, true);
    }else if(command == "notify"){
            // this->ota_notify(device_type, )
        SYLAR_LOG_INFO(g_logger) << "device_type = " << device_type
                                 << ", device_no = " << device_no
                                 << ", command = " << command
                                 << ", client_id = " << client_id;
        OTAMessage msg;
        std::string& version = detail["version"];
        std::string& name = detail["name"];
        if(!get_notify_message(device_type, name, version, msg)){
            std::stringstream ss;
            ss << "device_type = " << device_type
                                   << ", command = " << command
                                   << ", client_id = " << client_id
                                   << ", name = " << name
                                   << ", version = " << version
                                   << " get notify message error";
            std::string sstr = ss.str();
            SYLAR_LOG_WARN(g_logger) << sstr;
            nlohmann::json j;
            j["msg"] = std::move(sstr);
            response_to_server(client_id, false, command, j, true);

            return;
        }
        ota_notify(device_type, msg, client_id);
    } else if(command == "query"){
        SYLAR_LOG_INFO(g_logger) << "device_type = " << device_type
                                 << ", device_no = " << device_no
                                 << ", command = " << command
                                 << ", client_id = " << client_id;
        std::string& action = detail["action"];
        ota_query(device_type, device_no, action, client_id);
    } else if(command == "stop_notify"){
        SYLAR_LOG_INFO(g_logger) << "device_type = " << device_type
                                 << ", device_no = " << device_no
                                 << ", command = " << command
                                 << ", client_id = " << client_id;
        std::string& name = detail["name"];
        std::string& version = detail["version"];
        ota_stop_notify(device_type, name, version, client_id);
    } else if(command == "query_download"){
        std::string& name = detail["name"];
        ota_query_download(device_type, device_no, name, client_id);
    } else if(command == "file_download"){
        ota_file_download(device_type, detail["name"], detail["version"], client_id);
        
    } else {
        nlohmann::json j;
        j["msg"] = "command is error.";
        response_to_server(client_id, false, command, j, true);
    }
}

bool OTAManager::get_notify_message(uint16_t device_type, const std::string& name, const std::string& version, struct OTAMessage& msg){
    
    msg.name = name;
    msg.version = version;
    msg.time = getCurrentTimeString();
    msg.file_name = "agsspds_20241110.zip";
    msg.file_size = 6773120;
    msg.url_path = "http://127.0.0.1:18882/download/ota/agsspds";
    msg.md5_value = "ed076287532e86365e841e92bfc50d8c";
    msg.launch_mode = 0;
    msg.upgrade_mode = 1;

    return true;

}

void OTAManager::response_to_server(int fd, bool success, const std::string& type, nlohmann::json& j, bool need_header){
    std::string ret = m_ota_http_res_builder->build_http_response(type, success, j);
    m_http_server->response(fd, ret);       
}

void OTAManager::response_to_server(int fd, bool success, const std::string& type, char* buffer, size_t buffer_size){
    m_http_server->response(fd, buffer, buffer_size);
}

bool OTAManager::check_device(uint16_t device_type, uint32_t device_no){
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_device_type_nums.find(device_type);
    if(it == m_device_type_nums.end()){
        return false;
    }

    auto itt = (*it).second.find(device_no);
    if(itt == (*it).second.end()){
        return false;
    }

    return true;
}

bool OTAManager::check_device(uint16_t device_type){
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_device_type_nums.find(device_type);
    if(it == m_device_type_nums.end()){
        return false;
    }

    return true;
}

}