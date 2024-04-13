#ifndef UTILS_H
#define UTILS_H

#include <cctype>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <sys/file.h>
#include <fcntl.h>


struct F_2_B_Message
{
    int type;
    std::string rowkey;
    std::string colkey;
    std::string value;
    std::string value2;
    int status;
    std::string errorMessage;
};

bool filepath_is_valid(std::string filepath);

bool fd_is_open(int fd);

F_2_B_Message decode_message(const std::string &serialized);

std::string encode_message(F_2_B_Message f2b_message);

void print_message(const F_2_B_Message &message);

std::vector<std::string> split(const std::string& s, const std::string& delimiter = " ");

std::string strip(const std::string& str, const std::string& chars = " \t\n\r\f\v");

std::map<std::string,std::string> parse_json_string_to_map(const std::string json_str);

#endif // UTILS_H
