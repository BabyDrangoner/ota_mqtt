#include "ota_http_command_dispatcher.h"
#include "log.h"

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

template<typename T>
static bool get_element(T& element, const std::string& key, const nlohmann::json& j){
    if(!j.contains(key)){
        return false;
    }

    if(key == "device_type"){
        if (j["device_type"].is_number_integer()) {
            element = j.at("device_type").get<uint16_t>();
        } else if (j["device_type"].is_string()) {
            element = static_cast<uint16_t>(std::stoul(j.at("device_type").get<std::string>()));
        } else {
            return false;
        }
        return true;
    } else if(key == "device_no") {
        if(j["device_no"].is_number_integer()){
            element = j.at("device_no").get<uint32_t>();
        } else if(j["device_no"].is_string()){
            element = static_cast<uint32_t>(std::stoul(j.at("device_no").get<std::string>()));
        } else {
            return false;
        }

        return true;
    }

    element = j[key];
    return true;
}


OTARequest::OTARequest(OTAManager::ptr ota_manager)
    :m_ota_mgr(ota_manager){

}

OTANotifyReq::OTANotifyReq(OTAManager::ptr ota_manager)
    :OTARequest(ota_manager){

}

bool OTANotifyReq::submit(const nlohmann::json& j, int connection_id){

    uint16_t device_type = -1;
    if(!get_element(device_type, "device_type", j)){
        return false;
    }

    std::string version;
    if(!get_element(version, "version", j)){
        return false;
    }

    std::string name;
    if(!get_element(name, "name", j)){
        return false;
    }

    std::unordered_map<std::string, std::string> detail;
    detail["version"] = std::move(version);
    detail["name"] = std::move(name);

    m_ota_mgr->submit(device_type, -1, "notify", connection_id, detail);    
    return true;
}

OTAQueryReq::OTAQueryReq(OTAManager::ptr ota_manager)
    :OTARequest(ota_manager){
}

bool OTAQueryReq::submit(const nlohmann::json& j, int connection_id){

    uint16_t device_type = -1;
    if(!get_element(device_type, "device_type", j)){
        return false;
    }

    uint32_t device_no = -1;
    if(!get_element(device_no, "device_no", j)){
        return false;
    }

    std::string action;
    if(!get_element(action, "action", j)){
        return false;
    }

    std::unordered_map<std::string, std::string> detail;
    detail["action"] = action;

    m_ota_mgr->submit(device_type, device_no, "query", connection_id, detail);
    return true;
}

OTAStopNotifyReq::OTAStopNotifyReq(OTAManager::ptr ota_manager)
    :OTARequest(ota_manager){

}

bool OTAStopNotifyReq::submit(const nlohmann::json& j, int connection_id){
    uint16_t device_type = -1;
    if(!get_element(device_type, "device_type", j)){
        return false;
    }

    std::string version;
    if(!get_element(version, "version", j)){
        return false;
    }

    std::string name;
    if(!get_element(name, "name", j)){
        return false;
    }

    std::unordered_map<std::string, std::string> detail;
    detail["version"] = std::move(version);
    detail["name"] = std::move(name);

    m_ota_mgr->submit(device_type, -1, "stop_notify", connection_id, detail); 
    return true;
}

OTAQueryDownloadReq::OTAQueryDownloadReq(OTAManager::ptr ota_manager)
    :OTARequest(ota_manager){
}

bool OTAQueryDownloadReq::submit(const nlohmann::json& j, int connection_id){
    uint16_t device_type = -1;
    if(!get_element(device_type, "device_type", j)){
        return false;
    }

    uint32_t device_no = -1;
    if(!get_element(device_no, "device_no", j)){
        return false;
    }

    std::string name;
    if(!get_element(name, "name", j)){
        return false;
    }

    std::unordered_map<std::string, std::string> detail;
    detail["name"] = std::move(name);
    m_ota_mgr->submit(device_type, device_no, "query_download", connection_id, detail); 
    return true;
}


OTAFileDownloadReq::OTAFileDownloadReq(OTAManager::ptr ota_manager)
    :OTARequest(ota_manager){

}

bool OTAFileDownloadReq::submit(const nlohmann::json& j, int connection_id){
    uint16_t device_type = -1;
    if(!get_element(device_type, "device_type", j)){
        return false;
    }

    std::string version;
    if(!get_element(version, "version", j)){
        return false;
    }

    std::string name;
    if(!get_element(name, "name", j)){
        return false;
    }

    std::unordered_map<std::string, std::string> detail;
    detail["version"] = std::move(version);
    detail["name"] = std::move(name);
    m_ota_mgr->submit(device_type, -1, "file_download", connection_id, detail); 
    return true;
}

