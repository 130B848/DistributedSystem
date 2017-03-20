#ifndef DSDV_H_
#define DSDV_H_

#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include "util.h"

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

    std::string serialize();

    std::map<std::string, class RouteTableItem> deserialize(const std::string &str, std::string &nextHop);

    void updateForwardingTable(const std::string &nextHop, const std::map<std::string, class RouteTableItem> &routeTable);

    bool refreshNeighborInfo(const std::string &filename);

    void printOut();
};

#endif
