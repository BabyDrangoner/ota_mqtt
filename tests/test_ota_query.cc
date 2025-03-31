#include "../sherry/sherry.h"
#include "../sherry/ota_query_responder.h"
#include "../sherry/mqtt_client.h"

static sherry::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

std::string protocol = "tcp";
std::string host = "localhost";
int port = 1883;
std::string client_id = "ota_query_test";
int device_type = 1;

std::string name = "agsspds";
std::string action = "getVersion";

void test01(){
    sherry::MqttClient::ptr client = std::make_shared<sherry::MqttClient>(
        protocol, port, host, client_id, device_type,
        [](const std::string& topic, const std::string& payload) {
            SYLAR_LOG_INFO(g_logger) << "收到消息 topic: " << topic
                                     << " payload: " << payload;
        }
    );
    sherry::OTAQueryResponder::ptr oqr = std::make_shared<sherry::OTAQueryResponder>(client, device_type);
    
    client->connect();
    oqr->publish_query(name, action, 1, true);

    while(1);
}

int main(void){

    test01();

    return 0;
}