OTAHttpCommandDispatcher::OTAHttpCommandDispatcher(const std::string& http_version, const std::string& content_type, OTAManager::ptr ota_manager)
    :m_http_version(http_version)
    ,m_content_type(content_type){
    m_command_dispatchers["notify"] = std::make_shared<OTANotifyReq>(ota_manager);
    m_command_dispatchers["query"] = std::make_shared<OTAQueryReq>(ota_manager);
    m_command_dispatchers["stop_notify"] = std::make_shared<OTAStopNotifyReq>(ota_manager);
    m_command_dispatchers["query_download"] = std::make_shared<OTAQueryDownloadReq>(ota_manager);
    m_command_dispatchers["file_download"] = std::make_shared<OTAFileDownloadReq>(ota_manager);

}

bool OTAHttpCommandDispatcher::handle_request(std::string& request, int connect_id) {

    std::istringstream ss(request);
    std::string cmd, uri, protocol;
    ss >> cmd >> uri >> protocol;
    
    if(!check_protocol(protocol)){
        return false;
    }

    // 拆第一，第二行
    std::string tmp;
    std::getline(ss, tmp, '\n');
    std::getline(ss, tmp, '\n');

    if(cmd == "POST"){
        std::string res;
        if(!check_uri(uri, res)){
            return false;
        }

        std::getline(ss, tmp, ' ');
        if(tmp != "Content-Type:"){
            return false;
        }

        std::getline(ss, tmp, '\r');
        if(tmp != m_content_type){
            return false;
        }

        size_t pos = request.find("\r\n\r\n");
        if (pos == std::string::npos) {
            return false; // 不合法请求
        }

        std::string body = request.substr(pos + 4); // 跳过 \r\n\r\n

        nlohmann::json j = nlohmann::json::parse(body);
        
        auto it = m_command_dispatchers.find(res);
        if(it == m_command_dispatchers.end()){
            return false;
        }
        return m_command_dispatchers[res]->submit(j, connect_id);

    } else if(cmd == "GET"){
        std::string res;
        std::string name;
        std::string version;
        uint16_t device_type;
        
        if(!check_uri(uri, device_type, name, version, res)){
            return false;
        }

        nlohmann::json j;
        j["device_type"] = device_type;
        j["name"] = name;
        j["version"] = version;

        auto it = m_command_dispatchers.find(res);
        if(it == m_command_dispatchers.end()){
            return false;
        }
        return m_command_dispatchers[res]->submit(j, connect_id);
    }
    
    return false;
    
}

bool OTAHttpCommandDispatcher::check_uri(const std::string& uri, uint16_t& device_type, std::string& name, std::string& version, std::string& res){
    if(uri.find("/download/ota/", 0) != 0){
        return false;
    }

    try{
        std::istringstream uri_ss(uri);
        std::string tmp;
        std::getline(uri_ss, tmp, '/');   // 空
        std::getline(uri_ss, tmp, '/');   // "download"等
        std::getline(uri_ss, tmp, '/');   // "ota"

        std::getline(uri_ss, tmp, '/'); // device_type
        device_type = static_cast<uint16_t>(std::stoul(tmp));

        std::getline(uri_ss, name, '/');

        std::getline(uri_ss, version, '/'); // name

        uri_ss >> res;  // get方法

        return true;
    } catch(...){
        return false;
    }

}

bool OTAHttpCommandDispatcher::check_uri(const std::string& uri, std::string& res){
    if(uri.find("/api/ota/", 0) != 0){
        return false;
    }

    try{
        std::istringstream uri_ss(uri);
        std::string tmp;
        std::getline(uri_ss, tmp, '/');   // 空
        std::getline(uri_ss, tmp, '/');   // "api"
        std::getline(uri_ss, tmp, '/');   // "ota"

        uri_ss >> res;  // post方法

        return true;
    } catch(...){
        return false;
    }

}

bool OTAHttpCommandDispatcher::check_protocol(const std::string& protocol){
    if(protocol.find("HTTP/", 0) != 0){
        return false;
    }

    if(protocol.substr(5, 3) != m_http_version){
        return false;
    }

    return true;
}

// void OTAHttpCommandDispatcher::handle_command(const std::string& uri, int connection_id){
//     if(uri.rfind("/download/ota/", 0) == 0){
//         try{
//             std::istringstream uri_ss(uri);
//             std::string tmp;
//             std::getline(uri_ss, tmp, '/'); // 跳过空
//             std::getline(uri_ss, tmp, '/'); // "download"
//             std::getline(uri_ss, tmp, '/'); // "ota"

//             std::string device_type, device_no, name;
//             std::getline(uri_ss, device_type, '/');
//             std::getline(uri_ss, device_no, '/');
//             std::getline(uri_ss, name, '/');

//         }catch(...){
//             return;
//         }
//     }
// }


}