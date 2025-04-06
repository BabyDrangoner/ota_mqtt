#include "ota_subscribe_download.h"
#include "log.h"
#include <functional>

static sherry::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

namespace sherry{

OTASubscribeDownload::OTASubscribeDownload(MqttClient::ptr client, const std::string& topic, int device_type, int device_id, OTAMessage::ptr msg)
    :m_client(client)
    ,m_topic(topic)
    ,m_device_type(device_type)
    ,m_device_id(device_id)
    ,m_msg(msg){

}

void OTASubscribeDownload::subscribe_download(const std::string& topic, int qos){
    if(!m_client){
        SYLAR_LOG_ERROR(g_logger) << "client is null, when subscribe topic: " << topic
                                  << std::endl;
        return;
    }

    if(!m_client->get_isconnected()){
        SYLAR_LOG_ERROR(g_logger) << "client is not connected, when subscribe topic: " << topic
                                  << std::endl;
    }

    m_client->subscribe(topic, qos, [this, qos](const std::string& topic, int res){
        if(res == -1){
            m_client->subscribe(topic, 1);
            SYLAR_LOG_ERROR(g_logger) << "subscribe to topic: " << topic
                                      << " for downloading details failed, now subscribe again!"
                                      << std::endl;
        } else if(res == 0){
            SYLAR_LOG_INFO(g_logger) << "subscribe to topic: " << topic
                                     << " for downloading details success!"
                                     << std::endl;
        }
    });
}

}