#ifndef UTILS_FUNCTIONS_H
#define UTILS_FUNCTIONS_H

#include "../utils/utils.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <pthread.h>
#include <regex>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <set>
#include <vector>

#define CHECKPOINT_SIZE 2
#define NUM_SPLITS 2

extern const int MAX_BUFFER_SIZE;
extern const int WELCOME_BUFFER_SIZE;
// Global variables for server configuration and state

struct fileRange
{
  std::string range_start;
  std::string range_end;
  std::string filename;
};

struct tablet_data
{
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      row_to_kv;
  pthread_mutex_t tablet_lock;
  int requests_since_checkpoint = 0;
  int tablet_version;
  tablet_data() : requests_since_checkpoint(0)
  {
    // Initialize the mutex
    pthread_mutex_init(&tablet_lock, NULL);
  }
};

// Declare function prototypes
F_2_B_Message handle_get(F_2_B_Message message, std::string tablet_name,
                         std::unordered_map<std::string, tablet_data> &cache);
F_2_B_Message handle_put(F_2_B_Message message, std::string tablet_name,
                         std::unordered_map<std::string, tablet_data> &cache);
F_2_B_Message handle_cput(F_2_B_Message message, std::string tablet_name,
                          std::unordered_map<std::string, tablet_data> &cache);
F_2_B_Message
handle_delete(F_2_B_Message message, std::string tablet_name,
              std::unordered_map<std::string, tablet_data> &cache);

std::string
get_new_file_name(const std::string &row_key,
                  const std::vector<std::string> &server_tablet_list);

// Function prototype for reading data from client socket
bool do_read(int client_fd, char *client_buf);

void log_message(const F_2_B_Message &f2b_message,
                 std::string data_file_location, std::string tablet_name);
void checkpoint_tablet(tablet_data &checkpoint_tablet_data,
                       std::string tablet_name, std::string data_file_location);

void load_cache(std::unordered_map<std::string, tablet_data> &cache,
                std::string data_file_location);

void recover(std::unordered_map<std::string, tablet_data> &cache,
             std::string &data_file_location,
             std::vector<std::string> &server_tablet_ranges);

void update_server_tablet_ranges(
    std::vector<std::string> &server_tablet_ranges);

#endif