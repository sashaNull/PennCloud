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
#include <sys/stat.h>

extern const int MAX_BUFFER_SIZE;

struct fileRange
{
    std::string range_start;
    std::string range_end;
    std::string filename;
};

struct tablet_data
{
    std::map<std::string, std::map<std::string, std::string>> row_to_kv;
    pthread_mutex_t tablet_lock;
    int requests_since_checkpoint = 0;
};

// Declare function prototypes
F_2_B_Message handle_get(F_2_B_Message message, std::string data_file_location);
F_2_B_Message handle_put(F_2_B_Message message, std::string data_file_location);
F_2_B_Message handle_cput(F_2_B_Message message, std::string data_file_location);
F_2_B_Message handle_delete(F_2_B_Message message, std::string data_file_location);

// Function prototype for reading data from client socket
bool do_read(int client_fd, char *client_buf);

void createPrefixToFileMap(const std::string &directory_path, std::map<std::string, fileRange> &prefix_to_file);

std::string findFileNameInRange(const std::map<std::string, fileRange> &prefix_to_file, const std::string &rowname);

void log_message(const F_2_B_Message &f2b_message, std::string data_file_location, std::string tablet_name);
void checkpoint_tablet(tablet_data &checkpoint_tablet_data, std::string tablet_name, std::string data_file_location);

#endif // UTILS_FUNCTIONS_H