#ifndef __SHERRY_OTA_QUERY_RESPONDER_H__
#define __SHERRY_OTA_QUERY_RESPONDER_H__

#include "mqtt_client.h"
#include "ota_version_message.h"
#include <string>
#include <memory>

namespace sherry{


class OTAQueryResponder{
public:
    typedef std::shared_ptr<OTAQueryResponder> ptr;
    typedef RWMutex RWMutexType;

    OTAQueryResponder(MqttClient::ptr client
                     ,int device_no);
    
    std::string format_payload(const std::string& name, const std::string& action);
    void publish_query(const std::string& name, const std::string& action, int qos = 1, bool retain = false);
private:
    RWMutexType m_mutex;
    MqttClient::ptr m_client;
    
    int m_device_no;
};

}
#endif