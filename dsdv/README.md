# Lab2 DSDV
* Stu. ID: 5140309358
* Name: Li Dingji
* E-mail: dj_lee@sjtu.edu.cn
* This README is much easier to read on GitHub. Click
[Here](https://github.com/130B848/DistributedSystem/blob/master/dsdv/README.md)!

## Usage
    $ make
    ......
    $ ./dsdv <port> <filename> # repeat in several windows using different port and file
    ......
    $ make clean
Choose the picture in this lab assignment's PDF as an example. Assuming that there are 6 mobile hosts ( a, b, c, d, e, f ) binding the port from 3031 to 3036 sequentially. Then you need to type the above command for 6 times in 6 separate shell window ( *tmux* is highly recommended ). Each host will print out its own forwarding table information regularly ( default time slice is 10s ).

## Description
My implementation is based on the paper of [DSDV](https://courses.cs.washington.edu/courses/cse461/07wi/lectures/dsdv.pdf). 
* Everytime before broadcasting, the host adds 2 to their own sequence number in the forwarding table.
* If some hosts get unconnected, their neighbors will add 1 to these hosts' sequence number corresponding in neighbors' forwarding table and then do a new round broadcast. This method is viable because if these hosts reconnect in the network, the hosts add 2 to sequence number, which is larger than just add 1, so the reconnected hosts can overwrite the old forwarding information in other hosts. Therefore, the **lastest** information is guaranteed. 
* Each host **merely** maintains the information of its **neighbors** and its own **forwarding table**.
* There are a pair of seralize/deseralize functions to help send/receive the route tables among neighbors.
* In order to ensure the indenpendence of each host, there is **no** global variable except std::mutex for thread safety.
* If you still have any questions about my implementation, please refer to the following documentation or contact me via e-mail.

## Documentation
* util.h
```cpp
// 2KB length of buffer defined for socket buffer size
const int BUFLEN = 2048; 

// create new socket and bind specific port to it, return fd
int socketBind(int port);

// send str to port through file descriptor fd
void socketSend(int fd, int port, const std::string &str);

// receive a string through file descriptor fd
std::string socketReceive(int fd);
```
* dsdv.h
```cpp
// maximum metric/cost between hosts
const int MAX = 10000;

// forwarding table item maintained by each hosts
class ForwardingTableItem {
public:
    std::string nextHop;
    double metric;
    int seqNum;

    ForwardingTableItem() = default;
    ForwardingTableItem(std::string n, double m, int s) : nextHop(n), metric(m), seqNum(s) {}
};

// route table item used to broadcast among neighbors
class RouteTableItem {
public:
    double metric;
    int seqNum;
    
    RouteTableItem() = default;
    RouteTableItem(double m, int s) : metric(m), seqNum(s) {}
};

// neighbors' information, update regularly by reading local files
class NeighborInfo {
public:
    double metric;
    int port;

    NeighborInfo() = default;
    NeighborInfo(double m, int p) : metric(m), port(p) {}
};

// host represents each node
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

    // serialize host's forwarding table, prepare to broadcast
    std::string serialize();

    // deserialize the received messages into route table
    std::map<std::string, class RouteTableItem> deserialize(const std::string &str, std::string &nextHop);

    // update host's forwarding table using received route table
    void updateForwardingTable(const std::string &nextHop, const std::map<std::string, class RouteTableItem> &routeTable);

    // refresh neighborhood information by reading file
    bool refreshNeighborInfo(const std::string &filename);

    // print out current forwarding table
    void printOut();
};
```

* main.cpp
```cpp
// global mutual exclusive variable
std::mutex mutex;

// repeat updating neighbor info and broadcast to neighbors
void sending(int fd, MobileHost *host, std::string filename);

// always listens to the port and receive messages
void receiving(int fd, MobileHost *host);

int main(int argc, char *argv[]) {
    // **************** initialization begin ****************
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
    // **************** initialization end ****************

    auto fd = socketBind(port);
    // create a thread for sending
    std::thread sender(sending, fd, &host, filename);
    // create a thread for receiving
    std::thread receiver(receiving, fd, &host);
    // harvest two threads
    sender.join();
    receiver.join();
    close(fd);

    return 0;
}
```
