#ifndef _SHERRY_HTTPSERVER_H__
#define _SHERRY_HTTPSERVER_H__

#include "../sherry.h"
#include "../socket.h"
#include "../scheduler.h"
#include "../timer.h"
#include "../ota_http_command_dispatcher.h"

#include <sys/epoll.h>
#include <memory.h>
#include <atomic>
#include <queue>

namespace sherry{

class HttpServer : public Scheduler, public TimerManager, public std::enable_shared_from_this<HttpServer>{
friend class HttpSendBuffer;
public:
    typedef std::shared_ptr<HttpServer> ptr;
    typedef Mutex MutexType;

    enum Event {
        NONE = 0x0,
        READ = 0x1,
        WRITE = 0x4,

    };

    HttpServer(size_t threads, bool use_caller, const std::string & name,
               const std::string& http_version, const std::string& content_type, OTAManager::ptr ota_mgr=nullptr);
    ~HttpServer();
    
    int addEvent(int fd, Event event, Socket::ptr sock=nullptr, std::function<void()> cb = nullptr, bool persistent=false);
    bool delEvent(int fd, Event event);
    bool cancelEvent(int fd, Event event);

    bool cancelAll(int fd);

    void response(int fd, const std::string& data);

    // 启动监听服务
    bool start(uint16_t port);
    void stop();

    static HttpServer* GetThis();

    struct FdContext{
        typedef Mutex MutexType;
        struct EventContext{
            Scheduler* scheduler = nullptr;    // 事件执行的scheduler
            Fiber::ptr fiber;                  // 事件协程
            std::function<void()> cb;          // 事件的回调函数
            bool persistent;
        };

        EventContext & getContext(Event event);
        void resetContext(EventContext & ctx);
        void triggerEvent(Event event);
        void reset();

        int fd;               // 事件关联的句柄
        // std::weak_ptr<Socket> sock;
        Socket::ptr sock;
        EventContext read;    // 读事件
        EventContext write;   // 写事件
        Event events = NONE; // 已经注册的事件
        MutexType mutex;
    };


protected:

    void tickle() override;
    bool stopping() override;
    bool stopping(uint64_t & timeout);
    void idle() override;
    void onTimerInsertedAtFront() override;

    void contextResize(size_t size);

private:
    void handleClient(Socket::ptr client);  // 每个连接的处理

private:

    RWMutexType m_mutex;
    std::atomic<bool> m_running;  // 控制server是否运行
    OTAHttpCommandDispatcher::ptr m_dispatcher;
    Socket::ptr m_listenSocket;       // 保存监听socket

private:
    int m_epfd = 0;
    int m_tickleFds[2];

    std::atomic<size_t> m_pendingEventCount = {0};
    std::vector<FdContext*> m_fdContexts;

    MutexType m_fdCtx_queue_mutex;
    std::queue<FdContext*> m_reset_fdCtx_queue;
};



}

#endif