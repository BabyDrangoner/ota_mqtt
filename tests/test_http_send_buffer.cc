#include "../sherry/http/http_send_buffer.h"
#include <iostream>
#include <memory>

using namespace sherry;

int main() {
    HttpSendBuffer::ptr sendBuffer = std::make_shared<HttpSendBuffer>();

    uint32_t topic1 = 1;
    uint32_t topic2 = 2;

    // 给socket1添加3条数据
    for (int i = 0; i < 3; ++i) {
        auto data = std::make_shared<OTAData>();
        data->m_data = 100 + i;
        sendBuffer->addData(topic1, data);
    }

    // 给socket2添加2条数据
    for (int i = 0; i < 2; ++i) {
        auto data = std::make_shared<OTAData>();
        data->m_data = 200 + i;
        sendBuffer->addData(topic2, data);
    }

    // 取出socket1的数据
    auto datas1 = sendBuffer->getData(topic1);
    std::cout << "Socket 1 Data:" << std::endl;
    for (auto& d : datas1) {
        std::cout << "  OTAData: " << d->m_data << std::endl;
    }

    // 取出socket2的数据
    auto datas2 = sendBuffer->getData(topic2);
    std::cout << "Socket 2 Data:" << std::endl;
    for (auto& d : datas2) {
        std::cout << "  OTAData: " << d->m_data << std::endl;
    }

    // 再次取，确认已经清空
    auto datas1_empty = sendBuffer->getData(topic1);
    std::cout << "Socket 1 Data after fetch again, size: " << datas1_empty.size() << std::endl;

    auto datas2_empty = sendBuffer->getData(topic2);
    std::cout << "Socket 2 Data after fetch again, size: " << datas2_empty.size() << std::endl;

    return 0;
}
