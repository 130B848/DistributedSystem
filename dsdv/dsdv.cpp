#include "dsdv.h"
    
std::string MobileHost::serialize() {
    std::ostringstream sout;

    // nextHop -- lines
    sout << name << ' ' << forwardingTable.size() << ' ';
    for (const auto &it : forwardingTable) {
        // destination -- metric -- seqNum
        sout << it.first << ' ' << it.second.metric << ' ' << it.second.seqNum << ' ';
    }

    return sout.str();
}

std::map<std::string, class RouteTableItem> MobileHost::deserialize(const std::string &str, std::string &nextHop) {
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

void MobileHost::updateForwardingTable(const std::string &nextHop, const std::map<std::string, 
        class RouteTableItem> &routeTable) {
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

bool MobileHost::refreshNeighborInfo(const std::string &filename) {
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
                ++forwardingTable[neighborName].seqNum;
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

void MobileHost::printOut() {
    std::cout << "## print-out number " << seqNum << std::endl;
    for (const auto &it : forwardingTable) {
        std::cout << "shortest path to node " << it.first << " (seq# " << it.second.seqNum << "): the next hop is "
            << it.second.nextHop << " and the cost is " << it.second.metric << ", " << name << " -> " << it.first
            << " : " << it.second.metric << std::endl;
    }
}
