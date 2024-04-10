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
// Declare function prototypes
F_2_B_Message handle_get(F_2_B_Message message, std::string data_file_location);
F_2_B_Message handle_put(F_2_B_Message message, std::string data_file_location);
F_2_B_Message handle_cput(F_2_B_Message message, std::string data_file_location);
F_2_B_Message handle_delete(F_2_B_Message message, std::string data_file_location);

#endif // UTILS_FUNCTIONS_H