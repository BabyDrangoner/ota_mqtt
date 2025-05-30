#include "hook.h"
#include "fiber.h"
#include "iomanager.h"
#include <dlfcn.h>
#include "macro.h"
#include "fd_manager.h"
#include "log.h"
#include "config.h"
#include "./http/http_server.h"

namespace sherry{

sherry::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static sherry::ConfigVar<int>::ptr g_tcp_connect_timeout = 
    sherry::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt)
    

void hook_init() {
    static bool is_inited = false;
    if(is_inited) {
        return;
    }
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX);
#undef XX
}

static uint64_t s_connect_timeout = -1;

struct _HookIniter{
    _HookIniter(){
        hook_init(); // 调用hook_init()函数
        // 赋值s_connect_timeout
        s_connect_timeout = g_tcp_connect_timeout->getValue();
        g_tcp_connect_timeout->addListener([](const int & old_value, const int & new_value){
            SYLAR_LOG_INFO(g_logger) << "tcp connect timeout change from "
                                     << old_value << " to " << new_value;
            s_connect_timeout = new_value;
        });
    }
};

static _HookIniter s_hook_initer;

bool is_hook_enable(){
    return t_hook_enable;
}

void set_hook_enable(bool flag){
    t_hook_enable = flag;
}

struct timer_info{
    int cancelled = 0;
};

template<typename OriginFun, typename ... Args>
static ssize_t do_io(int fd, OriginFun fun, const char * hook_fun_name,
        uint32_t event, int timeout_so, Args && ... args){
    // 如果hook没有开启，直接调用原始函数
    if(!sherry::t_hook_enable){
        return fun(fd, std::forward<Args>(args)...);
    }           

    SYLAR_LOG_DEBUG(g_logger) << "do_io<" << hook_fun_name << ">";

    // 获取fd对应的FdCtx
    sherry::FdCtx::ptr ctx = sherry::FdMgr::GetInstance()->get(fd);
    if(!ctx){  // 如果ctx为空，直接调用原始函数
        return fun(fd, std::forward<Args>(args)...);
    }

    // 如果fd已经关闭，设置errno为EBADF
    if(ctx->isClose()){
        errno = EBADF;
        return -1;
    }
    // 如果fd不是套接字或者用户设置了非阻塞，直接调用原始函数
    if(!ctx->isSocket() || ctx->getUserNonblock()){
        return fun(fd, std::forward<Args>(args)...);
    }

    uint64_t to = ctx->getTimeout(timeout_so);
    std::shared_ptr<timer_info> tinfo(new timer_info);
retry:
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    // 如果n == -1 && errno == EINTR，说明是被信号中断，继续调用fun
    while(n == -1 && errno == EINTR){
        n = fun(fd, std::forward<Args>(args)...);
    }
    // 如果n == -1 && errno == EAGAIN，说明是超时，需要调用IOManager的addEvent函数
    if(n == -1 && errno == EAGAIN){
        sherry::HttpServer * iom = sherry::HttpServer::GetThis();
        sherry::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);
        // 如果to不等于-1，说明设置了超时时间，需要添加定时器
        if(to != (uint64_t)-1){
            timer = iom->addConditionTimer(to, [winfo, fd, iom, event](){
                auto t = winfo.lock();
                if(!t || t->cancelled){
                    return;
                }
                t->cancelled = ETIMEDOUT;
                iom->cancelEvent(fd, (sherry::HttpServer::Event)(event));
            }, winfo);
        }
        // int c = 0;
        // uint64_t now = 0;

        int rt = iom->addEvent(fd, (sherry::HttpServer::Event)(event));
        if(rt){
            
            SYLAR_LOG_ERROR(g_logger) << hook_fun_name << " addEvent("
                << fd << ", " << event << ")";
            if(timer){
                timer->cancel();
            }
            return -1;
        } else {
            SYLAR_LOG_DEBUG(g_logger) << "do_io<" << hook_fun_name << ">";
            sherry::Fiber::YieldToHold();
            SYLAR_LOG_DEBUG(g_logger) << "do_io<" << hook_fun_name << ">";
            if(timer){
                timer->cancel();
            }
            if(tinfo->cancelled){
                errno = tinfo->cancelled;
                return -1;
            }

            goto retry;
        }
    }

    return n;
}

