#include "http_server.h"
#include "../address.h"

#include <fcntl.h>
#include <sys/epoll.h>

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

HttpServer::HttpServer(size_t threads, bool use_caller, const std::string & name, OTAManager::ptr ota_mgr)
    :Scheduler(threads, use_caller, name)
    ,m_running(false){
    m_send_buffer = std::make_shared<HttpSendBuffer>();
    ota_mgr->set_send_buffer(m_send_buffer);
    m_dispatcher = std::make_shared<OTAHttpCommandDispatcher>(ota_mgr);

    m_epfd = epoll_create(5000);
    SYLAR_ASSERT(m_epfd > 0);

    int rt = pipe(m_tickleFds);
    SYLAR_ASSERT(!rt);

    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_tickleFds[0];

    epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);

    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
    SYLAR_ASSERT(!rt);

    contextResize(32);
    
    Scheduler::start();
}

HttpServer::~HttpServer(){

    stop();
    close(m_epfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);

    for(size_t i = 0;i < m_fdContexts.size();++i){
        if(m_fdContexts[i]){
            delete m_fdContexts[i];
        }
    }
}

bool HttpServer::start(uint16_t port){
    if(m_running){
        SYLAR_LOG_WARN(g_logger) << "HttpServer already running.";
        return false;
    }

    m_listenSocket = Socket::CreateTCPSocket();
    auto addr = IPAddress::Create("0.0.0.0", port);
    SYLAR_LOG_INFO(g_logger) << "listen fd = " << m_listenSocket->getSocket();

    if(!m_listenSocket->bind(addr)){
        SYLAR_LOG_ERROR(g_logger) << "HttpServer bind failed on port " << port
                                  << ".";
        return false;
    }

    if(!m_listenSocket->listen()){
        SYLAR_LOG_ERROR(g_logger) << "HttpServer listen failed.";
        return false;
    }

    SYLAR_LOG_INFO(g_logger) << "HttpServer started on port " << port
                             << ".";
    m_running = true;
    addEvent(m_listenSocket->getSocket(), HttpServer::READ, m_listenSocket, [this](){
        auto client = m_listenSocket->accept();
        if(client){
            SYLAR_LOG_DEBUG(g_logger) << "client is not null";
            int cli_fd = client->getSocket();
            addEvent(cli_fd, HttpServer::READ, client, ([this, client](){
                this->handleClient(client);
            }));
            
        } 
    });
    
    return true;
}

void HttpServer::idle(){
    epoll_event * events = new epoll_event[64]();
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr){
        delete[] ptr;
    });
    while(true){
        uint64_t next_timeout = 0;
        if(stopping(next_timeout)){
            if(next_timeout == ~0ull){
                SYLAR_LOG_INFO(g_logger) << "name=" << getName() << " idle stopping exit";
                break;
            }
        }
        
        int rt = 0;
        do{
            static const int MAX_TIMEOUT = 3000;
            if(next_timeout != ~0ull){
                next_timeout = (int)next_timeout > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
            } else{
                next_timeout = MAX_TIMEOUT;
            }
            rt = epoll_wait(m_epfd, events, 64, (int)next_timeout);
            if(rt < 0 && errno == EINTR){
            }else {
                break;
            }
        } while(true);

        std::vector<std::function<void()> > cbs;
        listExpiredCb(cbs);

        for(FdContext* fd_c : m_fdContexts){
            Socket::ptr sock = fd_c->sock;
            if(!sock){
                continue;
            }

            if(!sock->isConnected()){
                fd_c->sock = nullptr;
                std::string data = m_send_buffer->getData(fd_c->fd);
                continue;
            }

            std::string data = m_send_buffer->getData(fd_c->fd);
            SYLAR_LOG_DEBUG(g_logger) << "idle: " << data;

            if(data != ""){
                
                cbs.emplace_back([data, sock]{
                    sock->send(data.data(), data.size(), 0);
                });

                SYLAR_LOG_DEBUG(g_logger) << "get mqtt data";
            }
        }
        if(!cbs.empty()){
            // SYLAR_LOG_DEBUG(g_logger) << "on timer cbs.size=" << cbs.size();
            schedule(cbs.begin(), cbs.end());
            cbs.clear();
        }

        for(int i = 0;i < rt;++i){
            epoll_event & event = events[i];
            if(event.data.fd == m_tickleFds[0]){
                uint8_t dummy;
                while(read(m_tickleFds[0], &dummy, 1) == 1);
                continue;
            }
            HttpServer::FdContext * fd_ctx = (FdContext*)event.data.ptr;
            HttpServer::FdContext::MutexType::Lock lock(fd_ctx->mutex);
            if(event.events & (EPOLLERR | EPOLLHUP)){
                event.events |= EPOLLIN | EPOLLOUT;
            }
            int real_events = NONE;
            if(event.events & EPOLLIN){
                real_events |= READ;
            }
            if(event.events & EPOLLOUT){
                real_events |= WRITE;
            }

            if((fd_ctx->events & real_events) == NONE){
                continue;
            }

            int left_events = (fd_ctx->events & ~real_events);
            int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            event.events = EPOLLET | left_events;

            int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
            if(rt2){
                SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                    << op << "," << fd_ctx->fd << "," << event.events << ")"
                    << rt2 << " (" << errno << ") (" << strerror(errno) << ")";
                continue; 
            }

            if(real_events & READ){
                fd_ctx->triggerEvent(READ);
                --m_pendingEventCount;
            }

            if(real_events & WRITE){
                fd_ctx->triggerEvent(WRITE);
                --m_pendingEventCount;
            }
        }
        Fiber::ptr cur = Fiber::GetThis();
        auto raw_ptr = cur.get();
        cur.reset();

        raw_ptr->swapOut();
    }
}

