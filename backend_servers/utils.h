#ifndef UTILS_FUNCTIONS_H
#define UTILS_FUNCTIONS_H

#include "../utils/utils.h"
#include <vector>
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <regex>
#include <dirent.h>

extern const int MAX_BUFFER_SIZE;

struct fileRange
{
    std::string range_start;
    std::string range_end;
    std::string filename;
};

// Declare function prototypes
F_2_B_Message handle_get(F_2_B_Message message, std::string data_file_location);
F_2_B_Message handle_put(F_2_B_Message message, std::string data_file_location);
F_2_B_Message handle_cput(F_2_B_Message message, std::string data_file_location);
F_2_B_Message handle_delete(F_2_B_Message message, std::string data_file_location);

// Function prototype for reading data from client socket
bool do_read(int client_fd, char *client_buf);

void createPrefixToFileMap(const std::string &directory_path, std::map<std::string, fileRange> &prefix_to_file);

#endif // UTILS_FUNCTIONS_H