#ifndef BACKEND_COMMUNICATION_H
#define BACKEND_COMMUNICATION_H

#include <string>
#include <vector>
#include <iostream>
#include <cstdlib> // for EXIT_FAILURE
#include <sys/socket.h> // for socket() and related constants
#include <netinet/in.h>
#include "../utils/utils.h"


// Declaration of function to create a new socket and return its file descriptor.
int create_socket();

void send_message(int fd, const std::string &to_send);

std::string receive_one_message(int fd, std::string& buffer, const unsigned int BUFFER_SIZE);

F_2_B_Message send_and_receive_msg(int fd, const std::string &addr_str, F_2_B_Message msg);

F_2_B_Message construct_msg(int type, const std::string &rowkey, const std::string &colkey,const std::string &value, const std::string &value2, const std::string &errmsg, int status);

std::string ask_coordinator(int fd, sockaddr_in coordinator_addr, const std::string &rowkey, int type);

#endif // BACKEND_COMMUNICATION_H
