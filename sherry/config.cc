#include "config.h"

namespace sherry{

// static sherry::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
ConfigVarBase::ptr Config::LookupBase(const std::string & name){
    RWMutexType::ReadLock lock(GetMutex());
    auto it = GetDatas().find(name);
    return it == GetDatas().end() ? nullptr : it->second; 
}

static void ListAllMember(const std::string & prefix,
                            const YAML::Node & node,
                            std::list<std::pair<std::string, const YAML::Node> > & output){
    if(prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._012345678")
                != std::string::npos){
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Config invalid name: " << prefix << " : " << node;
        return;
    }

    output.push_back(std::make_pair(prefix, node));

    if(node.IsMap()){
        for(auto it = node.begin();it != node.end();++it){
            ListAllMember(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(), it->second, output);
        }
    }
}

void Config::LoadFromYaml(const YAML::Node & root){
    std::list<std::pair<std::string, const YAML::Node> > all_nodes;
    ListAllMember("", root, all_nodes);

    for(auto & i : all_nodes){
        std::string key = i.first;
        if(key.empty()){
            continue;
        }
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        ConfigVarBase::ptr var = LookupBase(key);

        if(var){
            if(i.second.IsScalar()){
                var->fromString(i.second.Scalar());
            } else {
                std::stringstream ss;
                ss << i.second;
                var->fromString(ss.str());
            }
        }
    }
}

void Config::Vist(std::function<void(ConfigVarBase::ptr)> cb){
    RWMutexType::ReadLock lock(GetMutex());
    ConfigVarMap & m = GetDatas();
    for(auto it = m.begin();it != m.end();++it){
        cb(it->second);
    }
}

}