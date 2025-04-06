#include "../sherry/sherry.h"
#include "../sherry/ota_query_responder.h"
#include "../sherry/mqtt_client.h"
#include "../sherry/ota_client_callback.h"

static sherry::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

std::string protocol = "tcp";
std::string host = "localhost";
int port = 1883;
std::string client_id = "ota_query_test";
int device_type = 1;

std::string name = "agsspds";
std::string action = "getVersion";
std::string sub_topic1 = "/ota/agsspds1/responder/";
std::string sub_topic2 = "/ota/agsspds2/responder/";

sherry::OTAClientCallbackManager::ptr cb_mgr = std::make_shared<sherry::OTAClientCallbackManager>();


void test01(){

    sherry::MqttClient::ptr client = std::make_shared<sherry::MqttClient>(
        protocol, port, host, client_id, device_type, cb_mgr);
    std::string pub_topic = "/ota/agsspds/query/";

    cb_mgr->regist_callback(sub_topic1, [client, pub_topic](const std::string& topic, const std::string& payload){
        if(payload == "123"){
            client->publish(pub_topic, "", 1);
            SYLAR_LOG_INFO(g_logger) << "topic : " << topic 
                                     << " callback success."
                                     << std::endl;
            client->unsubscribe(topic);
        }
    });

    cb_mgr->regist_callback(sub_topic2, [client, pub_topic](const std::string& topic, const std::string& payload){
        if(payload == "456"){
            client->publish(pub_topic, "", 1);
            SYLAR_LOG_INFO(g_logger) << "topic : " << topic 
                                     << " callback success."
                                     << std::endl;
            client->unsubscribe(topic);
        }
    });

    sherry::OTAQueryResponder::ptr oqr = std::make_shared<sherry::OTAQueryResponder>(client, device_type);
    
    client->connect();
    oqr->publish_query(name, action, 1, true);
    sleep(1);
    oqr->subscribe_responder(pub_topic, sub_topic1, 1);
    oqr->subscribe_responder(pub_topic, sub_topic2, 1);


    while(1);
}

int main(void){

    test01();

    return 0;
}