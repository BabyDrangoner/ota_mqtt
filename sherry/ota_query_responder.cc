#include "ota_query_responder.h"
#include "log.h"
#include "../include/json/json.hpp"

namespace sherry{

static sherry::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

OTAQueryResponder::OTAQueryResponder(MqttClient::ptr client
                                    ,int device_no)
    :m_client(std::move(client))
    ,m_device_no(device_no){

}

// void OTAQueryResponder::handle(const std::string& payload){
//     if(!m_client){
//         SYLAR_LOG_ERROR(g_logger) << "client is null"
//                                   << std::endl;
//         return;
//     } 
//     // RWMutexType::ReadLock lock(m_client->m_mutex);
//     if(!m_client->get_isconnected() && !m_client->get_isconnected()){
//         SYLAR_LOG_ERROR(g_logger) << "client is diconnect" 
//                                   << std::endl;
//         return;
//     }
//     try{
//         auto request = nlohmann::json::parse(payload);
//         if(request.contains("action") && request["action"] == "getVersion"){
//             OTAQueryResponseMessage response;
//             response.action = "Version";
//             response.device_no = m_device_no;
//             response.time = getCurrentTimeString();
//             response.name = m_version_message->get_name();
//             response.version = m_version_message->get_version();
//             response.upgrade_time = m_version_message->get_upgrade_time();

//             std::string response_payload = response.to_json();
//             m_client->publish(m_topic, response_payload, 1, true);

//         } catch (const std::exception& e){
//             SYLAR_LOG_ERROR(g_logger) << "Failed to parse OTA query payload: " << e.what() 
//                                       << std::endl;
//         }
//     }
// }
std::string OTAQueryResponder::format_payload(const std::string& name, const std::string& action){
    nlohmann::json j = {
        {"name", name},
        {"action", action},
        {"time", getCurrentTimeString()}
    };
    return j.dump();
}
void OTAQueryResponder::publish_query(const std::string& name, const std::string& action, int qos, bool retain){
    if(!m_client){
        SYLAR_LOG_ERROR(g_logger) << "client is null"
                                  << std::endl;
        return;
    } 
    // RWMutexType::ReadLock lock(m_client->m_mutex);
    if(!m_client->get_isconnected() && !m_client->get_isconnected()){
        SYLAR_LOG_ERROR(g_logger) << "client is diconnect" 
                                  << std::endl;
        return;
    }

    std::string topic("/ota/");
    topic = topic + name;
    topic.push_back('/');

    std::string payload = format_payload(name, action);

    m_client->publish(topic, payload, qos, retain);

}


}