extern "C" {
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX

unsigned int sleep(unsigned int seconds){
    if(!sherry::t_hook_enable){
        return sleep_f(seconds);
    }

    sherry::Fiber::ptr fiber = sherry::Fiber::GetThis();
    sherry::HttpServer * iom = sherry::HttpServer::GetThis();
    iom->addTimer(seconds * 1000, std::bind((void(sherry::Scheduler::*)
                    (sherry::Fiber::ptr, int thread))&sherry::HttpServer::schedule, iom, fiber, -1));
    sherry::Fiber::YieldToHold();

    return 0;
}

int usleep(useconds_t usec){
    if(!sherry::t_hook_enable){
        return usleep_f(usec);
    }

    sherry::Fiber::ptr fiber = sherry::Fiber::GetThis();
    sherry::HttpServer * iom = sherry::HttpServer::GetThis();
    // iom->addTimer(usec / 1000, std::bind(&sherry::IOManager::schedule, iom, fiber));
    iom->addTimer(usec / 1000, [iom, fiber]{
        iom->schedule(fiber);
    });
    sherry::Fiber::YieldToHold();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem){
    if(!sherry::t_hook_enable){
        return nanosleep(req, rem);
    }

    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    sherry::Fiber::ptr fiber = sherry::Fiber::GetThis();
    sherry::HttpServer * iom = sherry::HttpServer::GetThis();
    iom->addTimer(timeout_ms / 1000, [iom, fiber](){
        iom->schedule(fiber);
    });
    sherry::Fiber::YieldToHold();
    return 0;
}

int socket(int domain, int type, int protocol){
    if(!sherry::t_hook_enable){
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if(fd == -1){
        return fd;
    }
    sherry::FdMgr::GetInstance()->get(fd, true);
    return fd;
}

int connect_with_timeout(int fd, const struct sockaddr * addr, socklen_t addrlen, uint64_t timeout_ms){
    if(!sherry::t_hook_enable){
        return connect_f(fd, addr, addrlen);
    }
    sherry::FdCtx::ptr ctx = sherry::FdMgr::GetInstance()->get(fd);
    if(!ctx || ctx->isClose()){
        errno = EBADF;
    }

    if(!ctx->isSocket()){
        return connect_f(fd, addr, addrlen);
    }

    if(ctx->getUserNonblock()){
        return connect_f(fd, addr, addrlen);
    }

    int n = connect_f(fd, addr, addrlen);
    if(n == 0){
        return 0;
    } else if(n != -1 || errno != EINPROGRESS){
        return n;
    }

    sherry::HttpServer* iom = sherry::HttpServer::GetThis();
    sherry::Timer::ptr timer;
    std::shared_ptr<timer_info> tinfo(new timer_info);
    std::weak_ptr<timer_info> winfo(tinfo);

    if(timeout_ms != (uint64_t)-1){
        timer = iom->addConditionTimer(timeout_ms, [winfo, fd, iom](){
            auto t = winfo.lock();
            if(!t || t->cancelled){
                return;
            }
            t->cancelled = ETIMEDOUT;
            iom->cancelEvent(fd, sherry::HttpServer::WRITE);
        }, winfo);
    }

    int rt = iom->addEvent(fd, sherry::HttpServer::WRITE);
    if(rt == 0){
        sherry::Fiber::YieldToHold();
        if(timer){
            timer->cancel();
        }
        if(tinfo->cancelled){
            errno = tinfo->cancelled;
            return -1;
        }
    } else {
        if(timer){
            timer->cancel();
        }
        SYLAR_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
    }
    
    int error = 0;
    socklen_t len = sizeof(int);
    if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)){
        return -1;
    }
    if(!error){
        return 0;
    } else {
        errno = error;
        return -1;
    }
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
    return connect_with_timeout(sockfd, addr, addrlen, sherry::s_connect_timeout);
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen){
    int fd = do_io(s, accept_f, "accept", sherry::HttpServer::READ, SO_RCVTIMEO, addr, addrlen);
    if(fd >= 0){
        sherry::FdMgr::GetInstance()->get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void *buf, size_t count){
    return do_io(fd, read_f, "read", sherry::HttpServer::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt){
    return do_io(fd, readv_f, "readv", sherry::HttpServer::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags){
    return do_io(sockfd, recv_f, "recv", sherry::HttpServer::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen){
    return do_io(sockfd, recvfrom_f, "recvfrom", sherry::HttpServer::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags){
    return do_io(sockfd, recvmsg_f, "recvmsg", sherry::HttpServer::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count){
    return do_io(fd, write_f, "write", sherry::HttpServer::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt){
    return do_io(fd, writev_f, "writev", sherry::HttpServer::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void *msg, size_t len, int flags){
    return do_io(s, send_f, "send", sherry::HttpServer::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen){
    return do_io(s, sendto_f, "sendto", sherry::HttpServer::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags){
    return do_io(s, sendmsg_f, "sendmsg", sherry::HttpServer::WRITE, SO_SNDTIMEO, msg, flags);   
}

int close(int fd){
    if(!sherry::t_hook_enable){
        return close_f(fd);
    }
    sherry::FdCtx::ptr ctx = sherry::FdMgr::GetInstance()->get(fd);
    if(ctx){
        auto iom = sherry::HttpServer::GetThis();
        if(iom){
            iom->cancelAll(fd);
        }
        sherry::FdMgr::GetInstance()->del(fd);
    }
    return close_f(fd);
}

int fcntl(int fd, int cmd, .../* arg */){
    va_list va;
    va_start(va, cmd);
    switch(cmd){
        case F_SETFL:{
            int arg = va_arg(va, int);
            va_end(va);
            sherry::FdCtx::ptr ctx = sherry::FdMgr::GetInstance()->get(fd);
            if(!ctx || ctx->isClose() || !ctx->isSocket()){
                return fcntl_f(fd, cmd, arg);
            }
            ctx->setUserNonblock(arg & O_NONBLOCK);
                if(ctx->getSysNonblock()) {
                    arg |= O_NONBLOCK;
                } else {
                    arg &= ~O_NONBLOCK;
                }
            return fcntl_f(fd, cmd, arg);
        }
            break;
        case F_GETFL:{
            va_end(va);
            int arg = fcntl_f(fd, cmd);
            sherry::FdCtx::ptr ctx = sherry::FdMgr::GetInstance()->get(fd);
            if(!ctx || ctx->isClose() || !ctx->isSocket()){
                return arg;
            }
            if(ctx->getUserNonblock()){
                return arg | O_NONBLOCK;
            } else {
                return arg & ~O_NONBLOCK;
            }
        }

        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
        case F_SETPIPE_SZ: {
            int arg = va_arg(va, int);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
            break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
        case F_GETPIPE_SZ: {
            va_end(va);
            return fcntl_f(fd, cmd);
        }
            break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK: {
            struct flock * arg = va_arg(va, struct flock *);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
            break;
        case F_GETOWN_EX: 
        case F_SETOWN_EX:{
            struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
            break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}


int ioctl(int d, unsigned long int request, ...){
    va_list va;
    va_start(va, request);
    void * arg = va_arg(va, void *);
    va_end(va);

    if(FIONBIO == request){
        bool user_nonblock = !!*(int *)arg;
        sherry::FdCtx::ptr ctx = sherry::FdMgr::GetInstance()->get(d);
        if(!ctx || ctx->isClose() || !ctx->isSocket()){
            return ioctl_f(d, request, arg);
        }
        ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen){
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen){
    if(!sherry::t_hook_enable){
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if(level == SOL_SOCKET){
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO){
            sherry::FdCtx::ptr ctx = sherry::FdMgr::GetInstance()->get(sockfd);
        
            if(ctx){
                const timeval * v = (const timeval *)optval;
                ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}



}

}

