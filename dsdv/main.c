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
    if (fd < 0) {
        std::cerr << "create socket error" << std::endl;
        return -1;
    }

    struct sockaddr_in sin;
    memset((char *)&sin, 0, sizeof(struct sockaddr_in));

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
        std::cerr << "bind fd error" << std::endl;
        close(fd);
        return -1;
    }
    
    return fd;
}

void socketSend(int fd, int port, const std::string &str) {
    char buf[BUFLEN];
    memset(buf, 0, BUFLEN);
    strcpy(buf, str.c_str());
    //std::cout << "socketSend fd " << fd << " port " << port << " buf " << buf << " size " << strlen(buf) << std::endl;

    struct sockaddr_in sin;
    memset((char *)&sin, 0, sizeof(struct sockaddr_in));

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (sendto(fd, buf, BUFLEN - 1, 0, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
        std::cerr << "socket send error" << std::endl;
        exit(0);
    }
}

std::string socketReceive(int fd) {
    //std::cout << "socketReceive fd " << fd << std::endl;
    char buf[BUFLEN];
    memset(buf, 0, BUFLEN);

    struct sockaddr_in sin;
    memset((char *)&sin, 0, sizeof(struct sockaddr_in));
    
    socklen_t len = sizeof(struct sockaddr_in);
    if (recvfrom(fd, buf, BUFLEN - 1, 0, (struct sockaddr *)&sin, &len) <= 0) {
        std::cerr << "socket receive error buf " << buf << std::endl;
        exit(0);
    }

    return std::string(buf);;
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
        sout << name << ' ' << forwardingTable.size() << ' ';
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

    void updateForwardingTable(const std::string &nextHop, const std::map<std::string, class RouteTableItem> &routeTable) {
        auto distance = neighborhood[nextHop].metric;
        std::cout << "========= Receive from " << nextHop << " distance is " << distance << std::endl;
        for (const auto &it : routeTable) {
            if (it.first == name) {
                continue;
            }
            if (forwardingTable.find(it.first) == forwardingTable.end()) {
                std::cout << "Add " << it.first << " with " << (it.second.metric + distance) << std::endl;
                forwardingTable[it.first] = ForwardingTableItem(nextHop, it.second.metric + distance, it.second.seqNum);
            } else {
                if (forwardingTable[it.first].seqNum < it.second.seqNum) {
                    std::cout << "SeqNum " << forwardingTable[it.first].seqNum << " < " << it.second.seqNum;
                    std::cout << " Update " << it.first << " from " << forwardingTable[it.first].metric << " to " << (it.second.metric + distance) << std::endl;
                    forwardingTable[it.first] = ForwardingTableItem(nextHop, it.second.metric + distance, it.second.seqNum);
                } else if ((forwardingTable[it.first].seqNum == it.second.seqNum) &&
                            (forwardingTable[it.first].metric > (it.second.metric + distance))) {
                    std::cout << "Change " << it.first << " from " << forwardingTable[it.first].metric << " to " << (it.second.metric + distance) << std::endl;
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
            if (neighborhood[neighborName].metric < MAX) {
                if (neighborMetric < 0) {
                    //++forwardingTable[neighborName].seqNum;
                    neighborMetric = MAX;
                }
                if (neighborhood[neighborName].metric != neighborMetric) {
                    flag = true;
                    neighborhood[neighborName] = NeighborInfo(neighborMetric, neighborPort);
                    if (forwardingTable.find(neighborName) != forwardingTable.end()) {
                        forwardingTable[neighborName].metric = neighborMetric;
                        for (auto &it : forwardingTable) {
                            if (it.second.nextHop == neighborName) {
                                it.second.metric = MAX;   
                            }
                        }
                    }
                }
            } else {
                if (neighborMetric >= 0) {
                    flag = true;
                    //++forwardingTable[neighborName].seqNum;
                    neighborhood[neighborName] = NeighborInfo(neighborMetric, neighborPort);
                }
            }
        }

        if (flag) {
            forwardingTable[name].seqNum += 2;
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
        host->seqNum += 2;
        auto packet = host->serialize();
        mutex.unlock();
        host->printOut();
        for (const auto &it : host->neighborhood) {
            //std::cout << host->name << " is sending to port " << it.second.port << std::endl;
            if (it.second.metric < MAX) {
                socketSend(fd, it.second.port, packet);
            }
        }
        sleep(5);
    }
}

void receiving(int fd, MobileHost *host) {
    for (;;) {
        std::string nextHop;
        auto routeTable =  host->deserialize(socketReceive(fd), nextHop);
        mutex.lock();
        host->updateForwardingTable(nextHop, routeTable);
        mutex.unlock();
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
    //std::cout << "init fd " << fd << " port " << port << std::endl;
    std::thread sender(sending, fd, &host, filename);
    std::thread receiver(receiving, fd, &host);
    sender.join();
    receiver.join();
    close(fd);

    return 0;
}

//#endif
