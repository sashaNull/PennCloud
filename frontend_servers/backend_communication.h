#ifndef BACKEND_COMMUNICATION_H
#define BACKEND_COMMUNICATION_H

#include <string>
#include <iostream>
#include <cstdlib> // for EXIT_FAILURE
#include <sys/socket.h> // for socket() and related constants
#include <netinet/in.h>
#include "../utils/utils.h"


// Declaration of function to create a new socket and return its file descriptor.
int create_socket();

F_2_B_Message send_and_receive_msg(int fd, const std::string &addr_str, F_2_B_Message msg);

F_2_B_Message construct_msg(int type, std::string rowkey, std::string colkey, std::string value, std::string value2, std::string errmsg, int status);

#endif // BACKEND_COMMUNICATION_H
