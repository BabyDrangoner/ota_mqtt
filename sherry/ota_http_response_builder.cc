#include "ota_http_response_builder.h"
#include "log.h"

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

OTAResponse::OTAResponse(float http_version)
    :m_http_version(http_version){
}

OTANotifyRes::OTANotifyRes(float http_version)
    :OTAResponse(http_version){
    
}

std::string OTANotifyRes::build_http_response(bool success, nlohmann::json& json_response){
    bool connect = true;
    if(success){
        json_response["status"] = "ok";
    } else {
        json_response["status"] = "error";
        connect = false;
    }

    std::string body = json_response.dump();

    std::stringstream ss;
    ss << "HTTP/" << m_http_version;
    if(success){
        ss << " 200 OK\n";
    } else {
        ss << " 400 BAD Request\n";
    }

    ss << "Content-Type: application/json"
       << "\nContent-Length: " << body.size()
       << "\nConnection: " << (connect ? "keep-alive\n" : "close\n")
       << '\n' << body;
    
    return ss.str();
}

OTAQueryRes::OTAQueryRes(float http_version)
    :OTAResponse(http_version){
    
}

std::string OTAQueryRes::build_http_response(bool success, nlohmann::json& json_response){
    if(success){
        json_response["status"] = "ok";
    } else {
        json_response["status"] = "error";
    }

    std::string body = json_response.dump();

    std::stringstream ss;
    ss << "HTTP/" << m_http_version;
    if(success){
        ss << " 200 OK\n";
    } else {
        ss << " 400 BAD Request\n";
    }

    ss << "Content-Type: application/json"
       << "\nContent-Length: " << body.size()
       << "\nConnection: " << "close\n"
       << '\n' << body;
    
    return ss.str();
}


OTAQueryDownloadRes::OTAQueryDownloadRes(float http_version)
    :OTAResponse(http_version){

}

std::string OTAQueryDownloadRes::build_http_response(bool success, nlohmann::json& json_response){
    bool connect = true;
    if(success){
        json_response["status"] = "ok";
    } else {
        json_response["status"] = "error";
        connect = false;
    }

    std::string body = json_response.dump();

    std::stringstream ss;
    ss << "HTTP/" << m_http_version;
    if(success){
        ss << " 200 OK\n";
    } else {
        ss << " 400 BAD Request\n";
    }

    if(json_response["download_status"] == "down"){
        connect = false;
    }

    ss << "Content-Type: application/json"
       << "\nContent-Length: " << body.size()
       << "\nConnection: " << (connect ? "keep-alive\n" : "close\n")
       << '\n' << body;
    
    return ss.str();
}

OTAFileDownloadRes::OTAFileDownloadRes(float http_version)
    :OTAResponse(http_version){

}


std::string OTAFileDownloadRes::build_http_response(bool success, nlohmann::json& json_response){
    if(success){
        json_response["status"] = "ok";
    } else {
        json_response["status"] = "error";
    }

    std::string body = json_response.dump();

    std::stringstream ss;
    ss << "HTTP/" << m_http_version;
    if(success){
        ss << " 200 OK\n";
    } else {
        ss << " 400 BAD Request\n";
    }

    ss << "Content-Type: application/octet-stream"
       << "\nContent-Length: " << body.size()
       << "\nConnection: " << "close\n"
       << '\n' << body;
    
    return ss.str();
}


OTAHttpResBuilder::OTAHttpResBuilder(float http_version){
    m_response.clear();
    m_response["notify"] = std::make_shared<OTANotifyRes>(http_version);
    m_response["query"] = std::make_shared<OTAQueryRes>(http_version);
    m_response["query_download"] = std::make_shared<OTAQueryDownloadRes>(http_version);
    m_response["file_download"] = std::make_shared<OTAFileDownloadRes>(http_version);
}

std::string OTAHttpResBuilder::build_http_response(const std::string& type, bool success, nlohmann::json& json_response){
    auto it = m_response.find(type);
    if(it == m_response.end()){
        return "";
    }

    if(!(*it).second){
        return "";
    }
    
    return (*it).second->build_http_response(success, json_response);
}

}