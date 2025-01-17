#ifndef UTILS_H
#define UTILS_H

#include <cctype>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <sys/file.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <openssl/md5.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

struct F_2_B_Message
{
    int type;
    std::string rowkey;
    std::string colkey;
    std::string value;
    std::string value2;
    int status;
    int isFromPrimary;
    std::string errorMessage;
};

bool filepath_is_valid(std::string filepath);

bool fd_is_open(int fd);

F_2_B_Message decode_message(const std::string &serialized);

std::string encode_message(F_2_B_Message f2b_message);

void print_message(const F_2_B_Message &message);

std::vector<std::string> split(const std::string &s, const std::string &delimiter = " ");

std::string strip(const std::string &str, const std::string &chars = " \t\n\r\f\v");

std::map<std::string, std::string> parse_json_string_to_map(const std::string json);

sockaddr_in get_socket_address(const std::string &addr_str);

std::string compute_md5_hash(const std::string& to_hash);

std::string lower_case(const std::string& str);

std::string base64_encode(const std::string &data);

std::string base64_decode(const std::string &encoded_data);

#endif // UTILS_H
