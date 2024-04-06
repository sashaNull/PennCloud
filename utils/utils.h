#ifndef F_2_B_MESSAGE_H
#define F_2_B_MESSAGE_H

#include <string>

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

#endif // F_2_B_MESSAGE_H
