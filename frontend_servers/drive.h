#ifndef DRIVE_H
#define DRIVE_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <netinet/in.h>

// Function prototypes
int delete_file_chunks(int fd, const std::string &rowkey, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

int copyChunks(int fd, const std::string &old_row_key, const std::string &new_row_key, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

#endif // DRIVE_H