#include "../sherry/sherry.h"
#include "../sherry/mqtt_client.h"
#include "../sherry/address.h"
#include "../sherry/iomanager.h"

#include <iostream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

std::string getCurrentTimeString() {
    // 获取当前时间点
    auto now = std::chrono::system_clock::now();
    // 转换为 time_t
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    // 转换为 tm 结构（本地时间）
    std::tm local_tm = *std::localtime(&now_time_t);

    // 使用 stringstream 格式化时间
    std::stringstream ss;
    ss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

static sherry::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

std::string mqtt_client_id = "sherry publish";
std::string mqtt_server_host = "localhost";
std::string protocol = "tcp";
int port = 1883;

int device_type = 0;
// 声明一个 std::function 回调函数变量
std::function<void(const std::string&, const std::string&)> print_message_callback =
    [](const std::string& topic, const std::string& payload) {
        SYLAR_LOG_INFO(g_logger) << "subscribe topic: " << topic
                                 << " message: " << payload
                                 << std::endl;
};

std::string topic = "test/topic";

void test01(){
    sherry::MqttAddress::ptr addr = std::make_shared<sherry::MqttAddress>(protocol, port, mqtt_server_host);
    sherry::MqttClient::ptr client = std::make_shared<sherry::MqttClient>(protocol, port, mqtt_server_host, mqtt_client_id, device_type, print_message_callback);
    
    client->connect();

    
    while(true){
        std::string payload = getCurrentTimeString();
        client->publish(topic, payload);
        sleep(1);
    }

}

int main(void){
    sherry::Scheduler sc(2, true, "test1");
    sc.start();
    sc.schedule(&test01);
    sc.stop();
    return 0;
}