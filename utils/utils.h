#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <sys/file.h>
#include <fstream>
#include <fcntl.h>
#include <sstream>

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

#endif // UTILS_H


