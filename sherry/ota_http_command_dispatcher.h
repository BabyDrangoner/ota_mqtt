#ifndef _SHERRY_OTAHTTP_COMMAND_DISPATCHER_H__
#define _SHERRY_OTAHTTP_COMMAND_DISPATCHER_H__

#include "sherry.h"
#include "ota_manager.h"
#include "../include/json/json.hpp"

namespace sherry{

class OTAHttpCommandDispatcher{
public:
    typedef std::shared_ptr<OTAHttpCommandDispatcher> ptr;
    
    OTAHttpCommandDispatcher(OTAManager::ptr ota_manager);

    void handle_command(const nlohmann::json& j, int connection_id);
private:
    OTAManager::ptr m_ota_manager;
};
}

#endif