#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iostream>

const int MAX = 10000;

class ForwardingTableItem {
public:
    //std::string destination;
    std::string nextHop;
    double metric;
    int seqNum;

    ForwardingTableItem(std::string n, double m, int s) : nextHop(n), metric(m), seqNum(s) {}
};

class RouteTableItem {
public:
    //std::string destination;
    double metric;
    int seqNum;

    RouteTableItem(double m, int s) : metric(m), seqNum(s) {}
};

class MobileHost {
public:
    std::string name;
    int port;
    int seqNum;
    // <key, value> ==> <name, port>
    std::map<std::string, int> neighbor;
    // <key, value> ==> <name, info>
    std::map<std::string, class ForwardingTableItem> forwardingTable;

    MobileHost(std::string n, int p) : name(n), port(p), seqNum(0) {}

    std::string serialize() {
        std::ostringstream sout;
       
        // nextHop -- lines
        sout << name << ' ' << neighbor.size() << ' ';
        for (const auto &it : forwardingTable) {
            // destination -- metric -- seqNum
            sout << it->first << ' ' << (it->second).metric << ' ' << (it->second).seqNum << ' ';
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
        if (!(routeTable[nextHop].seqNum & 0x1) && (seqNum & 0x1)) {
            if (routeTable[this->name].seqNum == seqNum) {
                ++seqNum;
                forwardingTable[nextHop].seqNum = routeTable[nextHop].seqNum; 
            }
            return;
        }
    }
};

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
        host.neighbor[neighborName] = neighborPort;
        host.forwardingTable[neighborName] = ForwardingTableItem(neighborName, neighborMetric, host.seqNum);
    }

    fin.close();



    return 0;
}

#endif