void HttpServer::stop(){
    if(!m_running) return;
    m_running = false;
    if(m_listenSocket){
        m_listenSocket->close();
        m_listenSocket.reset();
    }

    SYLAR_LOG_INFO(g_logger) << "HttpServer stopped.";
}

void HttpServer::handleClient(Socket::ptr client) {
    if (!client) {
        SYLAR_LOG_ERROR(g_logger) << "handleClient: client is null";
        return;
    }

    char buffer[4096] = {0};
    int len = client->recv(buffer, sizeof(buffer) - 1);
    if (len <= 0) {
        SYLAR_LOG_WARN(g_logger) << "handleClient: recv failed, len=" << len;
        client->close();
        delEvent(client->getSocket(), READ);
        return;
    }

    std::string request(buffer);
    SYLAR_LOG_INFO(g_logger) << "Received:\n" << request;

    try {
        // 先找 HTTP 报文头和 body 的分隔
        size_t pos = request.find("\r\n\r\n");
        if (pos == std::string::npos) {
            SYLAR_LOG_ERROR(g_logger) << "handleClient: Invalid HTTP request format, missing header-body separator.";
            client->close();
            cancelAll(client->getSocket());
            return;
        }

        // 提取 body
        std::string body = request.substr(pos + 4); // 跳过 "\r\n\r\n"
        SYLAR_LOG_INFO(g_logger) << "Extracted body:\n" << body;

        // 解析 body成JSON对象
        nlohmann::json json_request = nlohmann::json::parse(body);

        // 调用 dispatcher 处理 JSON命令
        m_dispatcher->handle_command(json_request, client->getSocket());

    } catch (const std::exception& e) {
        SYLAR_LOG_ERROR(g_logger) << "handleClient: JSON parse error: " << e.what();
        client->close();
        cancelAll(client->getSocket());
        return;
    }
}


HttpServer::FdContext::EventContext & HttpServer::FdContext::getContext(HttpServer::Event event){
    switch (event)
    {
    case HttpServer::READ:
        return read;
    case HttpServer::WRITE:
        return write;
    default:
        SYLAR_ASSERT2(false, "getContext");
    }
}

void HttpServer::FdContext::resetContext(EventContext & ctx){
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.cb = nullptr;

}

void HttpServer::FdContext::triggerEvent(HttpServer::Event event){
    SYLAR_ASSERT(events & event);
    events = (Event)(events & ~event);
    EventContext & ctx = getContext(event);
    if(ctx.cb){
        ctx.scheduler->schedule(&ctx.cb);
    } else {
        ctx.scheduler->schedule(&ctx.fiber);
    }
    ctx.scheduler = nullptr;
    return;
}

