#ifndef __SYLAR_ADDRESS_H__
#define __SYLAR_ADDRESS_H__

#include <memory>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <sys/un.h>
#include <stddef.h>
#include <map>
#include <vector>

namespace sherry{

class IPAddress;    
class Address{
public:
    typedef std::shared_ptr<Address> ptr;

    static Address::ptr Create(const sockaddr * addr, socklen_t addrlen);
    static bool Lookup(std::vector<Address::ptr> & result, const std::string & host,
                                int family = AF_INET, int type = 0, int protocol = 0);
    static Address::ptr LookupAny(const std::string & host,
                                int family = AF_INET, int type = 0, int protocol = 0);
    static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string & host,
                                int family = AF_INET, int type = 0, int protocol = 0);
    
    static bool GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t> > & result,
                                int family = AF_INET);
    
    static bool GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t> > & result,
                                const std::string & iface, int family = AF_UNSPEC);
    
    virtual ~Address() {}

    int getFamily() const ;

    virtual const sockaddr* getAddr() const = 0;
    virtual sockaddr * getAddr() = 0;
    virtual socklen_t getAddrLen() const = 0;
    
    virtual std::ostream& insert(std::ostream& os) const = 0;
    std::string toString();

    bool operator<(const Address& rhs) const;
    bool operator==(const Address& rhs) const;
    bool operator!=(const Address& rhs) const;

};

class IPAddress : public Address{
public:
    typedef std::shared_ptr<IPAddress> ptr;

    static IPAddress::ptr Create(const char * address, uint32_t port = 0);

    virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;
    virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;
    virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;

    virtual uint32_t getPort() const = 0;
    virtual void setPort(uint16_t v) = 0;
};

class IPv4Address : public IPAddress{
public:
    typedef std::shared_ptr<IPv4Address> ptr;

    static IPv4Address::ptr Create(const char * address, uint32_t port = 0);

    IPv4Address(const sockaddr_in & address);
    IPv4Address(uint32_t address = INADDR_ANY, uint32_t port = 0);

    const sockaddr * getAddr() const override;
    sockaddr * getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;

    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void setPort(uint16_t v) override;
private:
    sockaddr_in m_addr;
};

class IPv6Address : public IPAddress{
public:
    typedef std::shared_ptr<IPv6Address> ptr;
    static IPv6Address::ptr Create(const char * address, uint32_t port = 0);

    IPv6Address();
    IPv6Address(const sockaddr_in6 & address);
    IPv6Address(const uint8_t address[16], uint32_t port = 0);

    const sockaddr * getAddr() const override;
    sockaddr * getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;

    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void setPort(uint16_t v) override;
private:
    sockaddr_in6 m_addr;
};

class UnixAddress : public Address{
public:
    typedef std::shared_ptr<UnixAddress> ptr;
    UnixAddress(const std::string & path);
    UnixAddress();

    const sockaddr * getAddr() const override;
    sockaddr * getAddr() override;
    socklen_t getAddrLen() const override;
    void setAddrLen(uint32_t v);
    std::ostream& insert(std::ostream& os) const override;
private:
    sockaddr_un m_addr;
    socklen_t m_length;
};

class UnknownAddress : public Address{
public:
    typedef std::shared_ptr<UnknownAddress> ptr;
    UnknownAddress(int family);
    UnknownAddress(const sockaddr & address);
    const sockaddr * getAddr() const override;
    sockaddr * getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;
private:
    sockaddr m_addr;
};

class MqttAddress{
public:
    typedef std::shared_ptr<MqttAddress> ptr;

    MqttAddress(const std::string& protocol, int port, const std::string& host);
    ~MqttAddress(){};

    std::string getAddr() const {return m_MqttAddress;}
    std::string getProtocol() const {return m_protocol;}
    std::string getHost() const {return m_host;}
    int getPort() const {return m_port;}

    void setAddr(const std::string& protocol, int port, const std::string& host);
    void setProtocol(const std::string& protocol);
    void setHost(const std::string& host);
    void setPort(int port); 

    void updateAddr();

private:
    int m_port;
    std::string m_protocol;
    std::string m_host;
    std::string m_MqttAddress;
};

}

#endif