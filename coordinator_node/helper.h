#include <iostream>
#include <string>
#include <stdio.h>
#include <unistd.h>
#include <fstream>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <vector>

#define HOST "127.0.0.1"
#define PORT 7070

const int MAX_BUFFER_SIZE = 1024;

struct server_info
{
    std::string ip;
    int port;
    bool is_active;
};

void print_server_details(const std::vector<server_info *> &list_of_all_servers,
                          const std::unordered_map<std::string, std::vector<server_info *>> &range_to_server_map);
void populate_list_of_servers(const std::string &config_file_location,
                              std::vector<server_info *> &list_of_all_servers,
                              std::unordered_map<std::string, std::vector<server_info *>> &range_to_server_map);
void *handle_heartbeat(void *arg);
bool do_read(int client_fd, char *client_buf);
std::string get_range_from_rowname(const std::string &rowname);
std::string get_active_server_from_range(const std::unordered_map<std::string, std::vector<server_info *>> &range_to_server_map,
                                         const std::string &range, std::string type);