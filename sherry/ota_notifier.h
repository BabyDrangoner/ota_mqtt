#ifndef _SHERRY_OTA_NOTIFIER_H__
#define _SHERRY_OTA_NOTIFIER_H__

#include "mqtt_client.h"
#include "timer.h"
#include <string>
#include <memory>

namespace sherry{

struct OTAMessage{
    std::string name;
    std::string version;
    std::string time;
    std::string file_name;
    size_t file_size = 0;
    std::string url_path;
    std::string md5_value;
    int launch_mode = 0;
    int upgrade_mode = 1;

    std::string to_json() const;
};

class OTANotifier{
public:
    typedef std::shared_ptr<OTANotifier> ptr;
    typedef RWMutex RWMutexType;

    OTANotifier(MqttClient::ptr client
                ,TimerManager::ptr timer_mgr
                ,const std::string& topic
                ,uint64_t interval_ms = 180000);
    
    void set_message(const OTAMessage& msg);
    void start();
    void stop();

private:
    void publish_once(); // 实际发布逻辑
    RWMutexType m_mutex;

    MqttClient::ptr m_client;
    TimerManager::ptr m_timer_mgr;
    Timer::ptr m_timer;

    std::string m_topic;
    OTAMessage m_msg;
    uint64_t m_interval_ms;
};


}

#endif