#include "ota_http_command_dispatcher.h"
#include "log.h"

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

OTAHttpCommandDispatcher::OTAHttpCommandDispatcher(OTAManager::ptr ota_manager)
    :m_ota_manager(ota_manager){
}

void OTAHttpCommandDispatcher::handle_command(const nlohmann::json& j, int connection_id) {
    
    if (!j.contains("command") || !j["command"].is_string()) {
        SYLAR_LOG_WARN(g_logger) << "Invalid command field";
        // HttpSendBuffer::GetInstance()->push_response(connection_id, "{\"error\":\"missing or invalid command\"}");
        return;
    }

    std::string cmd = j["command"];

    uint16_t device_type = 0;
    uint32_t device_no = 0;

    try {
        if (j["device_type"].is_number_integer()) {
            device_type = j.at("device_type").get<uint16_t>();
        } else if (j["device_type"].is_string()) {
            device_type = static_cast<uint16_t>(std::stoi(j.at("device_type").get<std::string>()));
        } else {
            throw std::runtime_error("device_type format invalid");
        }

        if(cmd != "notify"){
            if (j["device_no"].is_number_integer()) {
                device_no = j.at("device_no").get<uint32_t>();
            } else if (j["device_no"].is_string()) {
                device_no = static_cast<uint32_t>(std::stoul(j.at("device_no").get<std::string>()));
            } else {
                throw std::runtime_error("device_no format invalid");
            }
        }
    } catch (const std::exception& e) {
        SYLAR_LOG_ERROR(g_logger) << "Parsing device_type/device_no failed: " << e.what();
        //HttpSendBuffer::GetInstance()->push_response(connection_id, "{\"error\":\"device_type or device_no parse error\"}");
        return;
    }

    // SYLAR_LOG_DEBUG(g_logger) << "device_type = " << device_type
    //                           << "device_no = " << device_no;
    // 成功解析，提交任务
    if(cmd == "notify"){
        if(!j.contains("version")){
            SYLAR_LOG_WARN(g_logger) << "notify: device_type " << device_type
                                     << " not gives version";
            return; 
        }
        std::string version = j["version"];
        m_ota_manager->submit(device_type, device_no, cmd, connection_id, version);
        
    } else if(cmd == "query"){
        if(!j.contains("action")){
            SYLAR_LOG_WARN(g_logger) << "query: device_type " << device_type
                                     << " not gives action";
            return; 
        }
        std::string action = j["action"];
        m_ota_manager->submit(device_type, device_no, cmd, connection_id, action);
    }
}


}