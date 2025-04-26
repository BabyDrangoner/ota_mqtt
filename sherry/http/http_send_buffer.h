#ifndef _SHERRY_HTTP_SENDBUFFER_H__
#define _SHERRY_HTTP_SENDBUFFER_H__

#include "../sherry.h"
#include "../socket.h"

#include <unordered_map>
#include <vector>
#include <memory>

namespace sherry {

struct OTAData {
    int m_data;
};

class HttpSocketBuffer {
public:
    typedef std::shared_ptr<HttpSocketBuffer> ptr;
    typedef Mutex MutexType;

    HttpSocketBuffer() = default;

    ~HttpSocketBuffer() {
        m_buffer.clear();
    }

    void addData(std::shared_ptr<OTAData> data);

    std::vector<std::shared_ptr<OTAData>> getData();

private:
    MutexType m_mutex;
    std::vector<std::shared_ptr<OTAData>> m_buffer;
};

class HttpSendBuffer {
public:
    typedef std::shared_ptr<HttpSendBuffer> ptr;
    typedef Mutex MutexType;

    HttpSendBuffer() = default;

    ~HttpSendBuffer() {
        m_mp.clear();
    }

    void addData(uint32_t key, std::shared_ptr<OTAData> data);

    std::vector<std::shared_ptr<OTAData>> getData(uint32_t key);

private:
    MutexType m_mutex;
    std::unordered_map<uint32_t, HttpSocketBuffer::ptr> m_mp;
};

}

#endif
