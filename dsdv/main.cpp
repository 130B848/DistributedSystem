#include "util.h"
#include "dsdv.h"

std::mutex mutex;

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
