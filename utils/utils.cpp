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

string encode_message(F_2_B_Message f2b_message)
{
  ostringstream oss;
  oss << f2b_message.type << "|" << f2b_message.rowkey << "|"
      << f2b_message.colkey << "|" << f2b_message.value << "|"
      << f2b_message.value2 << "|" << f2b_message.status << "|"
      << f2b_message.isFromBackend << "|" << f2b_message.errorMessage << "\r\n";
  string serialized = oss.str();
  return serialized;
}

void print_message(const F_2_B_Message &message)
{
  cout << "Type: " << message.type << endl;
  cout << "Rowkey: " << message.rowkey << endl;
  cout << "Colkey: " << message.colkey << endl;
  cout << "Value: " << message.value << endl;
  cout << "Value2: " << message.value2 << endl;
  cout << "Status: " << message.status << endl;
  cout << "From Backend: " << message.isFromBackend << endl;
  cout << "ErrorMessage: " << message.errorMessage << endl;
}

vector<string> split(const string &s, const string &delimiter)
{
  vector<string> tokens;
  string token;
  size_t start = 0, end = 0;
  if (delimiter == " ")
  {
    istringstream stream(s);
    while (stream >> token)
    {
      tokens.push_back(token);
    }
  }
  else
  {
    while ((end = s.find(delimiter, start)) != string::npos)
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

string strip(const string &str, const string &chars)
{
  size_t start = str.find_first_not_of(chars);
  if (start == string::npos)
    return "";
  size_t end = str.find_last_not_of(chars);
  return str.substr(start, end - start + 1);
}

map<string, string> parse_json_string_to_map(const string json)
{
    map<string, string> resultMap;
    size_t i = 0;
    size_t n = json.length();
    auto skipWhitespace = [&]() {
        while (i < n && isspace(json[i])) i++;
    };
    auto extractString = [&]() -> string {
        skipWhitespace();
        if (json[i] != '"') throw runtime_error("Expected '\"'");
        size_t start = ++i;
        while (i < n && json[i] != '"') {
            if (json[i] == '\\') {
                i++;
            }
            i++;
        }
        if (i >= n) throw runtime_error("Unterminated string");
        string result = json.substr(start, i - start);
        i++;
        return result;
    };
    skipWhitespace();
    if (json[i] != '{') throw runtime_error("Expected '{'");
    i++;
    while (true) {
        skipWhitespace();
        if (json[i] == '}') break;
        string key = extractString();
        skipWhitespace();
        if (json[i] != ':') throw runtime_error("Expected ':' after key");
        i++;
        string value = extractString();
        resultMap[key] = value;
        skipWhitespace();
        if (json[i] == ',') {
            i++;
        } else if (json[i] != '}') {
            throw runtime_error("Expected ',' or '}'");
        }
    }
    i++;
    return resultMap;
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

string compute_md5_hash(const string& to_hash) {
  MD5_CTX c;
  MD5_Init(&c);
  MD5_Update(&c, to_hash.c_str(), to_hash.size());
  unsigned char digest_buffer[MD5_DIGEST_LENGTH];
  MD5_Final(digest_buffer, &c);
  // Convert digest to hexadecimal representation
  string message_uid;
  char hex_buffer[3];
  for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
      sprintf(hex_buffer, "%02x", digest_buffer[i]);
      message_uid += hex_buffer;
  }
  return message_uid;
}

string lower_case(const string& str) {
  string result = str;
  transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return tolower(c); });
  return result;
}