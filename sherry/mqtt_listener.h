#ifndef SHERRY_MQTT_LISTENER_H__
#define SHERRY_MQTT_LISTENER_H__


#include <string>
#include <functional>
#include <mqtt/async_client.h>

namespace sherry{

class MqttActionListener : public virtual mqtt::iaction_listener{
public:
    typedef std::function<void(const std::string& topic, int result_code)> CallbackType;

    MqttActionListener(const std::string& topic, CallbackType callback)
        :m_topic(topic)
        ,m_callback(std::move(callback)){}
    
    void on_success(const mqtt::token&) override{
        if(m_callback) m_callback(m_topic, 0); // 成功 code = 0
    }

    void on_failure(const mqtt::token&) override{
        if(m_callback) m_callback(m_topic, -1); // 失败， code = -1
    }

private:
    std::string m_topic;
    CallbackType m_callback;
};

class MqttDisconnectListener : public virtual mqtt::iaction_listener{
public:
    typedef std::function<void(int)> CallbackType; 
    MqttDisconnectListener(CallbackType callback)
        :m_callback(std::move(callback)){};
    
    void on_success(const mqtt::token&) override{
        if(m_callback) m_callback(0);
    }

    void on_failure(const mqtt::token&) override{
        if(m_callback) m_callback(-1);
    }

private:
    CallbackType m_callback;
};



}


#endif