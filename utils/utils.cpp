#include "utils.h"

using namespace std;

bool filepath_is_valid(string filepath)
{
  FILE *file = fopen(filepath.c_str(), "r");
  if (file)
  {
    fclose(file);
    return true;
  }
  else
  {
    return false;
  }
}

bool fd_is_open(int fd)
{
  return fcntl(fd, F_GETFD) != -1;
}

F_2_B_Message decode_message(const string &serialized)
{
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

  getline(iss, token, '|');
  message.isFromBackend = stoi(token);

  // errorMessage might contain '|' characters, but since it's the last field,
  // we use the remainder of the string.
  getline(iss, message.errorMessage);

  return message;
}

std::string encode_message(F_2_B_Message f2b_message)
{
  ostringstream oss;
  oss << f2b_message.type << "|" << f2b_message.rowkey << "|"
      << f2b_message.colkey << "|" << f2b_message.value << "|"
      << f2b_message.value2 << "|" << f2b_message.status << "|" << f2b_message.isFromBackend << "|"
      << f2b_message.errorMessage << "\r\n";
  string serialized = oss.str();
  return serialized;
}

void print_message(const F_2_B_Message &message)
{
  std::cout << "Type: " << message.type << std::endl;
  std::cout << "Rowkey: " << message.rowkey << std::endl;
  std::cout << "Colkey: " << message.colkey << std::endl;
  std::cout << "Value: " << message.value << std::endl;
  std::cout << "Value2: " << message.value2 << std::endl;
  std::cout << "Status: " << message.status << std::endl;
  std::cout << "From Backend: " << message.isFromBackend << std::endl;
  std::cout << "ErrorMessage: " << message.errorMessage << std::endl;
}

std::vector<std::string> split(const std::string &s, const std::string &delimiter)
{
  std::vector<std::string> tokens;
  std::string token;
  size_t start = 0, end = 0;
  if (delimiter == " ")
  {
    std::istringstream stream(s);
    while (stream >> token)
    {
      tokens.push_back(token);
    }
  }
  else
  {
    while ((end = s.find(delimiter, start)) != std::string::npos)
    {
      token = s.substr(start, end - start);
      if (!token.empty())
      {
        tokens.push_back(token);
      }
      start = end + delimiter.length();
    }
    if (start < s.length())
    {
      token = s.substr(start);
      if (!token.empty())
      {
        tokens.push_back(token);
      }
    }
  }
  return tokens;
}

std::string strip(const std::string &str, const std::string &chars)
{
  size_t start = str.find_first_not_of(chars);
  if (start == std::string::npos)
    return "";
  size_t end = str.find_last_not_of(chars);
  return str.substr(start, end - start + 1);
}

std::map<std::string, std::string> parse_json_string_to_map(const std::string json_str)
{
  std::map<std::string, std::string> to_return;
  std::string to_strip = "{}";
  std::string stripped_str = strip(json_str, to_strip);
  std::vector<std::string> pairs = split(stripped_str, ",");
  for (const auto &s : pairs)
  {
    std::vector<std::string> key_value = split(s, ":");
    std::string key = strip(key_value[0], "\"");
    std::string value = strip(key_value[1], "\"");
    to_return[key] = value;
  }
  return to_return;
}

sockaddr_in get_socket_address(const string &addr_str)
{
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  // parse ip and port
  size_t pos = addr_str.find(':');
  string ip_str = addr_str.substr(0, pos);
  int port = stoi(addr_str.substr(pos + 1));
  // set ip
  if (inet_pton(AF_INET, ip_str.c_str(), &addr.sin_addr) <= 0)
  {
    cerr << "Invalid IP address format." << endl;
    return addr;
  }
  // set port
  addr.sin_port = htons(static_cast<uint16_t>(port));
  return addr;
}