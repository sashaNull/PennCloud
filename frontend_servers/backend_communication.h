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

std::string receive_one_message(int fd, const std::string& buffer, const unsigned int BUFFER_SIZE);

F_2_B_Message send_and_receive_msg(int fd, const std::string &addr_str, F_2_B_Message msg);

F_2_B_Message construct_msg(int type, const std::string &rowkey, const std::string &colkey,const std::string &value, const std::string &value2, const std::string &errmsg, int status);

std::string ask_coordinator(sockaddr_in coordinator_addr, const std::string &rowkey, const std::string &type);

bool check_backend_connection(int fd, const std::string &backend_serveraddr_str, const std::string &rowkey, const std::string &colkey);

std::string get_backend_server_addr(int fd, const std::string &rowkey, const std::string &colkey, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr, const std::string &type);

int send_msg_to_backend(int fd, F_2_B_Message msg_to_send, std::string &value, int &status, std::string &err_msg,
                        const std::string &rowkey, const std::string &colkey, 
                        std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr,
                        const std::string &type);

#endif // BACKEND_COMMUNICATION_H
