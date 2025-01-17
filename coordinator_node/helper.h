#include "../utils/utils.h"
#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#define HOST "127.0.0.1"
#define PORT 7070

const int MAX_BUFFER_SIZE = 1024;

struct server_info {
  std::string ip;
  int port;
  bool is_active;
};

bool do_read(int client_fd, char *client_buf);
std::string get_range_from_rowname(
    const std::string &rowname,
    const std::unordered_map<std::string, std::vector<server_info *>>
        &range_to_server_map);
std::string get_active_server_from_range(
    const std::unordered_map<std::string, std::vector<server_info *>>
        &range_to_server_map,
    const std::string &range, const std::string &type,
    std::unordered_map<std::string, server_info *> &range_to_primary_map);
void *handle_heartbeat(void *arg);
void print_server_details(
    const std::vector<server_info *> &list_of_all_servers,
    const std::unordered_map<std::string, std::vector<server_info *>>
        &range_to_server_map);
void populate_list_of_servers(
    const std::string &config_file_location,
    std::vector<server_info *> &list_of_all_servers,
    std::unordered_map<std::string, std::vector<server_info *>>
        &range_to_server_map);
void print_primaries(
    std::unordered_map<std::string, server_info *> &range_to_primary_map);
void update_primary(
    std::unordered_map<std::string, server_info *> &range_to_primary_map,
    std::unordered_map<std::string, std::vector<server_info *>>
        range_to_server_map,
    pthread_mutex_t &map_and_list_mutex);
void initialize_primaries(
    std::unordered_map<std::string, server_info *> &range_to_primary_map,
    std::unordered_map<std::string, std::vector<server_info *>>
        range_to_server_map,
    pthread_mutex_t &map_and_list_mutex);