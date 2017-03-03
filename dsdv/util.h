#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>
#include <map>

class MobileHost {
public:
    //std::string name;
    int port;
    int seqNum;
    // <key, value> ==> <name, port>
    std::vector<std::string, int> neighbor;
    // <key, value> ==> <name, info>
    std::map<std::string, ForwardingTableItem> forwardingTable;
};

class ForwardingTableItem {
public:
    //std::string destination;
    std::string nextHop;
    double metric;
    int seqNum;
};

class RouteTableItem {
public:
    //std::string destination;
    double metric;
    int seqNum;
};

std::map<std::string, MobileHost> hosts;

void DSDV_init(std::string filename);



#endif
