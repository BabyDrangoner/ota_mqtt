#ifndef _SHERRY_OTA_DOWNLOAD_H__
#define _SHERRY_OTA_DOWNLOAD_H__

#include "sherry.h"
#include "mqtt_client.h"
#include "ota_message.h"

namespace sherry{

class OTASubscribeDownload{
public:
    typedef std::shared_ptr<OTASubscribeDownload> ptr;
    typedef RWMutex RWMutexType;

    OTASubscribeDownload(MqttClient::ptr client, const std::string& topic, int device_type, int device_id, OTAMessage::ptr msg);

    void subscribe_download(const std::string& topic, int qos=1);
    
private:
    RWMutexType m_mutex;
    MqttClient::ptr m_client;
    std::string m_topic;
    int m_device_type;
    int m_device_id;

    OTAMessage::ptr m_msg;

};

}
#endif