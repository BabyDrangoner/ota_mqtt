#include "iomanager.h"
#include "sherry.h"
#include <sys/epoll.h>
#include <fcntl.h>

namespace sherry{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

IOManager::FdContext::EventContext & IOManager::FdContext::getContext(Event event){
    MutexType::Lock lock(mutex);
    if(event == READ){
        return read;
    } else if(event == WRITE){
        return write;
    } else {
        SYLAR_ASSERT2(false, "getContext invalid event");
    }
}

void IOManager::FdContext::resetContext(EventContext & ctx){
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.cb = nullptr;
}

void IOManager::FdContext::triggerEvent(Event event){
    if(event != READ && event != WRITE){
        return;
    }

    MutexType::Lock lock(mutex);
    SYLAR_ASSERT(events & event);
    events = (Event)(events & ~event);

    EventContext& e_ct = getContext(event);
    if(e_ct.cb){
        e_ct.scheduler->schedule(e_ct.cb);
    } else if(e_ct.fiber){
        e_ct.scheduler->schedule(e_ct.fiber);
    }

    e_ct.scheduler = nullptr;
}

IOManager::IOManager(size_t threads, bool use_caller, const std::string & name)
    :Scheduler(threads, use_caller, name){
    // 初始化m_epfd
    m_epfd = epoll_create(5000);
    SYLAR_ASSERT(m_epfd > 0);

    // 创建匿名管道，0是读端，1是写端
    // 当别的线程要通知当前IOManager有任务调度时，就往1写字节
    // 而主线程的idle会监听0端，收到字节就被唤醒
    int rt = pipe(m_tickleFds);
    SYLAR_ASSERT(!rt);

    // 监听读端，边沿触发
    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_tickleFds[0];

    rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    SYLAR_ASSERT(!rt);

    // 将读端设置为非阻塞
    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
    SYLAR_ASSERT(!rt);

    // 扩容
    contextResize(32);

    // 开启scheduler
    start();
    
}

IOManager::~IOManager(){
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

void IOManager::contextResize(size_t size){
    if(m_fdContexts.size() >= size){
        return;
    }
    RWMutexType::WriteLock lock(m_mutex);
    m_fdContexts.resize(size);
    for(size_t i = 0;i < m_fdContexts.size();++i){
        if(!m_fdContexts[i]){
            m_fdContexts[i] = new IOManager::FdContext();
        }
    }
}

int IOManager::addEvent(int fd, Event event, std::function<void()> cb){
    if(event != READ && event != WRITE){
        return 1;
    }

    if(m_fdContexts.size() <= (size_t)fd){
        contextResize((int)(fd * 1.5)); 
    }

    {
        FdContext* f_ctx = m_fdContexts[fd];
        if(!f_ctx){
            return 1;
        }

        FdContext::MutexType::Lock lock(f_ctx->mutex);
        f_ctx->fd = fd;

        if(f_ctx->events & event){
            return -1;
        }
        
        FdContext::EventContext& f_ev = f_ctx->getContext(event);
        SYLAR_ASSERT2(this, "iomanager is nullptr");
        f_ev.scheduler = Scheduler::GetThis();
        f_ev.cb = cb;
        f_ev.fiber = Fiber::GetThis();

        epoll_event epoll_evt;
        memset(&epoll_evt, 0, sizeof(epoll_event));
        if(event & READ){
            epoll_evt.events = EPOLLIN;
        } else if(event & WRITE){
            epoll_evt.events = EPOLLOUT;
        }
        epoll_evt.events |= EPOLLET;
        epoll_evt.data.fd = f_ctx->fd;

        int rt = -1;
        if(f_ctx->events == NONE){
            rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &epoll_evt);
            if(rt != 0){
                return 0; 
            }
            ++m_pendingEventCount;
            f_ctx->events = (Event)(f_ctx->events & event);
        } else {
            rt = epoll_ctl(m_epfd, EPOLL_CTL_MOD, fd, &epoll_evt);
            if(rt != 0){
                return 0; 
            }
            ++m_pendingEventCount;
            f_ctx->events = (Event)(f_ctx->events | event);
        }
    }    
    return 1;

}

bool IOManager::delEvent(int fd, Event event){
    if(m_fdContexts.size() <= (size_t)fd){
        return true;
    }

    if(event != READ && event != WRITE){
        return true;
    }
    
    {
        FdContext* f_ctx = m_fdContexts[fd];
        if(!f_ctx){
            return true;
        }

        FdContext::MutexType::Lock lock(f_ctx->mutex);
        if(!(f_ctx->events & event)){
            return true;
        }

        f_ctx->events = (Event)(f_ctx->events & ~event);

        epoll_event epoll_evt;
        memset(&epoll_evt, 0, sizeof(epoll_event));
        if(f_ctx->events & READ){
            epoll_evt.events |= EPOLLIN;
        } else if(f_ctx->events & WRITE){
            epoll_evt.events |= EPOLLOUT;
        }
        epoll_evt.events |= EPOLLET;
        epoll_evt.data.fd = f_ctx->fd;

        int op = (f_ctx->events == NONE) ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
        int rt = epoll_ctl(m_epfd, op, f_ctx->fd, &epoll_evt);
        if(rt != 0){
            return false;
        }

        f_ctx->resetContext(f_ctx->getContext(event));
        --m_pendingEventCount;
    }

    return true;

}

bool IOManager::cancelAll(int fd){
    if(m_fdContexts.size() <= (size_t)fd){
        return true;
    }

    {
        FdContext* f_ctx = m_fdContexts[fd];
        if(!f_ctx){
            return true;
        }

        
        FdContext::MutexType::Lock lock(f_ctx->mutex);
        if(f_ctx->events == NONE){
            return true;
        }

        int event_num = 0;
        if(f_ctx->events & READ){
            ++event_num;
            f_ctx->triggerEvent(READ);
        }
        if(f_ctx->events & WRITE){
            ++event_num;
            f_ctx->triggerEvent(WRITE);
        }

        int rt = epoll_ctl(m_epfd, EPOLL_CTL_DEL, f_ctx->fd, nullptr);
        if(rt != 0){
            return false;
        }

        m_pendingEventCount -= event_num;
        f_ctx->events = NONE;
        f_ctx->read.scheduler = nullptr;
        f_ctx->write.scheduler = nullptr;

        
    }
    return true;
}


bool IOManager::cancelEvent(int fd, Event event){
    if(m_fdContexts.size() <= (size_t)fd){
        return true;
    }

    if(event != READ && event != WRITE){
        return true;
    }

    {
        FdContext* f_ctx = m_fdContexts[fd];
        if(!f_ctx){
            return true;
        }

        FdContext::MutexType::Lock lock(f_ctx->mutex);

        if(!(f_ctx->events & event)){
            return true;
        }

        epoll_event epoll_evt;
        memset(&epoll_evt, 0, sizeof(epoll_event));
        if(f_ctx->events & READ){
            epoll_evt.events |= EPOLLIN;
        } else if(f_ctx->events & WRITE){
            epoll_evt.events |= EPOLLOUT;
        }
        epoll_evt.events |= EPOLLET;

        int op = (f_ctx->events == NONE) ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
        int rt = epoll_ctl(m_epfd, op, f_ctx->fd, &epoll_evt);
        if(rt != 0){
            return false;
        }

        --m_pendingEventCount;

        f_ctx->triggerEvent(event);
    }

    return true;
}

IOManager* IOManager::GetThis(){
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle(){
    if(hasIdleThreads()){
        return;
    }
    int rt = write(m_tickleFds[1], "T", 1);
    SYLAR_ASSERT(rt == 1);
}

bool IOManager::stopping(uint64_t& timeout){
    timeout = getNextTimer();
    return timeout == ~0ull
        && m_pendingEventCount == 0
        && Scheduler::stopping();
}

bool IOManager::stopping(){
    uint64_t timeout = 0;
    return stopping(timeout);
}

void IOManager::idle(){
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

        std::vector<std::function<void()>> cbs;
        listExpiredCb(cbs);
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
            FdContext * fd_ctx = (FdContext*)event.data.ptr;
            FdContext::MutexType::Lock lock(fd_ctx->mutex);
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

void IOManager::onTimerInsertedAtFront(){
    tickle();
}

}