#ifndef UTIL_H_
#define UTIL_H_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>

const int BUFLEN = 2048;

int socketBind(int port);

void socketSend(int fd, int port, const std::string &str);

std::string socketReceive(int fd);

#endif
