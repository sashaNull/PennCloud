#include "utils.h"

using namespace std;

static const string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

/**
 * @brief Checks if a given file path is valid.
 *
 * This function attempts to open the file at the specified path.
 * If the file can be opened, it is valid and the function returns true.
 * Otherwise, it returns false.
 *
 * @param filepath The file path to be checked.
 * @return True if the file path is valid, false otherwise.
 */
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

/**
 * @brief Checks if a file descriptor is open.
 *
 * This function uses the fcntl function to check if the file descriptor is open.
 * It returns true if the file descriptor is valid, otherwise false.
 *
 * @param fd The file descriptor to be checked.
 * @return True if the file descriptor is open, false otherwise.
 */
bool fd_is_open(int fd)
{
  return fcntl(fd, F_GETFD) != -1;
}

/**
 * @brief Decodes a serialized string into an F_2_B_Message structure.
 *
 * This function parses a serialized string to populate an F_2_B_Message
 * structure, extracting fields separated by '|' and converting them to
 * appropriate types.
 *
 * @param serialized The serialized string representing an F_2_B_Message.
 * @return An F_2_B_Message structure with the parsed data.
 */
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

/**
 * @brief Encodes an F_2_B_Message structure into a serialized string.
 *
 * This function converts an F_2_B_Message structure into a serialized string,
 * joining fields with '|' and appending "\r\n" at the end.
 *
 * @param f2b_message The F_2_B_Message structure to be encoded.
 * @return A serialized string representing the F_2_B_Message.
 */
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

/**
 * @brief Prints the details of an F_2_B_Message structure to the console.
 *
 * This function outputs each field of the given F_2_B_Message structure
 * to the console for debugging purposes.
 *
 * @param message The F_2_B_Message structure to be printed.
 */
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

/**
 * @brief Splits a string into a vector of tokens based on a delimiter.
 *
 * This function splits the given string by the specified delimiter and
 * returns a vector of tokens. It supports splitting by spaces and other
 * delimiters.
 *
 * @param s The string to be split.
 * @param delimiter The delimiter to use for splitting.
 * @return A vector of strings containing the split tokens.
 */
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

/**
 * @brief Strips specified characters from the start and end of a string.
 *
 * This function removes any leading and trailing characters from the given
 * string that match those specified in the chars parameter.
 *
 * @param str The string to be stripped.
 * @param chars The characters to remove from the string.
 * @return A string with the specified characters stripped from the start and end.
 */
string strip(const string &str, const string &chars)
{
  size_t start = str.find_first_not_of(chars);
  if (start == string::npos)
    return "";
  size_t end = str.find_last_not_of(chars);
  return str.substr(start, end - start + 1);
}

/**
 * @brief Parses a JSON-like string into a map of key-value pairs.
 *
 * This function extracts key-value pairs from a JSON-like string and stores
 * them in a map. It handles escaped characters and ensures correct parsing
 * of strings.
 *
 * @param json The JSON-like string to be parsed.
 * @return A map containing the parsed key-value pairs.
 */
map<string, string> parse_json_string_to_map(const string json)
{
  map<string, string> result_map;
  size_t index = 0;
  size_t length = json.length();

  auto skip_whitespace = [&]()
  {
    while (index < length && isspace(json[index]))
      index++;
  };

  auto extract_string = [&]() -> string
  {
    skip_whitespace();
    if (json[index] != '"')
      throw runtime_error("Expected '\"'");
    size_t start = ++index;
    string result;
    while (index < length && json[index] != '"')
    {
      if (json[index] == '\\')
      {
        index++;
        if (index >= length)
          throw runtime_error("Escape sequence not terminated");
        switch (json[index])
        {
        case 'n':
          result += '\n';
          break;
        case 't':
          result += '\t';
          break;
        case 'r':
          result += '\r';
          break;
        case '\\':
          result += '\\';
          break;
        case '"':
          result += '"';
          break;
        default:
          throw runtime_error("Invalid escape sequence");
        }
      }
      else
      {
        result += json[index];
      }
      index++;
    }
    if (index >= length)
      throw runtime_error("Unterminated string");
    index++;
    return result;
  };

  skip_whitespace();
  if (json[index] != '{')
    throw runtime_error("Expected '{'");
  index++;
  while (true)
  {
    skip_whitespace();
    if (json[index] == '}')
      break;
    string key = extract_string();
    skip_whitespace();
    if (json[index] != ':')
      throw runtime_error("Expected ':' after key");
    index++;
    string value = extract_string();
    result_map[key] = value;
    skip_whitespace();
    if (json[index] == ',')
    {
      index++;
    }
    else if (json[index] != '}')
    {
      throw runtime_error("Expected ',' or '}'");
    }
  }
  index++;
  return result_map;
}

