#ifndef _SHERRY_OTAMANAGER_H__
#define _SHERRY_OTAMANAGER_H__

#include "sherry.h"
#include "scheduler.h"
#include "ota_notifier.h"
#include "ota_query_responder.h"
#include "mqtt_client.h"
#include "timer.h"
#include "ota_client_callback.h"
#include "ota_http_response_builder.h"

#include <unordered_map>
#include <unordered_set>

namespace sherry{
class HttpServer;
class OTAManager{
public:
    typedef std::shared_ptr<OTAManager> ptr;
    typedef RWMutex RWMutexType;
    OTAManager(const std::string& protocol, const std::string& host, int port, float http_version);

    void set_protocol(const std::string& v){ m_protocol = v;}
    void set_host(const std::string& v){ m_host = v;}
    void set_port(int v) { m_port = v;}
    void set_http_server(std::shared_ptr<HttpServer> v){ m_http_server = v;}
    
    std::string get_protocol() const{ return m_protocol;}
    std::string get_host() const { return m_host;}
    int get_port() const { return m_port;}
    uint16_t get_device_types_counts() const { return m_device_types_counts;}
    uint64_t get_device_counts() const { return m_device_counts;}

    bool add_device(uint16_t device_type, uint32_t device_no);
    bool remove_device(uint16_t device_type, uint32_t device_no);

    void ota_notify(uint16_t device_type, struct OTAMessage& msg, int connect_id);
    void ota_stop_notify(uint16_t device_type);

    void ota_query(uint16_t device_type, uint32_t device_no, const std::string& action, int connect_id);

    void submit(uint16_t device_type
               ,uint32_t device_no
               ,const std::string& command
               ,int client_id
               ,const std::string& version = "");

private:
    bool get_notify_message(uint16_t device_type, const std::string& version, struct OTAMessage& msg);
    void response_to_server(int fd, bool success, nlohmann::json& j);


private:
    RWMutexType m_mutex;
    RWMutexType m_notifier_mutex;
    std::string m_protocol;
    std::string m_host;
    int m_port;

    std::shared_ptr<HttpServer> m_http_server;
    TimerManager::ptr m_timer_mgr;
    uint16_t m_device_types_counts;
    uint64_t m_device_counts;
    OTAClientCallbackManager::ptr m_callback_mgr;
    MqttClientManager::ptr m_client_mgr;
    
    std::unordered_map<uint16_t, std::unordered_set<uint32_t>> m_device_type_nums;
    std::unordered_map<uint16_t, OTANotifier::ptr> m_ota_notifier_map;
    Scheduler::ptr m_scheduler;
    OTAHttpResBuilder::ptr m_ota_http_res_builder;


};
}
#endif