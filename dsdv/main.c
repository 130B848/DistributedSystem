//#ifndef UTIL_H
//#define UTIL_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <pthread.h>
#include <mutex>

std::mutex mutex;
const int BUFLEN = 2048;

int socketBind(int port) {
    auto fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        std::cerr << "create socket error" << std::endl;
        return -1;
    }

    struct sockaddr_in sin;
    memset((char *)&sin, 0, sizeof(struct sockaddr_in));

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == -1) {
        std::cerr << "bind fd error" << std::endl;
        close(fd);
        return -1;
    }
    
    return fd;
}

int socketSend(int fd, int port, const std::string &str) {
    char buf[BUFLEN];
    memset(buf, 0, BUFLEN);

    struct sockaddr_in sin;
    memset((char *)&sin, 0, sizeof(struct sockaddr_in));

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == -1) {
        std::cerr << "socket send error" << std::endl;
        return -1;
    }
    return 0;
}

int socketReceive(int fd, std::string &str) {
    char buf[BUFLEN];
    memset(buf, 0, BUFLEN);

    struct sockaddr_in sin;
    memset((char *)&sin, 0, sizeof(struct sockaddr_in));

    socklen_t len = sizeof(sin);
    if (recvfrom(fd, buf, strlen(buf), 0, (struct sockaddr *)&sin, &len) <= 0) {
        std::cerr << "socket receive error" << std::endl;
        return -1;
    }

    str = std::string(buf);
    return ntohs(sin.sin_port);
}

const int MAX = 10000;

class ForwardingTableItem {
public:
    //std::string destination;
    std::string nextHop;
    double metric;
    int seqNum;

    ForwardingTableItem() = default;
    ForwardingTableItem(std::string n, double m, int s) : nextHop(n), metric(m), seqNum(s) {}
};

class RouteTableItem {
public:
    //std::string destination;
    double metric;
    int seqNum;
    
    RouteTableItem() = default;
    RouteTableItem(double m, int s) : metric(m), seqNum(s) {}
};

class NeighborInfo {
public:
    //std::string name;
    double metric;
    int port;

    NeighborInfo() = default;
    NeighborInfo(double m, int p) : metric(m), port(p) {}
};

class MobileHost {
public:
    std::string name;
    int port;
    int seqNum;
    // <key, value> ==> <name, port>
    std::map<std::string, class NeighborInfo> neighborhood;
    // <key, value> ==> <name, info>
    std::map<std::string, class ForwardingTableItem> forwardingTable;

    MobileHost(std::string n, int p) : name(n), port(p), seqNum(0) {}

    std::string serialize() {
        std::ostringstream sout;
       
        // nextHop -- lines
        sout << name << ' ' << neighborhood.size() << ' ';
        for (const auto &it : forwardingTable) {
            // destination -- metric -- seqNum
            sout << it.first << ' ' << it.second.metric << ' ' << it.second.seqNum << ' ';
        }
        
        return sout.str();
    }

    std::map<std::string, class RouteTableItem> deserialize(const std::string &str, std::string &nextHop) {
        std::map<std::string, class RouteTableItem> ret;
        std::string destination;
        int sequence, lines;
        double metric;
        std::istringstream sin(str);
        
        sin >> nextHop >> lines;
        for (auto i = 0; i < lines; ++i) {
            sin >> destination >> metric >> sequence;
            ret[destination] = RouteTableItem(metric, sequence);
        }
        
        return ret;
    }

    void updateForwardingTable(const std::string &nextHop, std::map<std::string, const class RouteTableItem> &routeTable) {
        auto distance = neighborhood[nextHop].metric;
        for (const auto &it : routeTable) {
            if (forwardingTable.find(it.first) == forwardingTable.end()) {
                forwardingTable[it.first] = ForwardingTableItem(nextHop, it.second.metric + distance, it.second.seqNum);
            } else {
                if (forwardingTable[it.first].seqNum < it.second.seqNum) {
                    forwardingTable[it.first] = ForwardingTableItem(nextHop, it.second.metric + distance, it.second.seqNum);
                } else if ((forwardingTable[it.first].seqNum == it.second.seqNum) &&
                            (forwardingTable[it.first].metric > (it.second.metric + distance))) {
                    forwardingTable[it.first] = ForwardingTableItem(nextHop, it.second.metric + distance, it.second.seqNum);
                }
            }
        }
    }

    bool refreshNeighborInfo(const std::string &filename) {
        std::ifstream fin(filename.c_str(), std::ifstream::in);
        if (!fin.good()) {
            fin.close();
            exit(0);
        }

        bool flag = false;
        int lines;
        std::string name;
        fin >> lines >> name;

        for (auto i = 0; i < lines; ++i) {
            std::string neighborName;
            double neighborMetric;
            int neighborPort;
            fin >> neighborName >> neighborMetric >> neighborPort;
            //change forwardingTable only if it is in the table as neighbor
            if (forwardingTable[neighborName].nextHop == neighborName) {
                if (neighborMetric < 0) {
                    neighborMetric = MAX;
                    ++forwardingTable[neighborName].seqNum;
                }
                //forwardingTable[neighborName].metric = neighborMetric;
            }
            neighborhood[neighborName] = NeighborInfo(neighborMetric, neighborPort);
        }

        fin.close();
        return flag;
    }

    void printOut() {
        std::cout << "## print-out number " << seqNum << std::endl;
        for (const auto &it : forwardingTable) {
            std::cout << "shortest path to node " << it.first << " (seq# " << it.second.seqNum << "): the next hop is "
                << it.second.nextHop << " and the cost is " << it.second.metric << ", " << name << " -> " << it.first
                << " : " << it.second.metric << std::endl;
        }
    }
};

void sending(int fd, MobileHost *host, std::string filename) {
    for (;;) {
        mutex.lock();
        host->refreshNeighborInfo(filename);
        host->forwardingTable[host->name].seqNum += 2;
        mutex.unlock();
        std::string packet = host->serialize();
        for (const auto &it : host->neighborhood) {
            socketSend(fd, it.second.port, packet);
        }
        sleep(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cout << "usage: " << argv[0] << " <port> <filename>" << std::endl;
        exit(0);
    }

    auto port = atoi(argv[1]);
    if (port <= 0) {
        std::cout << "invalid port number" << std::endl;
        exit(0);
    }
    
    std::string filename(argv[2]);
    std::ifstream fin(filename.c_str(), std::ifstream::in);
    if (!fin.good()) {
        fin.close();
        exit(0);
    }

    int lines;
    std::string name;
    fin >> lines >> name;
    MobileHost host(name, port);
    host.forwardingTable[name] = ForwardingTableItem(name, 0, 0);

    for (auto i = 0; i < lines; ++i) {
        std::string neighborName;
        double neighborMetric;
        int neighborPort;
        fin >> neighborName >> neighborMetric >> neighborPort;
        if (neighborMetric < 0) {
            neighborMetric = MAX;
        }
        host.neighborhood[neighborName] = NeighborInfo(neighborMetric, neighborPort);
    }

    fin.close();

    auto fd = socketBind(port);
    std::thread sender(sending, fd, &host, filename);
    //std::thread receiver(receive, fd, host);
    sender.join();
    //receiver.join();
    close(fd);

    return 0;
}

//#endif
