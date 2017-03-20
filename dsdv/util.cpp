#include "util.h"

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
