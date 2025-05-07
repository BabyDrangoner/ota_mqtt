#ifndef _SHERRY_OTAHTTP_COMMAND_DISPATCHER_H__
#define _SHERRY_OTAHTTP_COMMAND_DISPATCHER_H__

#include "sherry.h"
#include "ota_manager.h"
#include "../include/json/json.hpp"

namespace sherry{

class OTAManager;

class OTARequest{
public:
    typedef std::shared_ptr<OTARequest> ptr;
    OTARequest(OTAManager::ptr ota_manager);
    virtual ~OTARequest(){}

    virtual bool submit(const nlohmann::json& j, int connection_id) = 0;
protected:
    OTAManager::ptr m_ota_mgr;
    
};

class OTANotifyReq : public OTARequest{
public:
    OTANotifyReq(OTAManager::ptr ota_manager);
    ~OTANotifyReq(){}

    virtual bool submit(const nlohmann::json& j, int connection_id);
};

class OTAQueryReq : public OTARequest{
public:
    OTAQueryReq(OTAManager::ptr ota_manager);
    ~OTAQueryReq(){}

    virtual bool submit(const nlohmann::json& j, int connection_id);
};

class OTAStopNotifyReq : public OTARequest{
public:
    OTAStopNotifyReq(OTAManager::ptr ota_manager);
    ~OTAStopNotifyReq(){}

    virtual bool submit(const nlohmann::json& j, int connection_id);
};

class OTAQueryDownloadReq : public OTARequest{
public:
    OTAQueryDownloadReq(OTAManager::ptr ota_manager);
    ~OTAQueryDownloadReq(){}

    virtual bool submit(const nlohmann::json& j, int connection_id);
};

class OTAFileDownloadReq : public OTARequest{
public:
    OTAFileDownloadReq(OTAManager::ptr ota_manager);
    ~OTAFileDownloadReq(){}

    virtual bool submit(const nlohmann::json& j, int connection_id);
};


class OTAHttpCommandDispatcher{
public:
    typedef std::shared_ptr<OTAHttpCommandDispatcher> ptr;
    
    OTAHttpCommandDispatcher(const std::string& http_version, const std::string& content_type, OTAManager::ptr ota_manager);

    bool handle_request(std::string& request, int connect_id);
    bool check_uri(const std::string& uri, std::string& res);
    bool check_uri(const std::string& uri, uint16_t& device_type, uint32_t& device_no, std::string& name, std::string& res);

    bool check_protocol(const std::string& protocol);

private:
    std::string m_http_version;
    std::string m_content_type;
    std::unordered_map<std::string, OTARequest::ptr> m_command_dispatchers;
};
}

#endif