/**
 * @brief Converts a string representation of an address to a sockaddr_in structure.
 *
 * This function parses a string containing an IP address and port,
 * and fills a sockaddr_in structure with the parsed address information.
 *
 * @param addr_str The string representation of the address.
 * @return A sockaddr_in structure containing the parsed IP address and port.
 */
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

/**
 * @brief Computes the MD5 hash of a given string.
 *
 * This function calculates the MD5 hash of the input string and returns
 * the hash as a hexadecimal string.
 *
 * @param to_hash The string to be hashed.
 * @return A string representing the MD5 hash of the input.
 */
string compute_md5_hash(const string &to_hash)
{
  MD5_CTX c;
  MD5_Init(&c);
  MD5_Update(&c, to_hash.c_str(), to_hash.size());
  unsigned char digest_buffer[MD5_DIGEST_LENGTH];
  MD5_Final(digest_buffer, &c);
  // Convert digest to hexadecimal representation
  string message_uid;
  char hex_buffer[3];
  for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
  {
    sprintf(hex_buffer, "%02x", digest_buffer[i]);
    message_uid += hex_buffer;
  }
  return message_uid;
}

/**
 * @brief Converts a string to lowercase.
 *
 * This function transforms all characters in the given string to lowercase
 * and returns the resulting string.
 *
 * @param str The string to be converted to lowercase.
 * @return A string with all characters in lowercase.
 */
string lower_case(const string &str)
{
  string result = str;
  transform(result.begin(), result.end(), result.begin(), [](unsigned char c)
            { return tolower(c); });
  return result;
}

/**
 * @brief Encodes a string to Base64 format.
 *
 * This function converts the input string to its Base64 encoded representation.
 *
 * @param data The string to be encoded.
 * @return A Base64 encoded string.
 */
std::string base64_encode(const std::string &data)
{
  BIO *bio, *b64;
  BUF_MEM *bufferPtr;

  b64 = BIO_new(BIO_f_base64());
  bio = BIO_new(BIO_s_mem());
  bio = BIO_push(b64, bio);

  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Do not use newlines to flush buffer
  BIO_write(bio, data.c_str(), data.length());
  BIO_flush(bio);
  BIO_get_mem_ptr(bio, &bufferPtr);
  BIO_set_close(bio, BIO_NOCLOSE);

  std::string output(bufferPtr->data, bufferPtr->length);
  BIO_free_all(bio);

  return output;
}

/**
 * @brief Decodes a Base64 encoded string.
 *
 * This function converts a Base64 encoded string back to its original representation.
 *
 * @param encoded_data The Base64 encoded string to be decoded.
 * @return The original decoded string.
 */
std::string base64_decode(const std::string &encoded_data)
{
  BIO *bio, *b64;
  char inbuf[512];
  std::string output;
  int inlen;

  b64 = BIO_new(BIO_f_base64());
  bio = BIO_new_mem_buf(encoded_data.c_str(), encoded_data.length());

  bio = BIO_push(b64, bio);
  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Do not use newlines to flush buffer

  while ((inlen = BIO_read(bio, inbuf, sizeof(inbuf))) > 0)
  {
    output.append(inbuf, inlen);
  }

  BIO_free_all(bio);

  return output;
}