void HttpServer::contextResize(size_t size){  //
    m_fdContexts.resize(size);

    for(size_t i = 0;i < m_fdContexts.size();++i){
        if(!m_fdContexts[i]){
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;
        }
    }
}

// 1 success, 0 retry, -1 error
int HttpServer::addEvent(int fd, Event event, Socket::ptr sock, std::function<void()> cb){  //
    FdContext * fd_ctx = nullptr;
    
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() > fd){
        fd_ctx = m_fdContexts[fd];
        lock.unlock();
    } else{
        lock.unlock();
        RWMutexType::WriteLock lock2(m_mutex);
        contextResize(fd * 1.5);
        fd_ctx = m_fdContexts[fd];
    }

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(fd_ctx->events & event){
        SYLAR_LOG_ERROR(g_logger) << "addEvent assert fd=" << fd
                                  << " event=" << event
                                  << " fd_ctx.event=" << fd_ctx->events;
        SYLAR_ASSERT(!(fd_ctx->events & event));
    }

    int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events = EPOLLET | fd_ctx->events | event;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt){
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl" << m_epfd << ", "
                << op << "," << fd << "," << epevent.events << "):"
                << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return -1; 
    }

    ++m_pendingEventCount;
    fd_ctx->events = (Event)(fd_ctx->events | event);
    FdContext::EventContext & event_ctx = fd_ctx->getContext(event);
    SYLAR_ASSERT(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.cb);

    event_ctx.scheduler = Scheduler::GetThis();

    if(cb){
        event_ctx.cb.swap(cb);
    } else{
        event_ctx.fiber = Fiber::GetThis();
        SYLAR_ASSERT(event_ctx.fiber->getState() == Fiber::EXEC);
    }

    fd_ctx->sock = sock;

    return 0;
}

bool HttpServer::delEvent(int fd, Event event){
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd){
        return false;
    }
    FdContext * fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!(fd_ctx->events & event)){
        return false;
    }

    Event new_events = (Event)(fd_ctx->events & ~event);
    // 如果new_events为0，则不需要监听了，从epoll中删除；否则，修改
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;   
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt){
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl" << m_epfd << ", "
                << op << "," << fd << "," << epevent.events << "):"
                << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false; 
    }
    // 修改事件数据
    --m_pendingEventCount;
    fd_ctx->events = new_events;
    FdContext::EventContext & event_ctx = fd_ctx->getContext(event);
    fd_ctx->resetContext(event_ctx);
    return true;
}

bool HttpServer::cancelEvent(int fd, Event event){  //
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd){
        return false;
    }
    FdContext * fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!(fd_ctx->events & event)){
        return false;
    }

    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt){
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                << op << "," << fd << "," << epevent.events << "):"
                << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false; 
    }

    fd_ctx->triggerEvent(event);
    --m_pendingEventCount;
    return true;
}

bool HttpServer::cancelAll(int fd){
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd){
        return false;
    }
    FdContext * fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!fd_ctx->events){
        return false;
    }

    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt){
        SYLAR_LOG_ERROR(g_logger) << "epoll_ctl" << m_epfd << ", "
                << op << "," << fd << "," << epevent.events << "):"
                << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false; 
    }

    if(fd_ctx->events & READ){
        fd_ctx->triggerEvent(READ);
        --m_pendingEventCount;
    }

    if(fd_ctx->events & WRITE){
        fd_ctx->triggerEvent(WRITE);
        --m_pendingEventCount;
    }

    SYLAR_ASSERT(fd_ctx->events == 0);
    return true;
}

HttpServer* HttpServer::GetThis(){
    return dynamic_cast<HttpServer*>(Scheduler::GetThis());
}

void HttpServer::tickle(){   //
    if(hasIdleThreads()){
        return;
    }
    int rt = write(m_tickleFds[1], "T", 1);
    SYLAR_ASSERT(rt == 1); 
}

bool HttpServer::stopping(uint64_t & timeout){
    timeout = getNextTimer();
    return timeout == ~0ull
        && m_pendingEventCount == 0
        && Scheduler::stopping();
}

bool HttpServer::stopping(){  //
    uint64_t timeout = 0;
    return stopping(timeout);
}


void HttpServer::onTimerInsertedAtFront(){
    tickle();
}




}