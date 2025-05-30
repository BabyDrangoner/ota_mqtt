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
#include "ota_subscribe_download.h"

#include <unordered_map>
#include <unordered_set>

namespace sherry{
class HttpServer;
class OTAManager{
public:
    typedef std::shared_ptr<OTAManager> ptr;
    typedef RWMutex RWMutexType;
    OTAManager(size_t file_size, const std::string& protocol, const std::string& host, int port, float http_version, const std::string& file_prev_path);

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
    void ota_stop_notify(uint16_t device_type, const std::string& name, const std::string& verison, int connect_id);
    void ota_query(uint16_t device_type, uint32_t device_no, const std::string& action, int connect_id);
    void ota_query_download(uint16_t device_type, uint32_t device_no, const std::string& detail, int connect_id);
    void ota_file_download(uint16_t device_type, const std::string& name, const std::string& version, int connect_id);

    bool send_file(uint16_t device_type, const std::string& name, const std::string& version, int connect_id);

    void submit(uint16_t device_type
               ,uint32_t device_no
               ,const std::string& command
               ,int client_id
               ,std::unordered_map<std::string, std::string>& detail);

    bool check_device(uint16_t device_type, uint32_t device_no);
    bool check_device(uint16_t device_type);

private:
    bool get_notify_message(uint16_t device_type, const std::string& name, const std::string& version, struct OTAMessage& msg);
    void response_to_server(int fd, bool success, const std::string& type, nlohmann::json& j, bool need_header);
    void response_to_server(int fd, bool success, const std::string& type, char* buffer, size_t buffer_size);

private:
    RWMutexType m_mutex;
    RWMutexType m_notifier_mutex;
    std::string m_protocol;
    std::string m_host;
    int m_port;

    std::string m_file_prev_path;
    ssize_t m_buffer_size;


    std::shared_ptr<HttpServer> m_http_server;
    TimerManager::ptr m_timer_mgr;
    uint16_t m_device_types_counts;
    uint64_t m_device_counts;
    OTAClientCallbackManager::ptr m_callback_mgr;
    MqttClientManager::ptr m_client_mgr;
    
    std::unordered_map<uint16_t, std::unordered_set<uint32_t>> m_device_type_nums;
    std::unordered_map<std::string, OTANotifier::ptr> m_ota_notifier_map;
    std::unordered_map<uint16_t, std::unordered_map<uint32_t, OTASubscribeDownload::ptr>> m_ota_subscribe_download_map;
    
    Scheduler::ptr m_scheduler;
    OTAHttpResBuilder::ptr m_ota_http_res_builder;


};
}
#endif