#include "utils.h"

using namespace std;

static const string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

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
  message.isFromPrimary = stoi(token);

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
      << f2b_message.isFromPrimary << "|" << f2b_message.errorMessage << "\r\n";
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
  cout << "From Backend: " << message.isFromPrimary << endl;
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
  map<string, string> result_map;
  size_t index = 0;
  size_t length = json.length();
  
  auto skip_whitespace = [&]() {
      while (index < length && isspace(json[index])) index++;
  };

  auto extract_string = [&]() -> string {
    skip_whitespace();
    if (json[index] != '"') throw runtime_error("Expected '\"'");
    size_t start = ++index;
    string result;
    while (index < length && json[index] != '"') {
      if (json[index] == '\\') {
        index++;
        if (index >= length) throw runtime_error("Escape sequence not terminated");
        switch (json[index]) {
          case 'n': result += '\n'; break;
          case 't': result += '\t'; break;
          case 'r': result += '\r'; break;
          case '\\': result += '\\'; break;
          case '"': result += '"'; break;
          default: throw runtime_error("Invalid escape sequence");
        }
      } else {
        result += json[index];
      }
      index++;
    }
    if (index >= length) throw runtime_error("Unterminated string");
    index++;
    return result;
  };

  skip_whitespace();
  if (json[index] != '{') throw runtime_error("Expected '{'");
  index++;
  while (true) {
      skip_whitespace();
      if (json[index] == '}') break;
      string key = extract_string();
      skip_whitespace();
      if (json[index] != ':') throw runtime_error("Expected ':' after key");
      index++;
      string value = extract_string();
      result_map[key] = value;
      skip_whitespace();
      if (json[index] == ',') {
          index++;
      } else if (json[index] != '}') {
          throw runtime_error("Expected ',' or '}'");
      }
  }
  index++;
  return result_map;
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

string base_64_encode(const unsigned char* buf, unsigned int bufLen) {
  string base64;
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  while (bufLen--) {
    char_array_3[i++] = *(buf++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for(i = 0; (i <4) ; i++)
        base64 += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i)
  {
    for(j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

    for (j = 0; (j < i + 1); j++)
      base64 += base64_chars[char_array_4[j]];

    while((i++ < 3))
      base64 += '=';
  }

  return base64;
}

string base_64_decode(const string& encoded_string) {
    auto base64_char_value = [](char c) -> int {
      if (c >= 'A' && c <= 'Z') return c - 'A';
      if (c >= 'a' && c <= 'z') return c - 'a' + 26;
      if (c >= '0' && c <= '9') return c - '0' + 52;
      if (c == '+') return 62;
      if (c == '/') return 63;
      return -1;
    };

    int length = encoded_string.size();
    int i = 0, j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    string ret;

    while (length-- && (encoded_string[in_] != '=') && base64_char_value(encoded_string[in_]) != -1) {
      char_array_4[i++] = encoded_string[in_]; in_++;
      if (i == 4) {
        for (i = 0; i < 4; i++)
          char_array_4[i] = base64_char_value(char_array_4[i]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xF) << 4) + ((char_array_4[2] & 0x3C) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (i = 0; (i < 3); i++)
          ret += char_array_3[i];
        i = 0;
      }
    }

    if (i) {
      for (j = 0; j < i; j++)
        char_array_4[j] = base64_char_value(char_array_4[j]);

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xF) << 4) + ((char_array_4[2] & 0x3C) >> 2);

      for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
    }

    return ret;
}
