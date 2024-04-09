#include "utils.h"

using namespace std;

bool filepath_is_valid(string filepath) {
  FILE* file = fopen(filepath.c_str(), "r");
  if (file) {
    fclose(file);
    return true;
  } else {
    return false;
  }
}

bool fd_is_open(int fd) {
  return fcntl(fd, F_GETFD) != -1;
}

F_2_B_Message decode_message(const string &serialized) {
  F_2_B_Message message;
  istringstream iss(serialized);
  string token;

  getline(iss, token, '|');
  message.type = stoi(token);

  getline(iss, message.rowkey, '|');
  getline(iss, message.colkey, '|');
  getline(iss, message.value, '|');
  getline(iss, message.value2, '|');

  getline(iss, token, '|');
  message.status = stoi(token);

  // errorMessage might contain '|' characters, but since it's the last field,
  // we use the remainder of the string.
  getline(iss, message.errorMessage);

  return message;
}


std::string encode_message(F_2_B_Message f2b_message) {
  ostringstream oss;
  oss << f2b_message.type << "|" << f2b_message.rowkey << "|"
      << f2b_message.colkey << "|" << f2b_message.value << "|"
      << f2b_message.value2 << "|" << f2b_message.status << "|"
      << f2b_message.errorMessage << "\r\n";
  string serialized = oss.str();
  return serialized;
}