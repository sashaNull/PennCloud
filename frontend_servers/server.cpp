#include <arpa/inet.h>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <map>
#include <unordered_map>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>

#include "../utils/utils.h"
#include "./client_communication.h"
#include "./backend_communication.h"
#include "./webmail.h"
#include "./admin.h"
#include "./bulletin.h"
#include "./drive.h"
using namespace std;

#define LOAD_BALANCER_IP "127.0.0.1"
#define LOAD_BALANCER_PORT 8080

const unsigned int BUFFER_SIZE = 1140 * 10 + 1;

// Global mutex declaration
pthread_mutex_t map_mutex = PTHREAD_MUTEX_INITIALIZER;

bool verbose = false;
string g_coordinator_addr_str = "127.0.0.1:7070";
sockaddr_in g_coordinator_addr = get_socket_address(g_coordinator_addr_str);

map<string, string> g_map_rowkey_to_server;
// TODO: coordinator map: row key -> backend server address

string g_serveraddr_str;
int g_listen_fd;
std::unordered_map<std::string, std::string> g_endpoint_html_map;

// Map from cookie to user
std::unordered_map<std::string, std::string> cookie_user_map;

// Function to generate a random session ID
string generate_cookie()
{
  const string charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  const int length = 32; // You can adjust the length of the session ID
  string sessionID;
  srand(time(nullptr)); // Seed the random number generator
  for (int i = 0; i < length; ++i)
  {
    sessionID.push_back(charset[rand() % charset.length()]);
  }
  return sessionID;
}

struct Part
{
  std::map<std::string, std::string> headers;
  std::vector<char> content;
};

// Function to extract boundary for form data
std::string extract_boundary(const std::string &content_type)
{
  size_t pos = content_type.find("boundary=");
  if (pos != std::string::npos)
  {
    std::string result = content_type.substr(pos + 9); // 9 to skip 'boundary='
    size_t pos = result.find_last_not_of("\r\n");

    // If any non-whitespace character is found, erase the characters after it
    if (pos != std::string::npos)
    {
      result.erase(pos + 1);
    }

    return result;
  }
  return "";
}

// Function to compare two lines after stripping
bool compare_stripped(const std::string &line1, const std::string &line2)
{
  std::string stripped_line1 = strip(line1);
  std::string stripped_line2 = strip(line2);
  return stripped_line1 == stripped_line2;
}

bool recv_file_name_path(int client_fd, int &content_length, std::string boundary, std::string &filename, std::string &path)
{
  char buf[2];               // Buffer to hold received data
  std::string received_data; // Temporary buffer for processing

  // First Loop: Read until "\r\n\r\n"
  while (true)
  {
    int bytes_received = recv(client_fd, buf, 1, 0);
    if (bytes_received <= 0)
    {
      // Error handling for recv failure
      return false;
    }

    content_length -= bytes_received;

    received_data += buf[0];

    if (received_data.size() >= 4 && received_data.substr(received_data.size() - 4) == "\r\n\r\n")
    {
      break; // Found "\r\n\r\n", exit loop
    }
  }

  // Check if received_data contains the boundary
  if (received_data.find(boundary) == std::string::npos)
  {
    return false; // Boundary not found, return false
  }

  // Read the next line to get the filename
  filename.clear();
  while (true)
  {
    int bytes_received = recv(client_fd, buf, 1, 0);
    if (bytes_received <= 0)
    {
      // Error handling for recv failure
      return false;
    }

    content_length -= bytes_received;

    filename += buf[0];
    if (filename.size() >= 2 && filename.substr(filename.size() - 2) == "\r\n")
    {
      break; // Found end of line, exit loop
    }
  }

  size_t crlf_pos = filename.find_last_not_of("\r\n");
  if (crlf_pos != std::string::npos)
  {
    filename.erase(crlf_pos + 1);
  }

  // Second Loop: Read until "\r\n\r\n"
  received_data.clear();
  while (true)
  {
    int bytes_received = recv(client_fd, buf, 1, 0);
    if (bytes_received <= 0)
    {
      // Error handling for recv failure
      return false;
    }

    content_length -= bytes_received;

    received_data += buf[0];

    if (received_data.size() >= 4 && received_data.substr(received_data.size() - 4) == "\r\n\r\n")
    {
      break; // Found "\r\n\r\n", exit loop
    }
  }

  // Check if received_data contains the boundary
  if (received_data.find(boundary) == std::string::npos)
  {
    return false; // Boundary not found, return false
  }

  // Read the next line to get the path
  path.clear();
  while (true)
  {
    int bytes_received = recv(client_fd, buf, 1, 0);
    if (bytes_received <= 0)
    {
      // Error handling for recv failure
      return false;
    }

    content_length -= bytes_received;

    path += buf[0];
    if (path.size() >= 2 && path.substr(path.size() - 2) == "\r\n")
    {
      break; // Found end of line, exit loop
    }
  }

  crlf_pos = path.find_last_not_of("\r\n");
  if (crlf_pos != std::string::npos)
  {
    path.erase(crlf_pos + 1);
  }

  // Third Loop: Check for boundary
  received_data.clear();
  while (true)
  {
    int bytes_received = recv(client_fd, buf, 1, 0);
    if (bytes_received <= 0)
    {
      // Error handling for recv failure
      return false;
    }

    content_length -= bytes_received;

    received_data += buf[0];

    if (received_data.size() >= 4 && received_data.substr(received_data.size() - 4) == "\r\n\r\n")
    {
      break; // Found "\r\n\r\n", exit loop
    }
  }

  // Check if received_data contains the boundary
  if (received_data.find(boundary) == std::string::npos)
  {
    return false; // Boundary not found, return false
  }
  return true;
}

bool file_chunk_storing(int client_fd, int backend_fd, int content_length, string file_row_key, string boundary)
{
  char buffer[BUFFER_SIZE];
  int boundary_and_crlf_length = ("\r\n" + boundary + "--\r\n\r\n").length();

  int i = 1;
  while (true)
  {
    memset(buffer, 0, sizeof(buffer));

    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    fcntl(client_fd, F_SETFL, flags);

    if (bytes_received <= 0)
    {
      break;
    }
    cout << "BUFFER LENGTH: " << bytes_received << endl;

    string rowkey = file_row_key;
    string colkey = "content_" + to_string(i);
    string value = string(buffer, bytes_received);
    size_t boundary_index = value.find(boundary);
    string type = "put";

    if (boundary_index != std::string::npos)
    {
      value.erase(value.length() - boundary_and_crlf_length, boundary_and_crlf_length);
    }

    cout << "ITERATION: " << i << endl;
    cout << "BUFFER LENGTH: " << bytes_received << endl;
    cout << "LENGTH AFTER TURNING TO A STRING: " << string(buffer, bytes_received).size() << endl;
    cout << "BUFFER: " << value << endl;
    value = base64_encode(value);

    cout << "LENGTH AFTER TURNING TO BASE64: " << value.size() << endl;

    F_2_B_Message msg_to_send_put = construct_msg(2, rowkey, colkey, value, "", "", 0);
    string response_value, response_error_msg;
    int response_code, response_status;

    response_code = send_msg_to_backend(backend_fd, msg_to_send_put, response_value, response_status,
                                        response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                        g_coordinator_addr, type);

    if (response_code == 1)
    {
      cerr << "ERROR in communicating with coordinator" << endl;
      return false;
    }
    else if (response_code == 2)
    {
      cerr << "ERROR in communicating with backend" << endl;
      return false;
    }

    if (response_status != 0)
    {
      cerr << "ERROR in backend" << endl;
      return false;
    }
    i++;
  }

  cout << "OUT OF LOOP!!!!!!!!!!!" << endl;

  string type = "put";
  string rowkey = file_row_key;
  string colkey = "no_chunks";
  string value = to_string(i - 1);

  F_2_B_Message msg_to_send_put = construct_msg(2, rowkey, colkey, value, "", "", 0);
  string response_value, response_error_msg;
  int response_code, response_status;
  response_code = send_msg_to_backend(backend_fd, msg_to_send_put, response_value, response_status,
                                      response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                      g_coordinator_addr, type);
  cout << "CHECK HERE PLEASE: " << response_code << endl;
  if (response_code == 1)
  {
    cerr << "ERROR in communicating with coordinator" << endl;
    return false;
  }
  else if (response_code == 2)
  {
    cerr << "ERROR in communicating with backend" << endl;
    return false;
  }

  return true;
}

// Function to parse multi-part form data
std::map<std::string, Part> parse_multipart_form_data(const std::string &body, const std::string &boundary)
{
  std::map<std::string, Part> parts;
  std::string delimiter = "--" + boundary;
  std::string end_delimiter = delimiter + "--";
  bool isHeaderPart = true;
  Part currentPart;
  std::string partName;

  // Strip \r from the body content
  std::string new_body = strip(body, "\r");
  std::istringstream stream(new_body);
  std::string line;

  while (std::getline(stream, line))
  {
    // Normalize the line by removing carriage return
    if (!line.empty() && line.back() == '\r')
    {
      line.pop_back();
    }

    // std::cout << "Processing line: " << line << std::endl;

    // Detect boundaries
    if (compare_stripped(line, delimiter) || compare_stripped(line, end_delimiter))
    {
      // std::cout << "I FOUND A DELIMITER / END DEMILITER : " << line << std::endl;
      if (!isHeaderPart && !partName.empty())
      {
        parts[partName] = std::move(currentPart); // Save completed part
        // std::cout << "SAVED PART : " << std::endl;
        currentPart = Part(); // Reset part
        partName.clear();
      }
      isHeaderPart = true;
      if (compare_stripped(line, end_delimiter))
      {
        // std::cout << "I FOUND FINAL BOUNDARY : " << line << std::endl;
        break; // Stop processing after the final boundary
      }
      continue;
    }

    // Handle headers
    if (isHeaderPart)
    {
      if (line.empty())
      {
        // std::cout << "I FOUND EMPTY LINE SO HEADER ENDS " << std::endl;
        isHeaderPart = false; // Empty line indicates the end of headers
        continue;
      }

      // Parse headers
      size_t pos = line.find(':');
      if (pos != std::string::npos)
      {
        std::string headerKey = line.substr(0, pos);
        // std::cout << "HEADER KEY : " << headerKey << std::endl;
        std::string headerValue = line.substr(pos + 2); // Skip ': ' after the key
        // std::cout << "HEADER VALUE : " << headerValue << std::endl;

        // Check if this is a disposition header containing the part name
        if (headerKey == "Content-Disposition")
        {
          size_t namePos = headerValue.find("name=\"");
          if (namePos != std::string::npos)
          {
            namePos += 6; // Skip past 'name="'
            size_t nameEnd = headerValue.find('"', namePos);
            partName = headerValue.substr(namePos, nameEnd - namePos);

            // std::cout << "IN CONTENT DISPOSITION - PART NAME : " << partName << std::endl;
          }
        }
        currentPart.headers[headerKey] = headerValue;
      }
    }
    else if (!partName.empty())
    {
      // Content reading
      std::ostringstream contentStream;
      contentStream << line; // Start with the current line
      while (std::getline(stream, line) && !compare_stripped(line, delimiter) && !compare_stripped(line, end_delimiter))
      {
        if (!line.empty() && line.back() == '\r')
        {
          line.pop_back(); // Normalize the line
        }
        // std::cout << "CONTENT READING LINE : " << line << std::endl;

        contentStream << "\n"
                      << line; // Append the line to content
      }

      if (compare_stripped(line, delimiter) || compare_stripped(line, end_delimiter))
      {
        stream.seekg(-(long)(line.length() + 2), std::ios_base::cur); // Rewind to handle the delimiter again
        // std::cout << "REWINDED : " << line << std::endl;
      }

      // Assign content to the current part
      std::string contentStr = contentStream.str();
      // std::cout << "CONTENT STREAM: " << contentStr << std::endl;
      currentPart.content.assign(contentStr.begin(), contentStr.end());
      isHeaderPart = true; // Prepare for the next part
      // std::cout << "ASSIGNED CONTENT TO PART " << std::endl;
    }
    // std::cout << "GOING TO NEXT LINE: " << std::endl;
    // if (!std::getline(stream, line))
    // {
    //   std::cerr << "Failed to read line, stopping." << std::endl;
    //   break;
    // }
  }
  return parts;
}

std::string url_decode(const std::string &str)
{
  std::ostringstream decoded;
  for (size_t i = 0; i < str.size(); ++i)
  {
    if (str[i] == '%' && i + 2 < str.size())
    {
      int hex_val;
      std::istringstream hex(str.substr(i + 1, 2));
      if (hex >> std::hex >> hex_val)
      {
        decoded << static_cast<char>(hex_val);
        i += 2;
      }
      else
      {
        decoded << str[i];
      }
    }
    else if (str[i] == '+')
    {
      decoded << ' ';
    }
    else
    {
      decoded << str[i];
    }
  }
  return decoded.str();
}

void send_response_with_headers(int client_fd, int status_code, const std::string &status_text, const std::string &content_type, const std::string &content, const std::string &content_disposition, const std::string &cookie = "")
{
  // Construct the HTTP response headers
  std::stringstream response;
  response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
  response << "Content-Type: " << content_type << "\r\n";
  response << "Content-Disposition: " << content_disposition << "\r\n";
  response << "Content-Length: " << content.size() << "\r\n";
  if (!cookie.empty())
  {
    response << "Set-Cookie: sid=" << cookie << "; Path=/; Expires=Wed, 31 Jul 2024 07:28:00 GMT; HttpOnly\r\n";
  }
  response << "\r\n"; // End of headers

  // Send the response headers
  send(client_fd, response.str().c_str(), response.str().size(), 0);

  // Send the response content
  send(client_fd, content.c_str(), content.size(), 0);
}

std::string get_cookie_from_header(const std::string &request)
{
  std::cout << "I am in get cookie handler from the request" << std::endl;
  std::string cookie_header = "Cookie: sid=";
  size_t start_pos = request.find(cookie_header);
  if (start_pos != std::string::npos)
  {
    size_t end_pos = request.find("\r\n", start_pos);
    start_pos += cookie_header.length();
    if (end_pos != std::string::npos)
    {
      std::cout << "cookie is: " << request.substr(start_pos, end_pos - start_pos) << std::endl;
      return request.substr(start_pos, end_pos - start_pos);
    }
  }
  return "";
}

std::string get_username_from_cookie(const std::string &cookie, int backend_fd)
{
  // Check if cookie exists in the map
  // Lock the mutex before accessing the map
  pthread_mutex_lock(&map_mutex);

  // Map the cookie to the username
  auto it = cookie_user_map.find(cookie);

  // Unlock the mutex after modifying the map
  pthread_mutex_unlock(&map_mutex);

  if (it != cookie_user_map.end() && !it->second.empty())
  {
    return it->second; // Return the username if found
  }
  else
  {
    // Cookie not found in local map, make a GET request to backend
    F_2_B_Message msg_to_send;
    string response_value, response_error_msg;
    int response_status, response_code;
    string rowkey = "cookie";
    string type = "get";
    string colkey = cookie;
    msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
    response_code = send_msg_to_backend(backend_fd, msg_to_send, response_value, response_status,
                                        response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                        g_coordinator_addr, type);
    if (response_code == 1)
    {
      cerr << "ERROR in communicating with coordinator" << endl;
      return "";
    }
    else if (response_code == 2)
    {
      cerr << "ERROR in communicating with backend" << endl;
      return "";
    }

    if (response_status == 0)
    {
      std::string username = response_value;
      // Lock the mutex before accessing the map
      pthread_mutex_lock(&map_mutex);

      // Map the cookie to the username
      cookie_user_map[cookie] = username; // Cache the username for future requests
      cout << cookie << endl;
      cout << username << endl;

      // Unlock the mutex after modifying the map
      pthread_mutex_unlock(&map_mutex);
      return username;
    }
    else
    {
      // Handle error
      return "";
    }
  }
}

std::vector<std::string> parse_items_list(const std::string &items_str)
{
  std::vector<std::string> items;

  // Find the position of the first '[' and the last ']'
  size_t start_pos = items_str.find_first_of('[');
  size_t end_pos = items_str.find_last_of(']');

  // Extract the substring between '[' and ']', excluding them
  std::string cleaned_str = items_str.substr(start_pos + 1, end_pos - start_pos - 1);

  // Use cleaned_str to extract items
  std::istringstream iss(cleaned_str);
  std::string item;
  while (std::getline(iss, item, ','))
  {
    items.push_back(item);
  }

  return items;
}

// Function to recursively delete folder contents
void deleteFolderContents(const string &folderPath, int client_fd, int fd, const string &username)
{
  // Get items for the folder
  string response_value, response_error_msg;
  int response_status;
  string row_key = username + "_" + folderPath;
  string colkey = "items";
  string type = "get";
  F_2_B_Message msg_to_send = construct_msg(1, row_key, colkey, "", "", "", 0);
  int response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                          response_error_msg, row_key, colkey, g_map_rowkey_to_server,
                                          g_coordinator_addr, type);
  if (response_code == 1)
  {
    cerr << "ERROR in communicating with coordinator" << endl;
    return;
  }
  else if (response_code == 2)
  {
    cerr << "ERROR in communicating with backend" << endl;
    return;
  }

  std::cout << "row key for delete: " << row_key << std::endl;

  if (response_status == 0)
  {
    // Parse items list
    vector<string> items = parse_items_list(response_value);

    // Iterate through items
    for (const auto &item : items)
    {
      string itemType = item.substr(0, 2);
      string itemName = item.substr(2);

      // Recursively delete folders
      if (itemType == "D@")
      {
        deleteFolderContents(folderPath + "/" + itemName, client_fd, fd, username);

        // Delete folder item
        row_key = username + "_" + folderPath + "/" + itemName;
        colkey = "items";
        type = "delete";
        msg_to_send = construct_msg(3, row_key, colkey, "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, row_key, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          return;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          return;
        }

        std::cout << "Sending delete for: " << row_key << std::endl;
      }
      // Delete files
      else if (itemType == "F@")
      {
        // Delete file content
        string row_key = username + "_" + folderPath + "/" + itemName;
        int delete_response_status = delete_file_chunks(fd, row_key, g_map_rowkey_to_server, g_coordinator_addr);

        if (delete_response_status != 0)
        {
          // Error handling: send appropriate error response to the client
          string error_message = "Failed to delete file";
          send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
          return; // Stop processing if there's an error
        }
      }
    }
  }
}

// Function to recursively rename folder
void renameFolder(const string &oldFolderPath, const string &newFolderPath, int client_fd, int fd, const string &username)
{
  // Get items for the folder
  string row_key = username + "_" + oldFolderPath;
  string new_row_key = username + "_" + newFolderPath;

  string response_value, response_error_msg;
  int response_status, response_code;

  std::cout << "new row key for rename: " << new_row_key << std::endl;

  // GET items from original folder
  string colkey = "items";
  string type = "get";
  F_2_B_Message msg_to_send = construct_msg(1, row_key, colkey, "", "", "", 0);
  response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                      response_error_msg, row_key, colkey, g_map_rowkey_to_server,
                                      g_coordinator_addr, type);
  if (response_code == 1)
  {
    cerr << "ERROR in communicating with coordinator" << endl;
    return;
  }
  else if (response_code == 2)
  {
    cerr << "ERROR in communicating with backend" << endl;
    return;
  }

  if (response_status == 0)
  {
    // Parse items list
    vector<string> items = parse_items_list(response_value);

    // Iterate through items
    for (const auto &item : items)
    {
      string itemType = item.substr(0, 2);
      string itemName = item.substr(2);

      // Recursively delete folders
      if (itemType == "D@")
      {
        renameFolder(oldFolderPath + "/" + itemName, newFolderPath + "/" + itemName, client_fd, fd, username);

        // GET old folder contents and PUT folder path with new folder name
        row_key = username + "_" + oldFolderPath + "/" + itemName;
        new_row_key = username + "_" + newFolderPath + "/" + itemName;

        msg_to_send = construct_msg(1, row_key, colkey, "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, row_key, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          return;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          return;
        }

        std::cout << "Row key for get: " << row_key << std::endl;
        std::cout << "Value of get: " << response_value << std::endl;

        if (response_status != 0)
        {
          // Error handling: send appropriate error response to the client
          string error_message = "Failed to fetch old folder";
          send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
          return; // Stop processing if there's an error
        }
        string type = "put";
        F_2_B_Message msg_to_send = construct_msg(2, new_row_key, colkey, response_value, "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, new_row_key, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          return;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          return;
        }

        std::cout << "New Row key for put: " << new_row_key << std::endl;
        std::cout << "Value of put: " << response_value << std::endl;

        if (response_status != 0)
        {
          // Error handling: send appropriate error response to the client
          string error_message = "Failed to create new path";
          send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
          return; // Stop processing if there's an error
        }

        // Delete folder item
        std::cout << "Sending delete for: " << row_key << std::endl;
        type = "delete";
        msg_to_send = construct_msg(3, row_key, colkey, "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, row_key, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          return;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          return;
        }
        if (response_status != 0)
        {
          // Error handling: send appropriate error response to the client
          string error_message = "Failed to delete folder";
          send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
          return; // Stop processing if there's an error
        }
      }
      // Delete files
      else if (itemType == "F@")
      {

        // GET old file contents and PUT folder path with new folder name
        row_key = username + "_" + oldFolderPath + "/" + itemName;
        new_row_key = username + "_" + newFolderPath + "/" + itemName;

        colkey = "content";
        type = "get";
        msg_to_send = construct_msg(1, row_key, colkey, "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, row_key, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          return;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          return;
        }
        std::cout << "Row key for get: " << row_key << std::endl;
        std::cout << "Value of get: " << response_value << std::endl;

        if (response_status != 0)
        {
          // Error handling: send appropriate error response to the client
          string error_message = "Failed to fetch old file";
          send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
          return; // Stop processing if there's an error
        }
        type = "put";
        msg_to_send = construct_msg(2, new_row_key, colkey, response_value, "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, new_row_key, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          return;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          return;
        }
        std::cout << "New Row key for put: " << new_row_key << std::endl;
        std::cout << "Value of put: " << response_value << std::endl;

        if (response_status != 0)
        {
          // Error handling: send appropriate error response to the client
          string error_message = "Failed to create new path";
          send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
          return; // Stop processing if there's an error
        }

        // Delete file content
        string row_key = username + "_" + oldFolderPath + "/" + itemName;

        std::cout << "row key for delete file: " << row_key << std::endl;
        type = "delete";
        msg_to_send = construct_msg(3, row_key, colkey, "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, row_key, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          return;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          return;
        }

        if (response_status != 0)
        {
          // Error handling: send appropriate error response to the client
          string error_message = "Failed to delete file";
          send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
          return; // Stop processing if there's an error
        }
      }
    }
  }
}

void sigint_handler(int signum)
{
  cout << "SIGINT received, shutting down." << endl;
  if (fd_is_open(g_listen_fd))
  {
    close(g_listen_fd);
  }
  exit(EXIT_SUCCESS);
}

void install_sigint_handler()
{
  if (signal(SIGINT, sigint_handler) == SIG_ERR)
  {
    cerr << "SIGINT handling in main thread failed" << endl;
    exit(EXIT_FAILURE);
  }
}

string parse_commands(int argc, char *argv[])
{
  int cmd;
  string ordering_str;
  while ((cmd = getopt(argc, argv, "o:v")) != -1)
  {
    switch (cmd)
    {
    case 'v':
      verbose = true;
      break;
    default: /* if other error, stderr */
      cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
      exit(EXIT_FAILURE);
    }
  }
  // get config file
  string config_file_path;
  if (argv[optind])
  {
    config_file_path = argv[optind];
  }
  else
  {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }
  if (config_file_path.empty())
  {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }
  if (!filepath_is_valid(config_file_path))
  {
    cerr << "Config file " << config_file_path << " is not valid." << endl;
    exit(EXIT_FAILURE);
  }
  // get number of servers
  ifstream file(config_file_path);
  string line;
  vector<string> lines;
  int line_count = 0;
  while (getline(file, line))
  {
    lines.push_back(line);
    line_count++;
  }
  // get server index
  optind++;
  int server_index;
  if (argv[optind])
  {
    server_index = stoi(argv[optind]);
  }
  else
  {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }
  if (server_index < 0 || server_index > line_count - 1)
  {
    cerr << "Error: Server index " << server_index << " is not in valid range."
         << endl;
    exit(EXIT_FAILURE);
  }
  // return server address
  return lines[server_index];
}

bool suspended = false;
pthread_mutex_t suspend_mutex = PTHREAD_MUTEX_INITIALIZER;

string recv_header(int client_fd)
{
  char buffer;
  ssize_t bytes_received;
  string received_request_header = "";

  while (true)
  {
    bytes_received = recv(client_fd, &buffer, 1, 0);
    if (bytes_received == -1)
    {
      return "HANDLE ERROR";
    }
    else if (bytes_received == 0)
    {
      return "CONNECTION CLOSED";
    }
    else
    {
      received_request_header += buffer;

      if (received_request_header.size() >= 4 && received_request_header.substr(received_request_header.size() - 4) == "\r\n\r\n")
      {
        break;
      }
    }
  }

  return received_request_header;
}

void *handle_connection(void *arg)
{
  int client_fd = *static_cast<int *>(arg);
  delete static_cast<int *>(arg);
  // Receive the request
  ssize_t bytes_received;

  int fd = create_socket();

  string colkey, rowkey, type;
  // Keep listening for requests
  while (true)
  {
    cout << "Listening..." << endl;
    string request_header = recv_header(client_fd);

    if (request_header == "HANDLE ERROR")
    {
      continue;
    }
    else if (request_header == "CONNECTION CLOSED")
    {
      break;
    }

    cout << request_header << endl;
    unordered_map<string, string> html_request_map = parse_http_header(request_header);

    if (html_request_map["method"] != "POST" || html_request_map["uri"] != "/upload_file")
    {
      if (html_request_map.find("header_Content-Length") != html_request_map.end())
      {
        int small_content_length = stoi(html_request_map["header_Content-Length"]);
        if (small_content_length > 0)
        {
          char buffer[small_content_length + 1];
          buffer[small_content_length] = '\0';
          bytes_received = recv(client_fd, &buffer, small_content_length, 0);
          if (bytes_received == -1)
          {
            continue;
          }
          else if (bytes_received == 0)
          {
            break;
          }
          else
          {
            html_request_map["body"] = string(buffer);
          }
        }
      }
    }

    // Check if server is suspended and the request is not for /revive
    pthread_mutex_lock(&suspend_mutex);
    bool current_suspended = suspended;
    pthread_mutex_unlock(&suspend_mutex);

    // Check if the incoming request is for the /status endpoint
    if (html_request_map["uri"] == "/status")
    {
      if (current_suspended)
      {
        // If the server is suspended, send a 503 Service Unavailable response
        send_response(client_fd, 503, "Service Unavailable", "text/plain", "Server is suspended.");
      }
      else
      {
        // If the server is not suspended, send a 200 OK response
        send_response(client_fd, 200, "OK", "text/plain", "Server is active and accepting requests.");
      }
      continue; // Continue to the next iteration of the loop
    }

    if (current_suspended && html_request_map["uri"] != "/revive")
    {
      std::string suspended_response = R"(
        <html>
            <head><title>Suspended</title></head>
            <body>
                <p>This server is currently suspended. Redirecting to another server in 1 second...</p>
                <script>
                    setTimeout(function() {
                        window.location.href = 'http://127.0.0.1:8080/';
                    }, 500);
                </script>
            </body>
        </html>
    )";
      send_response(client_fd, 200, "OK", "text/html", suspended_response);
      continue;
    }

    // Handling /suspend
    if (html_request_map["uri"] == "/suspend")
    {
      pthread_mutex_lock(&suspend_mutex);
      suspended = true;
      pthread_mutex_unlock(&suspend_mutex);
      std::string response_body = R"(
          <html>
          <head>
              <script>
                  window.onload = function() {
                      setTimeout(function() {
                          // Check if the referrer is available
                          if (document.referrer) {
                              // Modify the referrer URL to point to the /admin page
                              var adminUrl = new URL(document.referrer);
                              adminUrl.pathname = '/admin';  // Set the path to /admin
                              
                              // Redirect to the modified URL
                              window.location.href = adminUrl.href;
                          } else {
                              // If no referrer is available, use a default or fallback URL
                              window.location.href = '/admin';
                          }
                      }, 1500);
                  };
              </script>
          </head>
          <body>
              <p>Server is suspended. You will be redirected to the admin page shortly. Access <a href='/revive'>/revive</a> to resume.</p>
          </body>
          </html>
      )";

      send_response(client_fd, 200, "OK", "text/html", response_body);
      continue;
    }

    // Handling /revive
    if (html_request_map["uri"] == "/revive")
    {
      pthread_mutex_lock(&suspend_mutex);
      suspended = false;
      pthread_mutex_unlock(&suspend_mutex);
      std::string response_body = R"(
          <html>
          <head>
              <script>
                  window.onload = function() {
                      setTimeout(function() {
                          // Check if the referrer is available
                          if (document.referrer) {
                              // Modify the referrer URL to point to the /admin page
                              var adminUrl = new URL(document.referrer);
                              adminUrl.pathname = '/admin';  // Set the path to /admin
                              
                              // Redirect to the modified URL
                              window.location.href = adminUrl.href;
                          } else {
                              // If no referrer is available, use a default or fallback URL
                              window.location.href = '/admin';
                          }
                      }, 1500);
                  };
              </script>
          </head>
          <body>
              <p>Server is revived and operational. You will be redirected to the admin page shortly.</p>
          </body>
          </html>
      )";
      send_response(client_fd, 200, "OK", "text/html", response_body);
      continue;
    }

    // GET: rendering signup page
    if (html_request_map["uri"] == "/signup" && html_request_map["method"] == "GET")
    {
      // get cookie
      std::string cookie = get_cookie_from_header(request_header);

      // check if cookie exists
      if (cookie.empty())
      {
        // Retrieve HTML content from the map
        std::string html_content = g_endpoint_html_map["signup"];

        // Construct and send the HTTP response
        send_response(client_fd, 200, "OK", "text/html", html_content);
      }
      else
      {
        // Redirect to login
        redirect(client_fd, "/login");
      }
    }

    // POST: new user signup
    else if (html_request_map["uri"] == "/signup" && html_request_map["method"] == "POST")
    {
      map<string, string> form_data = parse_json_string_to_map(html_request_map["body"]);
      string username = form_data["username"];
      transform(username.begin(), username.end(), username.begin(), ::tolower);

      // START COORDINATOR
      string response_value, get_response_value;
      string response_error_msg, get_response_error_msg;
      int response_status, get_response_status;
      rowkey = username + "_info";
      colkey = "password";
      type = "get";
      F_2_B_Message msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
      int response_code = send_msg_to_backend(fd, msg_to_send, get_response_value, get_response_status,
                                              get_response_error_msg, rowkey, colkey,
                                              g_map_rowkey_to_server, g_coordinator_addr, type);
      if (response_code == 1)
      {
        cerr << "ERROR in communicating with coordinator" << endl;
        continue;
      }
      else if (response_code == 2)
      {
        cerr << "ERROR in communicating with backend" << endl;
        continue;
      }

      cout << "get response status: " << get_response_status << endl;
      cout << "get response error msg: " << get_response_error_msg << endl;

      if (get_response_status == 1 && strip(get_response_error_msg) == "Rowkey does not exist")
      {
        // Put new user in database
        string firstname = form_data["firstName"];
        string lastname = form_data["lastName"];
        string email = form_data["email"];
        string password = form_data["password"];

        rowkey = username + "_info";
        colkey = "firstName";
        type = "put";
        msg_to_send = construct_msg(2, rowkey, colkey, firstname, "", "", 0);
        int response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                                response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          continue;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          continue;
        }

        colkey = "email";
        msg_to_send = construct_msg(2, rowkey, colkey, email, "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          continue;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          continue;
        }

        colkey = "password";
        msg_to_send = construct_msg(2, rowkey, colkey, password, "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          continue;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          continue;
        }

        rowkey = username + "_email";
        colkey = "inbox_items";
        msg_to_send = construct_msg(2, username + "_email", "inbox_items", "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          continue;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          continue;
        }

        colkey = "sentbox_items";
        msg_to_send = construct_msg(2, username + "_email", "sentbox_items", "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          continue;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          continue;
        }

        rowkey = username + "_drive";
        colkey = "items";
        msg_to_send = construct_msg(2, username + "_drive", "items", "[]", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          continue;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          continue;
        }

        // if successful, ask browser to redirect to /login
        string redirect_to = "/login";
        redirect(client_fd, redirect_to);
      }
      else if (get_response_status == 0)
      {
        // error: user already exists - Construct and send the HTTP response
        std::string content = "{\"error\":\"User already exists\"}";
        send_response(client_fd, 409, "Conflict", "application/json", content);
      }
      else
      {
        // error: signup failed - Construct and send the HTTP response
        std::string content = "{\"error\":\"Signup Failed\"}";
        send_response(client_fd, 400, "Bad Request", "application/json", content);
      }
    }

    // GET: rendering login page
    else if (html_request_map["uri"] == "/login" && html_request_map["method"] == "GET")
    {
      // get cookie
      std::string cookie = get_cookie_from_header(request_header);

      // check if cookie exists
      if (cookie.empty())
      {
        // Retrieve HTML content from the map
        std::string html_content = g_endpoint_html_map["login"];

        // Construct and send the HTTP response
        send_response(client_fd, 200, "OK", "text/html", html_content);
      }
      else
      {
        // Redirect to login
        redirect(client_fd, "/home");
      }
    }

    // POST: user login
    else if (html_request_map["uri"] == "/login" && html_request_map["method"] == "POST")
    {
      map<string, string> form_data = parse_json_string_to_map(html_request_map["body"]);
      string username = form_data["username"];
      string password = form_data["password"];
      transform(username.begin(), username.end(), username.begin(), ::tolower);

      // START COORDINATOR
      string response_value, get_response_value;
      string response_error_msg, get_response_error_msg;
      int response_status, get_response_status;
      rowkey = username + "_info";
      colkey = "password";
      type = "get";
      F_2_B_Message msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
      int response_code = send_msg_to_backend(fd, msg_to_send, get_response_value, get_response_status,
                                              get_response_error_msg, rowkey, colkey,
                                              g_map_rowkey_to_server, g_coordinator_addr, type);
      if (response_code == 1)
      {
        cerr << "ERROR in communicating with coordinator" << endl;
        continue;
      }
      else if (response_code == 2)
      {
        cerr << "ERROR in communicating with backend" << endl;
        continue;
      }

      if (get_response_status == 1 && strip(get_response_error_msg) == "Rowkey does not exist")
      {
        // User does not exist - Construct and send the HTTP response
        string content = "{\"error\":\"User does not exist\"}";
        send_response(client_fd, 404, "Not Found", "application/json", content);
      }
      else if (get_response_status == 0)
      {
        // User exists, check password
        string actual_password = get_response_value;
        if (password == actual_password)
        {

          std::string cookie = get_cookie_from_header(request_header);
          if (cookie.empty())
          {
            cookie = generate_cookie();
            // Map the cookie to the username
            // Lock the mutex before accessing the map
            pthread_mutex_lock(&map_mutex);

            // Map the cookie to the username
            cookie_user_map[cookie] = username;

            // Unlock the mutex after modifying the map
            pthread_mutex_unlock(&map_mutex);

            // PUT this mapping in the backend

            rowkey = "cookie";
            colkey = cookie;
            type = "put";
            F_2_B_Message msg_to_send = construct_msg(2, rowkey, colkey, username, "", "", 0);
            int response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                                    response_error_msg, rowkey, colkey,
                                                    g_map_rowkey_to_server, g_coordinator_addr, type);
            if (response_code == 1)
            {
              cerr << "ERROR in communicating with coordinator" << endl;
              continue;
            }
            else if (response_code == 2)
            {
              cerr << "ERROR in communicating with backend" << endl;
              continue;
            }
          }

          // Redirect to home page with the cookie
          redirect_with_cookie(client_fd, "/home", cookie);
        }
        else
        {
          // Password is incorrect- Construct and send the HTTP response
          string content = "{\"error\":\"Incorrect password\"}";
          send_response(client_fd, 401, "Unauthorized", "application/json", content);
        }
      }
      else
      {
        // Error in checking user existence - Construct and send the HTTP response
        string content = "{\"error\":\"Error checking user existence\"}";
        send_response(client_fd, 500, "Internal Server Error", "application/json", content);
      }
    }

    // GET: rendering home page
    else if (html_request_map["uri"] == "/home" && html_request_map["method"] == "GET")
    {
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // Retrieve HTML content from the map
        std::string html_content = g_endpoint_html_map["home"];

        // Construct and send the HTTP response
        send_response(client_fd, 200, "OK", "text/html", html_content);
      }
    }

    // GET: rendering reset-password page
    else if (html_request_map["uri"] == "/reset-password" && html_request_map["method"] == "GET")
    {
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // Retrieve HTML content from the map
        std::string html_content = g_endpoint_html_map["reset-password"];

        // Construct and send the HTTP response
        send_response(client_fd, 200, "OK", "text/html", html_content);
      }
    }

    // POST: reset password
    else if (html_request_map["uri"] == "/reset-password" && html_request_map["method"] == "POST")
    {
      // Parse out formData
      map<string, string> form_data = parse_json_string_to_map(html_request_map["body"]);
      string username = form_data["username"];
      string oldPassword = form_data["oldPassword"];
      string newPassword = form_data["newPassword"];

      // Convert username to lowercase
      transform(username.begin(), username.end(), username.begin(), ::tolower);

      // START COORDINATOR
      string response_value, reset_response_value;
      string response_error_msg, reset_response_error_msg;
      int response_status, reset_response_status;
      rowkey = username + "_info";
      colkey = "password";
      type = "cput";
      F_2_B_Message msg_to_send = construct_msg(4, rowkey, colkey, oldPassword, newPassword, "", 0);
      int response_code = send_msg_to_backend(fd, msg_to_send, reset_response_value, reset_response_status,
                                              reset_response_error_msg, rowkey, colkey,
                                              g_map_rowkey_to_server, g_coordinator_addr, type);
      if (response_code == 1)
      {
        cerr << "ERROR in communicating with coordinator" << endl;
        continue;
      }
      else if (response_code == 2)
      {
        cerr << "ERROR in communicating with backend" << endl;
        continue;
      }

      if (reset_response_status == 1 && strip(reset_response_error_msg) == "Rowkey does not exist")
      {
        // User does not exist - Construct and send the HTTP response
        string content = "{\"error\":\"User does not exist\"}";
        send_response(client_fd, 404, "Not Found", "application/json", content);
      }
      else if (reset_response_status == 1 && strip(reset_response_error_msg) == "Current value is not v1")
      {
        // Old password is wrong - Construct and send the HTTP response
        string content = "{\"error\":\"Current password is wrong!\"}";
        send_response(client_fd, 401, "Unauthorized", "application/json", content);
      }
      else if (reset_response_status == 0)
      {
        // Password reset successful, redirect to login page
        string redirect_to = "/login";
        redirect(client_fd, redirect_to);
      }
      else
      {
        // Error in fetching user - Construct and send the HTTP response
        string content = "{\"error\":\"Error fetching user\"}";
        send_response(client_fd, 500, "Internal Server Error", "application/json", content);
      }
    }
    // GET: logout
    else if (html_request_map["uri"] == "/logout" && html_request_map["method"] == "GET")
    {
      std::string cookie = get_cookie_from_header(request_header);

      if (!cookie.empty())
      {
        // Remove the cookie from local map
        cookie_user_map.erase(cookie);

        // Send a message to the backend to delete the cookie

        string delete_response_value;
        string delete_response_error_msg;
        int delete_response_status;

        rowkey = "cookie";
        colkey = cookie;
        type = "delete";
        F_2_B_Message msg_to_send = construct_msg(3, rowkey, colkey, "", "", "", 0); // Assuming '3' is the DELETE operation

        int response_code = send_msg_to_backend(fd, msg_to_send, delete_response_value, delete_response_status,
                                                delete_response_error_msg, rowkey, colkey,
                                                g_map_rowkey_to_server, g_coordinator_addr, type);
        if (response_code == 1)
        {
          cerr << "ERROR in communicating with coordinator" << endl;
          continue;
        }
        else if (response_code == 2)
        {
          cerr << "ERROR in communicating with backend" << endl;
          continue;
        }

        cout << "Delete response status: " << delete_response_status << endl;
        cout << "Delete response error msg: " << delete_response_error_msg << endl;

        // Redirect to the login page and delete the cookie
        std::string response_stream = "HTTP/1.1 302 Found\r\n";
        response_stream += "Location: http://" + g_serveraddr_str + "/login\r\n";                           // Correct redirect URL
        response_stream += "Set-Cookie: sid=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly\r\n"; // Delete the cookie
        response_stream += "Content-Length: 0\r\n";                                                         // Optional, generally not needed for redirects
        response_stream += "Connection: close\r\n";                                                         // Ensure connection is not kept alive
        response_stream += "\r\n";                                                                          // End of headers
        ssize_t bytes_sent = send(client_fd, response_stream.c_str(), response_stream.length(), 0);
        if (bytes_sent < 0)
        {
          std::cerr << "Failed to send logout response: " << strerror(errno) << std::endl;
        }
      }
      else
      {
        // No cookie found, maybe already logged out or session expired
        send_response(client_fd, 400, "Bad Request", "application/json", "{\"error\":\"No session found\"}");
      }
    }

    else if (html_request_map["uri"] == "/" && html_request_map["method"] == "GET")
    {
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // Redirect to home
        redirect(client_fd, "/home");
      }

      string redirect_to = "/login";
      redirect(client_fd, redirect_to);
    }
    // GET: /inbox
    else if (html_request_map["uri"] == "/inbox" && html_request_map["method"] == "GET")
    {
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        F_2_B_Message msg_to_send, response_msg;
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd);
        if (!username.empty())
        {
          string inbox_emails_str, response_error_msg;
          int response_status;
          rowkey = username + "_email";
          colkey = "inbox_items";
          type = "get";
          F_2_B_Message msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
          int response_code = send_msg_to_backend(fd, msg_to_send, inbox_emails_str, response_status,
                                                  response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                  g_coordinator_addr, type);
          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }

          string html_content = generate_inbox_html(inbox_emails_str);
          send_response(client_fd, 200, "OK", "text/html", html_content);
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }
    // GET: /sentbox
    else if (html_request_map["uri"] == "/sentbox" && html_request_map["method"] == "GET")
    {
      F_2_B_Message msg_to_send, response_msg;
      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          string sentbox_emails_str, response_error_msg;
          int response_status;
          rowkey = username + "_email";
          colkey = "sentbox_items";
          type = "get";
          F_2_B_Message msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
          int response_code = send_msg_to_backend(fd, msg_to_send, sentbox_emails_str, response_status,
                                                  response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                  g_coordinator_addr, type);
          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }

          string html_content = generate_sentbox_html(sentbox_emails_str);
          send_response(client_fd, 200, "OK", "text/html", html_content);
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }

    // GET: /compose
    else if (html_request_map["uri"].substr(0, 8) == "/compose" && html_request_map["method"] == "GET")
    {
      // /compose?mode=reply&email_id=123
      F_2_B_Message msg_to_send, response_msg;
      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          F_2_B_Message msg_to_send;

          string prefill_to = "";
          string prefill_subject = "";
          string prefill_body = "";

          if (html_request_map["uri"] != "/compose")
          {
            string mode, uid;
            vector<string> params = split(html_request_map["uri"].substr(9), "&");
            mode = split(params[0], "=")[1];
            uid = split(params[1], "=")[1];
            // Get Subject
            string encoded_subject, response_error_msg;
            int response_status;
            rowkey = "email/" + uid;
            colkey = "subject";
            type = "get";
            msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
            int response_code = send_msg_to_backend(fd, msg_to_send, encoded_subject, response_status,
                                                    response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                    g_coordinator_addr, type);
            if (response_code == 1)
            {
              cerr << "ERROR in communicating with coordinator" << endl;
              continue;
            }
            else if (response_code == 2)
            {
              cerr << "ERROR in communicating with backend" << endl;
              continue;
            }
            string subject = base64_decode(encoded_subject);

            // Get Display Message
            string encoded_display;
            rowkey = "email/" + uid;
            colkey = "display";
            type = "get";
            msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
            response_code = send_msg_to_backend(fd, msg_to_send, encoded_display, response_status,
                                                response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);
            if (response_code == 1)
            {
              cerr << "ERROR in communicating with coordinator" << endl;
              continue;
            }
            else if (response_code == 2)
            {
              cerr << "ERROR in communicating with backend" << endl;
              continue;
            }
            string display = base64_decode(encoded_display);

            // Get Sender
            string encoded_sender;
            rowkey = "email/" + uid;
            colkey = "from";
            type = "get";
            msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
            response_code = send_msg_to_backend(fd, msg_to_send, encoded_sender, response_status,
                                                response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);
            if (response_code == 1)
            {
              cerr << "ERROR in communicating with coordinator" << endl;
              continue;
            }
            else if (response_code == 2)
            {
              cerr << "ERROR in communicating with backend" << endl;
              continue;
            }
            string sender = base64_decode(encoded_sender);

            if (mode == "reply")
            {
              prefill_to = sender;
              prefill_subject = "RE: " + subject;
              prefill_body = "\n--------------- REPLYING TO MSG ---------------\n" + display;
            }
            else
            {
              prefill_subject = "FWD: " + subject;
              prefill_body = "\n--------------- FORWARDING MSG ---------------\n" + display;
            }
          }
          string html_content = generate_compose_html(prefill_to, prefill_subject, prefill_body);
          send_response(client_fd, 200, "OK", "text/html", html_content);
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }

    // POST: /compose
    else if (html_request_map["uri"].substr(0, 8) == "/compose" && html_request_map["method"] == "POST")
    {
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "http://" + g_serveraddr_str + "/login");
      }
      else
      {
        string from_username = get_username_from_cookie(cookie, fd);
        string from = from_username + "@localhost";
        string encoded_from = base64_encode(from);

        F_2_B_Message msg_to_send;
        string response_value, response_error_msg;
        int response_status, response_code;

        map<string, string> form_data = parse_json_string_to_map(html_request_map["body"]);
        string to = form_data["to"];
        string encoded_to = base64_encode(to);
        vector<vector<string>> recipients = parse_recipients_str_to_vec(to);

        string invalid_recipients = "";
        for (const auto &usrname : recipients[0])
        {
          rowkey = usrname + "_info";
          colkey = "email";
          type = "get";
          msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
          response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                              response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                              g_coordinator_addr, type);
          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }

          if (response_status == 1 && strip(response_error_msg) == "Rowkey does not exist")
          {
            invalid_recipients += usrname + "@localhost;";
          }
        }
        for (const auto &r : recipients[1])
        {
          if (!is_valid_email(r))
          {
            invalid_recipients += r;
          }
        }

        if (!invalid_recipients.empty())
        {

          string content = "{\"error\":\"These recipents do not exist:\\n" + invalid_recipients + "\\nPlease enter valid recipients.\"}";
          send_response(client_fd, 400, "Not Found", "application/json", content);
        }
        else
        {

          string ts_sentbox = get_timestamp();
          string encoded_ts = base64_encode(ts_sentbox);

          string subject = form_data["subject"];
          string encoded_subject = base64_encode(subject);

          string body = form_data["body"];
          string encoded_body = base64_encode(body);

          string for_display = format_mail_for_display(subject, from, ts_sentbox, body);
          string encoded_display = base64_encode(for_display);

          string uid = compute_md5_hash(for_display);

          // deliver for external recipients
          for (const auto &r : recipients[1])
          {
            string dummy_from = from_username + "@seas.upenn.edu";
            pthread_t thread_id;
            auto *data = new std::map<std::string, std::string>{
                {"to", r},
                {"from", dummy_from},
                {"subject", subject},
                {"content", body}};
            if (pthread_create(&thread_id, nullptr, smtp_client, data) != 0)
            {
              std::cerr << "Failed to create thread: " << std::strerror(errno) << std::endl;
              delete data;
            }
            else
            {
              pthread_detach(thread_id);
            }
          }
          // deliver for local recipients
          for (const auto &usrname : recipients[0])
          {
            int deliver_success = deliver_local_email(usrname, uid, encoded_from, encoded_subject, encoded_body,
                                                      encoded_display, g_map_rowkey_to_server, g_coordinator_addr);
            if (deliver_success == 0)
            {
              cout << "SUCCESS: delivered local mail to " << usrname << endl;
            }
            else
            {
              cout << "ERROR: failed to deliver local mail to " << usrname << endl;
            }
          }

          // store email
          int store_email_success = put_email_to_backend(uid, encoded_from, encoded_to, encoded_ts,
                                                         encoded_subject, encoded_body, encoded_display,
                                                         g_map_rowkey_to_server, g_coordinator_addr);
          if (store_email_success == 0)
          {
            cout << "SUCCESS: stored email with uid " << uid << endl;
          }
          else
          {
            cout << "ERROR: failed to store email with uid " << uid << endl;
          }

          // put in sentbox
          int sentbox_success = put_in_sentbox(from_username, uid, encoded_to, encoded_ts,
                                               encoded_subject, encoded_body, g_map_rowkey_to_server,
                                               g_coordinator_addr);
          if (sentbox_success == 0)
          {
            cout << "SUCCESS: stored email with uid " << uid << " in sentbox of " << from_username << endl;
          }
          else
          {
            cout << "ERROR: failed to store email with uid " << uid << " in sentbox of " << from_username << endl;
          }

          std::string redirect_to = "/inbox";
          redirect(client_fd, redirect_to);
        }
      }
    }

    // GET: view_email
    else if (html_request_map["uri"].substr(0, 11) == "/view_email" && html_request_map["method"] == "GET")
    {
      F_2_B_Message msg_to_send, response_msg;

      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          /// view_email/?source=inbox&id=
          string source = split(split(split(html_request_map["uri"], "?")[1], "&")[0], "=")[1];
          string uid = split(split(split(html_request_map["uri"], "?")[1], "&")[1], "=")[1];

          F_2_B_Message msg_to_send;
          string response_error_msg;
          int response_status, response_code;
          rowkey = "email/" + uid;
          type = "get";

          // get subject
          string encoded_subject;
          colkey = "subject";
          msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
          response_code = send_msg_to_backend(fd, msg_to_send, encoded_subject, response_status,
                                              response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                              g_coordinator_addr, type);
          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }
          string subject = base64_decode(encoded_subject);

          // get from
          string encoded_from;
          colkey = "from";
          msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
          response_code = send_msg_to_backend(fd, msg_to_send, encoded_from, response_status,
                                              response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                              g_coordinator_addr, type);
          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }
          string from = base64_decode(encoded_from);

          // get to
          string encoded_to;
          colkey = "to";
          msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
          response_code = send_msg_to_backend(fd, msg_to_send, encoded_to, response_status,
                                              response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                              g_coordinator_addr, type);
          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }
          string to = base64_decode(encoded_to);

          // get timestamp
          string encoded_timestamp;
          colkey = "timestamp";
          msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
          response_code = send_msg_to_backend(fd, msg_to_send, encoded_timestamp, response_status,
                                              response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                              g_coordinator_addr, type);
          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }
          string timestamp = base64_decode(encoded_timestamp);

          // get body
          string encoded_body;
          colkey = "body";
          msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
          response_code = send_msg_to_backend(fd, msg_to_send, encoded_body, response_status,
                                              response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                              g_coordinator_addr, type);
          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }
          string body = base64_decode(encoded_body);
          // display the email content
          string html_content = construct_view_email_html(subject, from, to, timestamp, body, uid, source);
          send_response(client_fd, 200, "OK", "text/html", html_content);
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }
    else if (html_request_map["uri"].substr(0, 13) == "/delete_email" && html_request_map["method"] == "GET")
    {
      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          // /delete_email?source=" << source << "&id=" << uid << "
          string source = split(split(split(html_request_map["uri"], "?")[1], "&")[0], "=")[1];
          string uid = split(split(split(html_request_map["uri"], "?")[1], "&")[1], "=")[1];
          cout << "delete email with uid: " << uid << " from " << source << endl;
          delete_email(username, uid, source, g_map_rowkey_to_server, g_coordinator_addr);
          std::string redirect_to = "/" + source;
          redirect(client_fd, redirect_to);
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }
    // ENDMAIL

    // GET: display drive page
    else if (html_request_map["uri"].find("/drive") == 0 && html_request_map["method"] == "GET")
    {
      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        cout << "Going from drive to home" << endl;
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::cout << "username: " << cookie_user_map[cookie] << " " << cookie << std::endl;
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          std::string uri_path = html_request_map["uri"];

          // Extract path from the URI path
          std::string path = uri_path.substr(1 + uri_path.find_first_of("/"));

          // Construct the row key by appending "user_" to the foldername
          std::string rowkey = username + "_" + path;

          // Fetch items for the foldername from the backend
          F_2_B_Message msg_to_send;
          string response_value, response_error_msg;
          int response_status, response_code;
          type = "get";
          colkey = "items";
          msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
          response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                              response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                              g_coordinator_addr, type);
          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }

          if (response_status == 0)
          {
            // Parse the response to get the list of items
            std::vector<std::string> items = parse_items_list(response_value);

            // Generate HTML for displaying folders and files
            std::stringstream html_content;

            // Append upload file and create folder buttons
            html_content << "<button onclick=\"uploadFile('" << path << "')\">Upload File</button>";
            html_content << "<button onclick=\"createFolder()\">Create Folder</button>";

            // Add a loading indicator element
            html_content << "<div id='loadingIndicator' style='display:none;'>Loading...</div>";

            // JavaScript functions for upload and create folder actions
            html_content << "<script>";
            html_content << "function uploadFile(path) { ";
            html_content << "var input = document.createElement('input');";
            html_content << "input.type = 'file';";
            html_content << "input.onchange = function(event) {";
            html_content << "var file = event.target.files[0];";
            html_content << "if (file) {";
            html_content << "uploadFileRequest(file, file.name, path);";
            html_content << "}";
            html_content << "};";
            html_content << "input.click();";
            html_content << "}";
            html_content << "function uploadFileRequest(file, filename, path) {";
            html_content << "var formData = new FormData();";
            html_content << "formData.append('filename', filename);";
            html_content << "formData.append('path', path);";
            html_content << "let blobVar = new Blob([file], { type: file.type });";
            html_content << "formData.append('file', blobVar, file.name);";

            // Show the loading indicator
            html_content << "document.getElementById('loadingIndicator').style.display = 'block';";

            html_content << "fetch('/upload_file', {";
            html_content << "method: 'POST',";
            html_content << "body: formData";
            html_content << "})";
            html_content << ".then(response => {";
            html_content << "document.getElementById('loadingIndicator').style.display = 'none';"; // Hide the loading indicator
            html_content << "if (response.ok) {";
            html_content << "window.location.reload(true);";
            html_content << "} else {";
            html_content << "response.json().then(data => {";
            html_content << "alert('Failed to upload file: ' + data.error)});";
            html_content << "}";
            html_content << "})";
            html_content << ".catch(error => {";
            html_content << "document.getElementById('loadingIndicator').style.display = 'none';"; // Hide the loading indicator on error
            html_content << "console.error('Error:', error);";
            html_content << "});";
            html_content << "}";
            html_content << "</script>";

            html_content << "<script>";
            html_content << "function createFolder() { ";
            html_content << "var folderName = prompt('Enter folder name:');"; // Display a prompt to enter folder name
            html_content << "if (folderName) {";                              // Check if folder name is provided
            html_content << "createFolderRequest(folderName);";               // Call function to send POST request
            html_content << "}";
            html_content << "}";
            html_content << "function createFolderRequest(folderName) {"; // Function to send POST request
            html_content << "fetch('/create_folder', {";                  // Send POST request to create_folder endpoint
            html_content << "method: 'POST',";
            html_content << "headers: {";
            html_content << "'Content-Type': 'application/json'"; // Specify content type as JSON
            html_content << "},";
            html_content << "body: JSON.stringify({";  // Pass query parameters as JSON data
            html_content << "folderName: folderName,"; // Folder name
            html_content << "path: '" << path << "'";  // Path of parent folder
            html_content << "})";
            html_content << "})";
            html_content << ".then(response => {";
            html_content << "console.log(response);";
            html_content << "if (response.ok) {";
            html_content << "window.location.reload(true);"; // Refresh the page after successful create folder request
            html_content << "} else {";
            html_content << "response.json().then(data => {"; // Show error message
            html_content << "alert('Failed to create folder: ' + data.error)});";
            html_content << "}";
            html_content << "})";
            html_content << ".catch(error => {";
            html_content << "console.error('Error:', error);"; // Log error to console
            html_content << "});";
            html_content << "}";
            html_content << "</script>";

            // Check if the response contains a list of items (folders)
            if (!items.empty())
            {
              html_content << "<ul>";

              for (const auto &item : items)
              {
                std::string item_type = item.substr(0, 2);
                std::string item_name = item.substr(2);

                // To do: folder icon not displaying

                if (item_type == "D@")
                {
                  html_content << "<li><img src=\"https://www.creativefabrica.com/wp-content/uploads/2021/06/22/Folder-Icon-Template-Design-Vector-Graphics-13725642-1-1-580x387.jpg\" alt=\"Folder\" width=\"20\" height=\"20\"> <a href=\"#\" onclick=\"showFolderOptions('" << path << "/" << item_name << "');\">" << item_name << "</a></li>";
                }
                else if (item_type == "F@")
                {
                  html_content << "<li><img src=\"https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcRv3qczswxiDVO0twxBealS6lUxxOOm5rrnRrTbRoMjKA&s\" alt=\"File\" width=\"20\" height=\"20\"> <a href=\"#\" onclick=\"showFileOptions('" << path << "/" << item_name << "');\">" << item_name << "</a></li>";
                }
              }
              html_content << "</ul>";
            }

            // JavaScript function for showing folder options
            html_content << "<script>";
            html_content << "function showFolderOptions(folderPath) {";
            html_content << "var options = ['Delete', 'Move', 'Rename', 'Show'];";
            html_content << "var choice = prompt('Select an option:\\n1. Delete\\n2. Move\\n3. Rename\\n4. Show');";
            html_content << "if (choice) {";
            html_content << "var optionIndex = parseInt(choice) - 1;";
            html_content << "var selectedOption = options[optionIndex];";
            html_content << "if (selectedOption === 'Delete') {";
            html_content << "if (confirm('Are you sure you want to delete the folder?')) {";
            html_content << "deleteFolder(folderPath);"; // Call delete folder function
            html_content << "}";
            html_content << "} else if (selectedOption === 'Rename') {";
            html_content << "var newFolderName = prompt('Enter the new name for the folder:');";
            html_content << "if (newFolderName) {";
            html_content << "renameFolder(folderPath, newFolderName);"; // Call rename folder function
            html_content << "}";
            html_content << "} else if (selectedOption === 'Move') {";
            html_content << "var newPath = prompt('Enter the absolute path where you want to move the folder:');";
            html_content << "if (newPath) {";
            html_content << "moveFolder(folderPath, newPath);"; // Call move folder function
            html_content << "}";
            html_content << "} else if (selectedOption === 'Show') {";
            html_content << "window.location.href = '/' + folderPath;"; // Redirect to folder path without encoding slashes
            html_content << "} else {";
            html_content << "alert(selectedOption + ' selected for folder: ' + folderPath);"; // Placeholder for other options
            html_content << "}";
            html_content << "}";
            html_content << "}";
            html_content << "</script>";

            // JavaScript function for deleting a folder
            html_content << "<script>";
            html_content << "function deleteFolder(folderPath) {";
            // Show the loading indicator when the deletion process starts
            html_content << "document.getElementById('loadingIndicator').style.display = 'block';";

            html_content << "fetch('/delete_folder', {";
            html_content << "method: 'POST',";
            html_content << "headers: {";
            html_content << "'Content-Type': 'application/json'";
            html_content << "},";
            html_content << "body: JSON.stringify({";
            html_content << "'folderPath': folderPath";
            html_content << "})";
            html_content << "})";
            html_content << ".then(response => {";
            html_content << "document.getElementById('loadingIndicator').style.display = 'none';"; // Hide the loading indicator
            html_content << "if (response.ok) {";
            html_content << "alert('Folder deleted successfully');";
            html_content << "window.location.reload(true);";
            html_content << "} else {";
            html_content << "response.json().then(data => {";
            html_content << "alert('Failed to delete folder: ' + data.error)});";
            html_content << "}";
            html_content << "})";
            html_content << ".catch(error => {";
            html_content << "document.getElementById('loadingIndicator').style.display = 'none';"; // Hide the loading indicator on error
            html_content << "console.error('Error:', error);";
            html_content << "});";
            html_content << "}";
            html_content << "</script>";

            // JavaScript function for renaming a folder
            html_content << "<script>";
            html_content << "function renameFolder(folderPath, newFolderName) {";
            html_content << "fetch('/rename_folder', {"; // Send POST request to rename_folder endpoint
            html_content << "method: 'POST',";
            html_content << "headers: {";
            html_content << "'Content-Type': 'application/json'";
            html_content << "},";
            html_content << "body: JSON.stringify({";
            html_content << "'folderPath': folderPath,";      // Include folder path in JSON data
            html_content << "'newFolderName': newFolderName"; // Include new folder name in JSON data
            html_content << "})";
            html_content << "})";
            html_content << ".then(response => {";
            html_content << "if (response.ok) {";
            html_content << "alert('Folder renamed successfully');"; // Show success message
            html_content << "window.location.reload(true);";         // Refresh the page after successful renaming
            html_content << "} else {";
            html_content << "response.json().then(data => {"; // Show error message
            html_content << "alert('Failed to rename folder: ' + data.error)});";
            html_content << "}";
            html_content << "})";
            html_content << ".catch(error => {";
            html_content << "console.error('Error:', error);"; // Log error to console
            html_content << "});";
            html_content << "}";
            html_content << "</script>";

            // JavaScript function for moving a folder
            html_content << "<script>";
            html_content << "function moveFolder(folderPath, newPath) {";
            html_content << "fetch('/move_folder', {"; // Send POST request to move_folder endpoint
            html_content << "method: 'POST',";
            html_content << "headers: {";
            html_content << "'Content-Type': 'application/json'";
            html_content << "},";
            html_content << "body: JSON.stringify({";
            html_content << "'folderPath': folderPath,"; // Include folder path in JSON data
            html_content << "'newPath': newPath";        // Include new path in JSON data
            html_content << "})";
            html_content << "})";
            html_content << ".then(response => {";
            html_content << "if (response.ok) {";
            html_content << "alert('Folder moved successfully');"; // Show success message
            html_content << "window.location.reload(true);";       // Refresh the page after successful renaming
            html_content << "} else {";
            html_content << "response.json().then(data => {"; // Show error message
            html_content << "alert('Failed to move folder: ' + data.error)});";
            html_content << "}";
            html_content << "})";
            html_content << ".catch(error => {";
            html_content << "console.error('Error:', error);"; // Log error to console
            html_content << "});";
            html_content << "}";
            html_content << "</script>";

            // JavaScript function for showing file options
            html_content << "<script>";
            html_content << "function showFileOptions(filePath) {";
            html_content << "var options = ['Delete', 'Move', 'Rename', 'Download'];";
            html_content << "var choice = prompt('Select an option:\\n1. Delete\\n2. Move\\n3. Rename\\n4. Download');";
            html_content << "if (choice) {";
            html_content << "var optionIndex = parseInt(choice) - 1;";
            html_content << "var selectedOption = options[optionIndex];";
            html_content << "if (selectedOption === 'Delete') {";
            html_content << "if (confirm('Are you sure you want to delete the file?')) {";
            html_content << "deleteFile(filePath);"; // Call delete file function
            html_content << "}";
            html_content << "} else if (selectedOption === 'Download') {";
            html_content << "downloadFile(filePath);"; // Call download file function
            html_content << "} else if (selectedOption === 'Rename') {";
            html_content << "var newFileName = prompt('Enter the new name for the file:');";
            html_content << "if (newFileName) {";
            html_content << "renameFile(filePath, newFileName);"; // Call rename file function
            html_content << "}";
            html_content << "} else if (selectedOption === 'Move') {";
            html_content << "var newFilePath = prompt('Enter the new path for the file:');";
            html_content << "if (newFilePath) {";
            html_content << "moveFile(filePath, newFilePath);"; // Call move file function
            html_content << "}";
            html_content << "} else {";
            html_content << "alert(selectedOption + ' selected for file: ' + filePath);"; // Placeholder for other options
            html_content << "}";
            html_content << "}";
            html_content << "}";
            html_content << "</script>";

            // JavaScript function for deleting a file
            html_content << "<script>";
            html_content << "function deleteFile(filePath) {";
            html_content << "fetch('/delete_file', {"; // Send POST request to delete_file endpoint
            html_content << "method: 'POST',";
            html_content << "headers: {";
            html_content << "'Content-Type': 'application/json'";
            html_content << "},";
            html_content << "body: JSON.stringify({";
            html_content << "'filePath': filePath"; // Include file path in JSON data
            html_content << "})";
            html_content << "})";
            html_content << ".then(response => {";
            html_content << "if (response.ok) {";
            html_content << "alert('File deleted successfully');"; // Show success message
            html_content << "window.location.reload(true);";       // Refresh the page after successful deletion
            html_content << "} else {";
            html_content << "response.json().then(data => {"; // Show error message
            html_content << "alert('Failed to delete file: ' + data.error)});";
            html_content << "}";
            html_content << "})";
            html_content << ".catch(error => {";
            html_content << "console.error('Error:', error);"; // Log error to console
            html_content << "});";
            html_content << "}";
            html_content << "</script>";

            // JavaScript function for moving a file
            html_content << "<script>";
            html_content << "function moveFile(filePath, newFilePath) {";
            html_content << "fetch('/move_file', {"; // Send POST request to move_file endpoint
            html_content << "method: 'POST',";
            html_content << "headers: {";
            html_content << "'Content-Type': 'application/json'";
            html_content << "},";
            html_content << "body: JSON.stringify({";
            html_content << "'filePath': filePath,";      // Include current file path in JSON data
            html_content << "'newFilePath': newFilePath"; // Include new file path in JSON data
            html_content << "})";
            html_content << "})";
            html_content << ".then(response => {";
            html_content << "if (response.ok) {";
            html_content << "alert('File moved successfully');"; // Show success message
            html_content << "window.location.reload(true);";     // Refresh the page after successful move
            html_content << "} else {";
            html_content << "response.json().then(data => {"; // Show error message
            html_content << "alert('Failed to move file: ' + data.error)});";
            html_content << "}";
            html_content << "})";
            html_content << ".catch(error => {";
            html_content << "console.error('Error:', error);"; // Log error to console
            html_content << "});";
            html_content << "}";
            html_content << "</script>";

            // JavaScript function for renaming a file
            html_content << "<script>";
            html_content << "function renameFile(filePath, newFileName) {";
            html_content << "fetch('/rename_file', {"; // Send POST request to rename_file endpoint
            html_content << "method: 'POST',";
            html_content << "headers: {";
            html_content << "'Content-Type': 'application/json'";
            html_content << "},";
            html_content << "body: JSON.stringify({";
            html_content << "'filePath': filePath,";      // Include file path in JSON data
            html_content << "'newFileName': newFileName"; // Include new file name in JSON data
            html_content << "})";
            html_content << "})";
            html_content << ".then(response => {";
            html_content << "if (response.ok) {";
            html_content << "alert('File renamed successfully');"; // Show success message
            html_content << "window.location.reload(true);";       // Refresh the page after successful renaming
            html_content << "} else {";
            html_content << "response.json().then(data => {"; // Show error message
            html_content << "alert('Failed to rename file: ' + data.error)});";
            html_content << "}";
            html_content << "})";
            html_content << ".catch(error => {";
            html_content << "console.error('Error:', error);"; // Log error to console
            html_content << "});";
            html_content << "}";
            html_content << "</script>";

            // JavaScript function for downloading a file
            html_content << "<script> function downloadFile(filePath)";
            html_content << "{";
            html_content << " if (confirm('Do you want to download this file?'))";
            html_content << "{";
            html_content << " window.location.href = '/download?file=' + encodeURIComponent(filePath);";
            html_content << "}";
            html_content << "}";
            html_content << "</script>";

            // Send the HTTP response with the generated HTML content
            send_response(client_fd, 200, "OK", "text/html", html_content.str());
          }
          else
          {
            // Error handling: send appropriate error response to the client
            std::string error_message = "Failed to fetch items for folder ";
            send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
          }
        }
        else
        {
          // Handle unauthenticated or failed lookup
          cout << "Drive me error" << endl;
          redirect(client_fd, "/login");
        }
      }
    }
    // Handle POST request to create a new folder
    else if (html_request_map["uri"] == "/create_folder" && html_request_map["method"] == "POST")
    {
      // Extract folder name and path from the request body
      map<string, string> post_data = parse_json_string_to_map(html_request_map["body"]);
      string folderName = post_data["folderName"];
      string path = post_data["path"];
      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          // Construct the row key by appending "user_" to the foldername
          string parentRowKey = username + "_" + path;
          std::cout << "parent row key: " << parentRowKey << std::endl;

          // Construct the row key for the new folder
          string newFolderRowKey = parentRowKey + "/" + folderName;

          F_2_B_Message msg_to_send;
          string response_value, response_error_msg;
          int response_code, response_status;
          // Check whether folder already exists in this directory
          type = "get";
          colkey = "items";
          msg_to_send = construct_msg(1, newFolderRowKey, colkey, "", "", "", 0);
          response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                              response_error_msg, newFolderRowKey, colkey, g_map_rowkey_to_server,
                                              g_coordinator_addr, type);
          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }

          if (response_status == 0)
          {
            // folder already exists
            string content = "{\"error\":\"Folder already exists!!\"}";
            send_response(client_fd, 409, "Conflict", "application/json", content);
          }
          else if (response_status == 1)
          {
            // Loop till success: GET the parent folder
            F_2_B_Message msg_to_send;
            string response_value, response_error_msg;
            int response_status, response_code;
            bool success = false;

            while (!success)
            {
              type = "get";
              colkey = "items";
              msg_to_send = construct_msg(1, parentRowKey, colkey, "", "", "", 0);
              response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                                  response_error_msg, parentRowKey, colkey, g_map_rowkey_to_server,
                                                  g_coordinator_addr, type);
              if (response_code == 1)
              {
                cerr << "ERROR in communicating with coordinator" << endl;
                continue;
              }
              else if (response_code == 2)
              {
                cerr << "ERROR in communicating with backend" << endl;
                continue;
              }
              // GET request for the parent folder

              if (response_status == 0)
              {
                std::string newValue;
                if (response_value == "[]")
                {
                  // If the list is empty, simply add the new value with square brackets
                  newValue = "[D@" + folderName + "]";
                }
                else
                {
                  // Find the position of the last closing bracket
                  size_t last_bracket_pos = response_value.find_last_of(']');

                  // Extract the substring up to the last closing bracket (excluding)
                  std::string existing_values = response_value.substr(0, last_bracket_pos);

                  // Concatenate the new value with a comma
                  newValue = existing_values + ",D@" + folderName + "]";
                }
                // CPUT the parent folder
                string cput_response_value, cput_response_error_msg;
                int cput_response_status;
                type = "cput";
                colkey = "items";
                msg_to_send = construct_msg(4, parentRowKey, colkey, response_value, newValue, "", 0);
                response_code = send_msg_to_backend(fd, msg_to_send, cput_response_value, cput_response_status,
                                                    cput_response_error_msg, parentRowKey, colkey, g_map_rowkey_to_server,
                                                    g_coordinator_addr, type);
                if (response_code == 1)
                {
                  cerr << "ERROR in communicating with coordinator" << endl;
                  continue;
                }
                else if (response_code == 2)
                {
                  cerr << "ERROR in communicating with backend" << endl;
                  continue;
                }

                if (cput_response_status == 0)
                {
                  success = true;
                }

                else if (cput_response_status == 1 && strip(cput_response_error_msg) == "Rowkey does not exist")
                {
                  // Parent folder does not exist - Construct and send the HTTP response
                  string content = "{\"error\":\"Parent folder does not exist\"}";
                  send_response(client_fd, 404, "Not Found", "application/json", content);
                }
                else if (cput_response_status == 1 && strip(cput_response_error_msg) == "Current value is not v1")
                {
                  // Old value is wrong - Construct and send the HTTP response
                  string content = "{\"error\":\"Current items in parent folder are wrong!\"}";
                  send_response(client_fd, 401, "Unauthorized", "application/json", content);
                }
                else
                {
                  // Error in fetching user - Construct and send the HTTP response
                  string content = "{\"error\":\"Error fetching user\"}";
                  send_response(client_fd, 500, "Internal Server Error", "application/json", content);
                }
              }
              else
              {
                // Error in fetching user - Construct and send the HTTP response
                string content = "{\"error\":\"Error fetching user\"}";
                send_response(client_fd, 500, "Internal Server Error", "application/json", content);
              }
            }

            // Construct the row key for the new folder
            string newFolderRowKey = parentRowKey + "/" + folderName;

            // PUT for new folder item with empty list
            type = "put";
            colkey = "items";
            msg_to_send = construct_msg(2, newFolderRowKey, colkey, "[]", "", "", 0);
            response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                                response_error_msg, newFolderRowKey, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);
            if (response_code == 1)
            {
              cerr << "ERROR in communicating with coordinator" << endl;
              continue;
            }
            else if (response_code == 2)
            {
              cerr << "ERROR in communicating with backend" << endl;
              continue;
            }

            if (response_status == 0)
            {
              // Success response
              string refresh_script = "<script>window.location.reload(true);</script>";
              send_response(client_fd, 200, "OK", "text/html", refresh_script);
            }
            else
            {
              // Error handling: send appropriate error response to the client
              std::string error_message = "Failed to create folder";
              send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
            }
          }
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }
    // Handle POST request to upload a file
    else if (html_request_map["uri"] == "/upload_file" && html_request_map["method"] == "POST")
    {
      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          // Parse the multipart form data from the request
          std::string content_type = html_request_map["header_Content-Type"];
          int content_length = stoi(html_request_map["header_Content-Length"]);
          std::string boundary = extract_boundary(content_type);

          std::string filename;
          std::string path;
          bool file_ready_to_read = recv_file_name_path(client_fd, content_length, boundary, filename, path);

          // cout << "Boundary used for parsing: " << boundary << endl;

          // auto parts = parse_multipart_form_data(html_request_map["body"], boundary);

          // for (const auto &part : parts)
          // {
          //   cout << "Part: " << part.first << " Content: ";
          //   cout.write(part.second.content.data(), part.second.content.size());
          //   cout << endl;
          // }

          // Getting the file from FormData
          if (file_ready_to_read)
          {
            // auto &filePart = parts["file"];
            // auto &filenamePart = parts["filename"];
            // auto &pathPart = parts["path"];

            // std::string fileContent = base64_encode(std::string(filePart.content.begin(), filePart.content.end()));

            cout << "FILENAME: " << filename << endl;
            cout << "FILEPATH: " << path << endl;
            // cout << "FILECONTENT: " << fileContent << endl;

            // Construct the row key by appending "user_" to the folder name
            std::string parentRowKey = username + "_" + path;
            std::string file_row_key = parentRowKey + "/" + filename;

            F_2_B_Message msg_to_send;
            string get_response_value, get_response_error_msg;
            int response_code, get_response_status;

            // Check whether file already exists in this directory
            type = "get";
            colkey = "no_chunks";
            msg_to_send = construct_msg(1, file_row_key, colkey, "", "", "", 0);
            response_code = send_msg_to_backend(fd, msg_to_send, get_response_value, get_response_status,
                                                get_response_error_msg, file_row_key, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);
            if (response_code == 1)
            {
              cerr << "ERROR in communicating with coordinator" << endl;
              continue;
            }
            else if (response_code == 2)
            {
              cerr << "ERROR in communicating with backend" << endl;
              continue;
            }

            std::cout << "RESPONSE: " << get_response_error_msg << std::endl;

            if (get_response_status == 0)
            {
              // file already exists
              string content = "{\"error\":\"File already exists!!\"}";
              send_response(client_fd, 409, "Conflict", "application/json", content);
            }
            else if (get_response_status == 1)
            {
              // Loop till success: GET the parent folder
              bool success = false;
              string response_value, response_error_msg;
              int response_code, response_status;

              while (!success)
              {
                // GET request for the parent folder
                type = "get";
                colkey = "items";
                msg_to_send = construct_msg(1, parentRowKey, colkey, "", "", "", 0);
                response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                                    response_error_msg, parentRowKey, colkey, g_map_rowkey_to_server,
                                                    g_coordinator_addr, type);
                if (response_code == 1)
                {
                  cerr << "ERROR in communicating with coordinator" << endl;
                  continue;
                }
                else if (response_code == 2)
                {
                  cerr << "ERROR in communicating with backend" << endl;
                  continue;
                }

                if (response_status == 0)
                {
                  std::string newValue;

                  if (response_value == "[]")
                  {
                    // If the list is empty, simply add the new value with square brackets
                    newValue = "[F@" + filename + "]";
                  }
                  else
                  {
                    // Find the position of the last closing bracket
                    size_t last_bracket_pos = response_value.find_last_of(']');

                    // Extract the substring up to the last closing bracket (excluding)
                    std::string existing_values = response_value.substr(0, last_bracket_pos);

                    // Concatenate the new value with a comma
                    newValue = existing_values + ",F@" + filename + "]";
                  }

                  // CPUT the parent folder
                  string cput_response_value, cput_response_error_msg;
                  int cput_response_status;
                  type = "cput";
                  colkey = "items";
                  msg_to_send = construct_msg(4, parentRowKey, colkey, response_value, newValue, "", 0);
                  response_code = send_msg_to_backend(fd, msg_to_send, cput_response_value, cput_response_status,
                                                      cput_response_error_msg, parentRowKey, colkey, g_map_rowkey_to_server,
                                                      g_coordinator_addr, type);
                  if (response_code == 1)
                  {
                    cerr << "ERROR in communicating with coordinator" << endl;
                    continue;
                  }
                  else if (response_code == 2)
                  {
                    cerr << "ERROR in communicating with backend" << endl;
                    continue;
                  }

                  if (cput_response_status == 0)
                  {
                    success = true;
                  }
                  else if (cput_response_status == 1 && strip(cput_response_error_msg) == "Rowkey does not exist")
                  {
                    // Parent folder does not exist - Construct and send the HTTP response
                    string content = "{\"error\":\"Parent folder does not exist\"}";
                    send_response(client_fd, 404, "Not Found", "application/json", content);
                  }
                  else if (cput_response_status == 1 && strip(cput_response_error_msg) == "Current value is not v1")
                  {
                    // Old value is wrong - Construct and send the HTTP response
                    string content = "{\"error\":\"Current items in parent folder are wrong!\"}";
                    send_response(client_fd, 401, "Unauthorized", "application/json", content);
                  }
                  else
                  {
                    // Error in fetching user - Construct and send the HTTP response
                    string content = "{\"error\":\"Error fetching user\"}";
                    send_response(client_fd, 500, "Internal Server Error", "application/json", content);
                  }
                }
              }

              bool file_transfer_successfully = file_chunk_storing(client_fd, fd, content_length, file_row_key, boundary);

              if (file_transfer_successfully)
              {
                // Success response
                string refresh_script = "<script>window.location.reload(true);</script>";
                send_response(client_fd, 200, "OK", "text/html", refresh_script);
              }
              else
              {
                // Error handling: send appropriate error response to the client
                std::string error_message = "Failed to upload file";
                send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
              }
            }
            else
            {
              // Error in fetching user - Construct and send the HTTP response
              string content = "{\"error\":\"Operation failed\"}";
              send_response(client_fd, 500, "Internal Server Error", "application/json", content);
            }
          }
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }
    // Handle GET request for downloading a file
    else if (html_request_map["uri"].find("/download") == 0 && html_request_map["method"] == "GET")
    {
      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          // Extract the file path from the request URI
          std::string file_path_encoded = html_request_map["uri"].substr(strlen("/download?file="));
          std::string file_path = url_decode(file_path_encoded);

          // Extract the file name from the file path
          size_t last_separator_pos = file_path.find_last_of('/');
          std::string file_name = file_path.substr(last_separator_pos + 1);

          std::cout << "in download file" << std::endl;

          // Construct the row key by appending "user_" to the foldername
          string rowKey = username + "_" + file_path;

          std::cout << "parent row key: " << rowKey << std::endl;
          std::cout << "file name: " << file_name << std::endl;

          // GET the contents of the file
          F_2_B_Message msg_to_send;
          string response_value, response_error_msg;
          int response_code, response_status;
          type = "get";
          colkey = "no_chunks";

          msg_to_send = construct_msg(1, rowKey, colkey, "", "", "", 0);
          response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                              response_error_msg, rowKey, colkey, g_map_rowkey_to_server,
                                              g_coordinator_addr, type);
          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }

          int no_chunks = stoi(response_value);
          cout << "Number of chunks: " << no_chunks << endl;
          if (no_chunks > 0)
          {
            string file_content = "";
            for (int i = 1; i < no_chunks + 1; i++)
            {
              F_2_B_Message msg_to_send;
              string response_value, response_error_msg;
              int response_code, response_status;
              type = "get";
              colkey = "content_" + to_string(i);

              msg_to_send = construct_msg(1, rowKey, colkey, "", "", "", 0);
              response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                                  response_error_msg, rowKey, colkey, g_map_rowkey_to_server,
                                                  g_coordinator_addr, type);
              if (response_code != 0)
              {

                // Error in fetching file - Construct and send the HTTP response
                string content = "{\"error\":\"Error fetching file\"}";
                send_response(client_fd, 500, "Internal Server Error", "application/json", content);
                continue;
              }

              cout << "RESPONSE: " << response_value << endl;
              cout << "content_" << i << ": " << response_value.size() << endl;
              string decoded_value = base64_decode(response_value);
              cout << "DECODED LENGTH: " << decoded_value.size() << endl;
              file_content += decoded_value;
            }
            std::string content_disposition = "attachment; filename=\"" + file_name + "\"";
            std::string content_type = "application/octet-stream";

            // Send the file contents with appropriate headers
            send_response_with_headers(client_fd, 200, "OK", content_type, file_content, content_disposition);

            // Send JavaScript to redirect back to the drive page after a delay
            std::stringstream redirect_script;
            redirect_script << "<script>";
            redirect_script << "setTimeout(function() { window.location.href = '/drive'; }, 1000);"; // Redirect after 1 second
            redirect_script << "</script>";

            // Send the response with the redirect script
            send_response(client_fd, 200, "OK", "text/html", redirect_script.str());

            cout << "TRANSER SUCCESS!!!" << endl;
          }
          else
          {
            // Error in fetching file - Construct and send the HTTP response
            string content = "{\"error\":\"Error fetching file\"}";
            send_response(client_fd, 500, "Internal Server Error", "application/json", content);
          }
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }
    // Handle POST request to delete a folder
    else if (html_request_map["uri"] == "/delete_folder" && html_request_map["method"] == "POST")
    {
      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          // Extract folder path from the request body
          map<string, string> post_data = parse_json_string_to_map(html_request_map["body"]);
          string folderPath = post_data["folderPath"];

          std::cout << "folder path for delete: " << folderPath << std::endl;

          // Recursively delete folder contents
          deleteFolderContents(folderPath, client_fd, fd, username);

          // Delete folder item
          string row_key = username + "_" + folderPath;
          F_2_B_Message msg_to_send;
          string response_value, response_error_msg;
          int response_code, response_status;
          type = "delete";
          colkey = "items";
          msg_to_send = construct_msg(3, row_key, colkey, "", "", "", 0);
          response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                              response_error_msg, row_key, colkey, g_map_rowkey_to_server,
                                              g_coordinator_addr, type);
          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }

          std::cout << "Sending delete for: " << row_key << std::endl;

          if (response_status != 0)
          {
            // Error handling: send appropriate error response to the client
            string error_message = "Failed to delete folder";
            send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
          }

          // Get items for the parent folder
          size_t lastSlashPos = folderPath.find_last_of('/');
          if (lastSlashPos != string::npos)
          {
            string folderName = folderPath.substr(lastSlashPos + 1);      // Extract the folder name
            string parentFolderPath = folderPath.substr(0, lastSlashPos); // Get the parent folder path
            string parentRowKey = username + "_" + parentFolderPath;

            std::cout << "parent row key for delete: " << parentRowKey << std::endl;

            // Loop till success: GET the parent folder
            bool success = false;

            while (!success)
            {
              // GET request for the parent folder
              type = "get";
              colkey = "items";
              msg_to_send = construct_msg(1, parentRowKey, colkey, "", "", "", 0);
              response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                                  response_error_msg, parentRowKey, colkey, g_map_rowkey_to_server,
                                                  g_coordinator_addr, type);
              if (response_code == 1)
              {
                cerr << "ERROR in communicating with coordinator" << endl;
                continue;
              }
              else if (response_code == 2)
              {
                cerr << "ERROR in communicating with backend" << endl;
                continue;
              }

              if (response_status == 0)
              {
                std::string newValue;

                if (response_value == "[]")
                {
                  // No items in parent folder
                  newValue = "[]";
                }
                else
                {
                  // Remove the deleted folder from parent folder's items
                  vector<string> parentItems = parse_items_list(response_value);

                  // Define the item to be removed (folderPath preceded by "D@" for directories)
                  string itemToRemove = "D@" + folderName;

                  // Use std::remove to move the items to be removed to the end of the vector
                  parentItems.erase(std::remove(parentItems.begin(), parentItems.end(), itemToRemove), parentItems.end());

                  // Concatenate remaining items to form the new value
                  newValue = "[";
                  bool firstItem = true;
                  for (const auto &item : parentItems)
                  {
                    if (!firstItem)
                    {
                      newValue += ",";
                    }
                    newValue += item;
                    firstItem = false;
                  }
                  newValue += "]";

                  std::cout << "New value after deletion in parent: " << newValue << std::endl;
                }

                // CPUT the parent folder
                string cput_response_value, cput_response_error_msg;
                int cput_response_status;
                type = "cput";
                colkey = "items";
                msg_to_send = construct_msg(4, parentRowKey, colkey, response_value, newValue, "", 0);
                response_code = send_msg_to_backend(fd, msg_to_send, cput_response_value, cput_response_status,
                                                    cput_response_error_msg, parentRowKey, colkey, g_map_rowkey_to_server,
                                                    g_coordinator_addr, type);
                if (response_code == 1)
                {
                  cerr << "ERROR in communicating with coordinator" << endl;
                  continue;
                }
                else if (response_code == 2)
                {
                  cerr << "ERROR in communicating with backend" << endl;
                  continue;
                }

                if (cput_response_status == 0)
                {
                  success = true;
                  // Success response
                  string refresh_script = "<script>window.location.reload(true);</script>";
                  send_response(client_fd, 200, "OK", "text/html", refresh_script);
                }
                else if (cput_response_status == 1 && strip(cput_response_error_msg) == "Rowkey does not exist")
                {
                  // Parent folder does not exist - Construct and send the HTTP response
                  string content = "{\"error\":\"Parent folder does not exist\"}";
                  send_response(client_fd, 404, "Not Found", "application/json", content);
                  break; // Exit loop if parent folder doesn't exist
                }
                else if (cput_response_status == 1 && strip(cput_response_error_msg) == "Current value is not v1")
                {
                  // Old value is wrong - Construct and send the HTTP response
                  string content = "{\"error\":\"Current items in parent folder are wrong!\"}";
                  send_response(client_fd, 401, "Unauthorized", "application/json", content);
                  break; // Exit loop if current value is wrong
                }
                else
                {
                  // Error in fetching user - Construct and send the HTTP response
                  string content = "{\"error\":\"Error in updating parent folder\"}";
                  send_response(client_fd, 500, "Internal Server Error", "application/json", content);
                  break; // Exit loop if there's an error in updating parent folder
                }
              }
              else
              {
                // Error in fetching user - Construct and send the HTTP response
                string content = "{\"error\":\"Error fetching parent folder\"}";
                send_response(client_fd, 500, "Internal Server Error", "application/json", content);
              }
            }
          }
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }

    // Handle POST request to rename a folder
    else if (html_request_map["uri"] == "/rename_folder" && html_request_map["method"] == "POST")
    {

      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          // Extract folder path and new folder name from the request body
          map<string, string> post_data = parse_json_string_to_map(html_request_map["body"]);
          string oldFolderPath = post_data["folderPath"];
          string newFolderName = post_data["newFolderName"];

          std::cout << "folder path for rename: " << oldFolderPath << std::endl;

          // Construct the new folder path with the new folder name
          string newFolderPath = oldFolderPath;
          size_t lastSlashPos = newFolderPath.find_last_of('/');
          if (lastSlashPos != string::npos)
          {
            // Replace the folder name with the new folder name
            newFolderPath.replace(lastSlashPos + 1, newFolderPath.length() - lastSlashPos - 1, newFolderName);
          }

          string row_key = username + "_" + oldFolderPath;
          string new_row_key = username + "_" + newFolderPath;

          F_2_B_Message msg_to_send;
          string response_value, response_error_msg;
          int response_status, response_code;
          string rowkey = new_row_key;
          string type = "get";
          string colkey = "items";
          msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
          response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                              response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                              g_coordinator_addr, type);
          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }

          std::cout << "RESPONSE: " << response_error_msg << std::endl;

          if (response_status == 0)
          {
            // file already exists
            string content = "{\"error\":\"Folder with same name already exists!!\"}";
            send_response(client_fd, 409, "Conflict", "application/json", content);
          }
          else if (response_status == 1)
          {

            // Recursively rename folder
            renameFolder(oldFolderPath, newFolderPath, client_fd, fd, username);

            // GET old folder and PUT folder path with new folder name
            msg_to_send = construct_msg(1, row_key, colkey, "", "", "", 0);
            response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                                response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);
            if (response_code == 1)
            {
              cerr << "ERROR in communicating with coordinator" << endl;
              continue;
            }
            else if (response_code == 2)
            {
              cerr << "ERROR in communicating with backend" << endl;
              continue;
            }
            F_2_B_Message msg_to_send_put;
            string response_value_put, response_error_msg_put;
            int response_status_put, response_code;
            string rowkey = new_row_key;
            string type = "put";
            string colkey = "items";
            msg_to_send_put = construct_msg(2, rowkey, colkey, response_value, "", "", 0);
            response_code = send_msg_to_backend(fd, msg_to_send_put, response_value_put, response_status_put,
                                                response_error_msg_put, rowkey, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);
            if (response_code == 1)
            {
              cerr << "ERROR in communicating with coordinator" << endl;
              continue;
            }
            else if (response_code == 2)
            {
              cerr << "ERROR in communicating with backend" << endl;
              continue;
            }
            if (response_status_put != 0)
            {
              // Error handling: send appropriate error response to the client
              string error_message = "Failed to create new path";
              send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
            }

            // Delete folder item
            row_key = username + "_" + oldFolderPath;

            std::cout << "Sending delete for: " << row_key << std::endl;

            F_2_B_Message msg_to_send_delete;
            string response_value_delete, response_error_msg_delete;
            int response_status_delete;
            rowkey = new_row_key;
            type = "delete";
            colkey = "items";
            msg_to_send_delete = construct_msg(3, row_key, colkey, "", "", "", 0);
            response_code = send_msg_to_backend(fd, msg_to_send_delete, response_value_delete, response_status_delete,
                                                response_error_msg_delete, rowkey, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);
            if (response_code == 1)
            {
              cerr << "ERROR in communicating with coordinator" << endl;
              continue;
            }
            else if (response_code == 2)
            {
              cerr << "ERROR in communicating with backend" << endl;
              continue;
            }
            if (response_status_delete != 0)
            {
              // Error handling: send appropriate error response to the client
              string error_message = "Failed to delete folder";
              send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
            }

            // Get items for the parent folder
            lastSlashPos = oldFolderPath.find_last_of('/');
            if (lastSlashPos != string::npos)
            {
              string folderName = oldFolderPath.substr(lastSlashPos + 1);      // Extract the folder name
              string parentFolderPath = oldFolderPath.substr(0, lastSlashPos); // Get the parent folder path
              string parentRowKey = username + "_" + parentFolderPath;

              std::cout << "parent row key for delete: " << parentRowKey << std::endl;

              // Loop till success: GET the parent folder
              bool success = false;

              while (!success)
              {
                F_2_B_Message msg_to_send_get;
                string response_value_get, response_error_msg_get;
                int response_status_get, response_code;
                string rowkey = parentRowKey;
                string type = "get";
                string colkey = "items";
                msg_to_send_get = construct_msg(1, rowkey, colkey, "", "", "", 0);
                response_code = send_msg_to_backend(fd, msg_to_send_get, response_value_get, response_status_get,
                                                    response_error_msg_get, rowkey, colkey, g_map_rowkey_to_server,
                                                    g_coordinator_addr, type);
                // GET request for the parent folder
                if (response_code == 1)
                {
                  cerr << "ERROR in communicating with coordinator" << endl;
                  continue;
                }
                else if (response_code == 2)
                {
                  cerr << "ERROR in communicating with backend" << endl;
                  continue;
                }
                if (response_status_get == 0)
                {
                  std::string newValue;

                  if (response_value_get == "[]")
                  {
                    // No items in parent folder
                    newValue = "[]";
                  }
                  else
                  {
                    // Remove the deleted folder from parent folder's items
                    vector<string> parentItems = parse_items_list(response_value_get);

                    // Define the item to be removed (folderPath preceded by "D@" for directories)
                    string itemToRemove = "D@" + folderName;

                    // Use std::remove to move the items to be removed to the end of the vector
                    parentItems.erase(std::remove(parentItems.begin(), parentItems.end(), itemToRemove), parentItems.end());

                    // Add the new folder name to the list
                    string newItem = "D@" + newFolderName;
                    parentItems.push_back(newItem);

                    // Concatenate remaining items to form the new value
                    newValue = "[";
                    bool firstItem = true;
                    for (const auto &item : parentItems)
                    {
                      if (!firstItem)
                      {
                        newValue += ",";
                      }
                      newValue += item;
                      firstItem = false;
                    }
                    newValue += "]";

                    std::cout << "New value after deletion in parent: " << newValue << std::endl;
                  }
                  F_2_B_Message msg_to_send_cput;
                  string response_value_cput, response_error_msg_cput;
                  int response_status_cput, response_code;
                  string rowkey = parentRowKey;
                  string type = "cput";
                  string colkey = "items";
                  msg_to_send_cput = construct_msg(4, rowkey, colkey, response_value_get, newValue, "", 0);
                  response_code = send_msg_to_backend(fd, msg_to_send_cput, response_value_cput, response_status_cput,
                                                      response_error_msg_cput, rowkey, colkey, g_map_rowkey_to_server,
                                                      g_coordinator_addr, type);
                  // CPUT the parent folder
                  if (response_code == 1)
                  {
                    cerr << "ERROR in communicating with coordinator" << endl;
                    continue;
                  }
                  else if (response_code == 2)
                  {
                    cerr << "ERROR in communicating with backend" << endl;
                    continue;
                  }

                  if (response_status_cput == 0)
                  {
                    success = true;
                    // Success response
                    string refresh_script = "<script>window.location.reload(true);</script>";
                    send_response(client_fd, 200, "OK", "text/html", refresh_script);
                  }
                  else if (response_status_cput == 1 && strip(response_error_msg_cput) == "Rowkey does not exist")
                  {
                    // Parent folder does not exist - Construct and send the HTTP response
                    string content = "{\"error\":\"Parent folder does not exist\"}";
                    send_response(client_fd, 404, "Not Found", "application/json", content);
                    break; // Exit loop if parent folder doesn't exist
                  }
                  else if (response_status_cput == 1 && strip(response_error_msg_cput) == "Current value is not v1")
                  {
                    // Old value is wrong - Construct and send the HTTP response
                    string content = "{\"error\":\"Current items in parent folder are wrong!\"}";
                    send_response(client_fd, 401, "Unauthorized", "application/json", content);
                    break; // Exit loop if current value is wrong
                  }
                  else
                  {
                    // Error in fetching user - Construct and send the HTTP response
                    string content = "{\"error\":\"Error in updating parent folder\"}";
                    send_response(client_fd, 500, "Internal Server Error", "application/json", content);
                    break; // Exit loop if there's an error in updating parent folder
                  }
                }
                else
                {
                  // Error in fetching user - Construct and send the HTTP response
                  string content = "{\"error\":\"Error fetching parent folder\"}";
                  send_response(client_fd, 500, "Internal Server Error", "application/json", content);
                }
              }
            }
          }
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }
    // Handle POST request to move a folder
    else if (html_request_map["uri"] == "/move_folder" && html_request_map["method"] == "POST")
    {
      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          // Extract folder path and new folder name from the request body
          map<string, string> post_data = parse_json_string_to_map(html_request_map["body"]);
          string oldFolderPath = post_data["folderPath"];
          string newPath = post_data["newPath"];

          std::cout << "folder path for rename: " << oldFolderPath << std::endl;

          // Construct the new folder path
          string folderName;
          size_t lastSlashPos = oldFolderPath.find_last_of('/');
          if (lastSlashPos != string::npos)
          {
            // Extract the folder name
            folderName = oldFolderPath.substr(lastSlashPos + 1);
          }

          // Append the folder name to the new folder path
          string newFolderPath = newPath + "/" + folderName;

          string row_key = username + "_" + oldFolderPath;
          string new_path_key = username + "_" + newFolderPath;
          string new_row_key = username + "_" + newPath;

          F_2_B_Message msg_to_send_get;
          string response_value_get, response_error_msg_get;
          int response_status_get, response_code;
          string rowkey = new_row_key;
          string type = "get";
          string colkey = "items";

          // Check whether the new directory exists
          msg_to_send_get = construct_msg(1, rowkey, colkey, "", "", "", 0);
          response_code = send_msg_to_backend(fd, msg_to_send_get, response_value_get, response_status_get,
                                              response_error_msg_get, rowkey, colkey, g_map_rowkey_to_server,
                                              g_coordinator_addr, type);

          if (response_code == 1)
          {
            cerr << "ERROR in communicating with coordinator" << endl;
            continue;
          }
          else if (response_code == 2)
          {
            cerr << "ERROR in communicating with backend" << endl;
            continue;
          }

          if (response_status_get == 1)
          {
            // directory does not exist
            string content = "{\"error\":\"New directory does not exist!!\"}";
            send_response(client_fd, 409, "Conflict", "application/json", content);
          }
          else if (response_status_get == 0)
          {
            // Check whether folder already exists in this directory
            string rowkey = new_path_key;
            msg_to_send_get = construct_msg(1, rowkey, colkey, "", "", "", 0);
            response_code = send_msg_to_backend(fd, msg_to_send_get, response_value_get, response_status_get,
                                                response_error_msg_get, rowkey, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);
            if (response_code == 1)
            {
              cerr << "ERROR in communicating with coordinator" << endl;
              continue;
            }
            else if (response_code == 2)
            {
              cerr << "ERROR in communicating with backend" << endl;
              continue;
            }

            std::cout << "RESPONSE: " << response_error_msg_get << std::endl;

            if (response_status_get == 0)
            {
              // file already exists
              string content = "{\"error\":\"Folder with same name already exists in new directory!!\"}";
              send_response(client_fd, 409, "Conflict", "application/json", content);
            }
            else if (response_status_get == 1)
            {

              // Recursively move folder
              renameFolder(oldFolderPath, newFolderPath, client_fd, fd, username);

              F_2_B_Message msg_to_send_get;
              string response_value_get, response_error_msg_get;
              int response_status_get, response_code;
              string rowkey = row_key;
              string type = "get";
              string colkey = "items";
              msg_to_send_get = construct_msg(1, rowkey, colkey, "", "", "", 0);
              response_code = send_msg_to_backend(fd, msg_to_send_get, response_value_get, response_status_get,
                                                  response_error_msg_get, rowkey, colkey, g_map_rowkey_to_server,
                                                  g_coordinator_addr, type);
              if (response_code == 1)
              {
                cerr << "ERROR in communicating with coordinator" << endl;
                continue;
              }
              else if (response_code == 2)
              {
                cerr << "ERROR in communicating with backend" << endl;
                continue;
              }

              // GET old folder and PUT new folder path
              F_2_B_Message msg_to_send_put;
              string response_value_put, response_error_msg_put;
              int response_status_put;
              rowkey = new_path_key;
              type = "put";
              colkey = "items";
              msg_to_send_put = construct_msg(2, rowkey, colkey, response_value_get, "", "", 0);
              response_code = send_msg_to_backend(fd, msg_to_send_put, response_value_put, response_status_put,
                                                  response_error_msg_put, rowkey, colkey, g_map_rowkey_to_server,
                                                  g_coordinator_addr, type);
              if (response_code == 1)
              {
                cerr << "ERROR in communicating with coordinator" << endl;
                continue;
              }
              else if (response_code == 2)
              {
                cerr << "ERROR in communicating with backend" << endl;
                continue;
              }
              if (response_status_put != 0)
              {
                // Error handling: send appropriate error response to the client
                string error_message = "Failed to create new path";
                send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
              }

              // Delete folder item
              std::cout << "Sending delete for: " << row_key << std::endl;

              F_2_B_Message msg_to_send_delete;
              string response_value_delete, response_error_msg_delete;
              int response_status_delete;
              rowkey = row_key;
              type = "delete";
              colkey = "items";
              msg_to_send_delete = construct_msg(3, rowkey, colkey, "", "", "", 0);
              response_code = send_msg_to_backend(fd, msg_to_send_delete, response_value_delete, response_status_delete,
                                                  response_error_msg_delete, rowkey, colkey, g_map_rowkey_to_server,
                                                  g_coordinator_addr, type);
              if (response_code == 1)
              {
                cerr << "ERROR in communicating with coordinator" << endl;
                continue;
              }
              else if (response_code == 2)
              {
                cerr << "ERROR in communicating with backend" << endl;
                continue;
              }

              if (response_status_delete != 0)
              {
                // Error handling: send appropriate error response to the client
                string error_message = "Failed to delete folder";
                send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
              }

              // Get items for the parent folder and new path
              lastSlashPos = oldFolderPath.find_last_of('/');
              if (lastSlashPos != string::npos)
              {
                string folderName = oldFolderPath.substr(lastSlashPos + 1);      // Extract the folder name
                string parentFolderPath = oldFolderPath.substr(0, lastSlashPos); // Get the parent folder path
                string parentRowKey = username + "_" + parentFolderPath;

                std::cout << "parent row key for delete: " << parentRowKey << std::endl;

                // Loop till success: GET the parent folder
                bool success = false;

                while (!success)
                {

                  F_2_B_Message msg_to_send_get;
                  string response_value_get, response_error_msg_get;
                  int response_status_get, response_code;
                  string rowkey = parentRowKey;
                  string type = "get";
                  string colkey = "items";
                  msg_to_send_get = construct_msg(1, rowkey, colkey, "", "", "", 0);
                  response_code = send_msg_to_backend(fd, msg_to_send_get, response_value_get, response_status_get,
                                                      response_error_msg_get, rowkey, colkey, g_map_rowkey_to_server,
                                                      g_coordinator_addr, type);
                  // GET request for the parent folder & new folder
                  if (response_code == 1)
                  {
                    cerr << "ERROR in communicating with coordinator" << endl;
                    continue;
                  }
                  else if (response_code == 2)
                  {
                    cerr << "ERROR in communicating with backend" << endl;
                    continue;
                  }
                  F_2_B_Message msg_to_send_get_new;
                  string response_value_get_new, response_error_msg_get_new;
                  int response_status_get_new;
                  rowkey = new_row_key;
                  type = "get";
                  colkey = "items";
                  msg_to_send_get_new = construct_msg(1, rowkey, colkey, "", "", "", 0);
                  response_code = send_msg_to_backend(fd, msg_to_send_get_new, response_value_get_new, response_status_get_new,
                                                      response_error_msg_get_new, rowkey, colkey, g_map_rowkey_to_server,
                                                      g_coordinator_addr, type);
                  if (response_code == 1)
                  {
                    cerr << "ERROR in communicating with coordinator" << endl;
                    continue;
                  }
                  else if (response_code == 2)
                  {
                    cerr << "ERROR in communicating with backend" << endl;
                    continue;
                  }
                  std::cout << "ROW KEY FOR HW2: " << new_row_key << std::endl;

                  // Parent folder handling
                  if (response_status_get == 0)
                  {
                    std::string newValue;
                    std::string newValueFolder;

                    if (response_value_get == "[]")
                    {
                      // No items in parent folder
                      newValue = "[]";
                    }
                    else
                    {
                      // Remove the deleted folder from parent folder's items
                      vector<string> parentItems = parse_items_list(response_value_get);

                      // Define the item to be removed (folderPath preceded by "D@" for directories)
                      string itemToRemove = "D@" + folderName;

                      // Use std::remove to move the items to be removed to the end of the vector
                      parentItems.erase(std::remove(parentItems.begin(), parentItems.end(), itemToRemove), parentItems.end());

                      // Concatenate remaining items to form the new value
                      newValue = "[";
                      bool firstItem = true;
                      for (const auto &item : parentItems)
                      {
                        if (!firstItem)
                        {
                          newValue += ",";
                        }
                        newValue += item;
                        firstItem = false;
                      }
                      newValue += "]";

                      std::cout << "New value after deletion in parent: " << newValue << std::endl;
                    }

                    // New folder handling
                    vector<string> folderItems = parse_items_list(response_value_get_new);

                    // Add the folder name to the list
                    string newItem = "D@" + folderName;
                    folderItems.push_back(newItem);

                    std::cout << "NEW ITEM TO PUT FOR HW2: " << newItem << std::endl;

                    // Concatenate remaining items to form the new value
                    newValueFolder = "[";
                    bool firstItem = true;
                    for (const auto &item : folderItems)
                    {
                      if (!firstItem)
                      {
                        newValueFolder += ",";
                      }
                      newValueFolder += item;
                      firstItem = false;
                    }
                    newValueFolder += "]";

                    F_2_B_Message msg_to_send_cput;
                    string response_value_cput, response_error_msg_cput;
                    int response_status_cput, response_code;
                    string rowkey = parentRowKey;
                    string type = "cput";
                    string colkey = "items";
                    msg_to_send_cput = construct_msg(4, rowkey, colkey, response_value_get, newValue, "", 0);
                    response_code = send_msg_to_backend(fd, msg_to_send_cput, response_value_cput, response_status_cput,
                                                        response_error_msg_cput, rowkey, colkey, g_map_rowkey_to_server,
                                                        g_coordinator_addr, type);
                    if (response_code == 1)
                    {
                      cerr << "ERROR in communicating with coordinator" << endl;
                      continue;
                    }
                    else if (response_code == 2)
                    {
                      cerr << "ERROR in communicating with backend" << endl;
                      continue;
                    }
                    // CPUT the parent folder

                    std::cout << "NEW ITEM ROW KEY FOR HW2: " << new_row_key << std::endl;
                    std::cout << "NEW VALUE TO PUT FOR HW2: " << newValueFolder << std::endl;

                    F_2_B_Message msg_to_send_cput_new;
                    string response_value_cput_new, response_error_msg_cput_new;
                    int response_status_cput_new;
                    rowkey = new_row_key;
                    type = "cput";
                    colkey = "items";
                    msg_to_send_cput_new = construct_msg(4, rowkey, colkey, response_value_get_new, newValueFolder, "", 0);
                    response_code = send_msg_to_backend(fd, msg_to_send_cput_new, response_value_cput_new, response_status_cput_new,
                                                        response_error_msg_cput_new, rowkey, colkey, g_map_rowkey_to_server,
                                                        g_coordinator_addr, type);
                    if (response_code == 1)
                    {
                      cerr << "ERROR in communicating with coordinator" << endl;
                      continue;
                    }
                    else if (response_code == 2)
                    {
                      cerr << "ERROR in communicating with backend" << endl;
                      continue;
                    }

                    // CPUT the new folder path

                    if (response_status_cput == 0 && response_status_cput_new == 0)
                    {
                      success = true;
                      // Success response
                      string refresh_script = "<script>window.location.reload(true);</script>";
                      send_response(client_fd, 200, "OK", "text/html", refresh_script);
                    }
                    else
                    {
                      // Error in fetching user - Construct and send the HTTP response
                      string content = "{\"error\":\"Error in updating parent folder\"}";
                      send_response(client_fd, 500, "Internal Server Error", "application/json", content);
                      break; // Exit loop if there's an error in updating parent folder
                    }
                  }
                  else
                  {
                    // Error in fetching user - Construct and send the HTTP response
                    string content = "{\"error\":\"Error fetching parent folder\"}";
                    send_response(client_fd, 500, "Internal Server Error", "application/json", content);
                  }
                }
              }
            }
          }
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }
    // Handle POST request to delete a file
    else if (html_request_map["uri"] == "/delete_file" && html_request_map["method"] == "POST")
    {
      // string backend_serveraddr_str = "127.0.0.1:6000";

      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          // Extract file path from the request body
          map<string, string> post_data = parse_json_string_to_map(html_request_map["body"]);
          string filePath = post_data["filePath"];

          std::cout << "file path for deletion: " << filePath << std::endl;

          // Construct the row key for the file
          string row_key = username + "_" + filePath;

          std::cout << "row key for file to delete: " << row_key << std::endl;

          // Get current directory path
          size_t lastSlashPos = filePath.find_last_of('/');
          if (lastSlashPos != std::string::npos)
          {
            string fileName = filePath.substr(lastSlashPos + 1);             // Extract the file name
            std::string parentFolderPath = filePath.substr(0, lastSlashPos); // Get the parent folder path
            std::string parentRowKey = username + "_" + parentFolderPath;

            std::cout << "parent directory to update: " << parentRowKey << std::endl;

            // Loop till success: GET the parent folder
            bool success = false;

            while (!success)
            {
              // Get current directory items
              F_2_B_Message msg_to_send;
              string get_response_value, get_response_error_msg;
              int get_response_status, response_code;
              type = "get";
              colkey = "items";
              msg_to_send = construct_msg(1, parentRowKey, colkey, "", "", "", 0);
              response_code = send_msg_to_backend(fd, msg_to_send, get_response_value, get_response_status,
                                                  get_response_error_msg, parentRowKey, colkey, g_map_rowkey_to_server,
                                                  g_coordinator_addr, type);
              if (response_code == 1)
              {
                cerr << "ERROR in communicating with coordinator" << endl;
                continue;
              }
              else if (response_code == 2)
              {
                cerr << "ERROR in communicating with backend" << endl;
                continue;
              }

              if (get_response_status == 0)
              {

                // Parse items list
                vector<string> items = parse_items_list(get_response_value);

                // Remove the file from the items list
                items.erase(remove(items.begin(), items.end(), "F@" + fileName), items.end());

                // Concatenate remaining items to form the new value
                string newValue = "[";
                bool firstItem = true;
                for (const auto &item : items)
                {
                  if (!firstItem)
                  {
                    newValue += ",";
                  }
                  newValue += item;
                  firstItem = false;
                }
                newValue += "]";

                std::cout << "new value to update: " << newValue << std::endl;

                // CPUT the current directory items without the file
                string cput_response_value, cput_response_error_msg;
                int cput_response_status, response_code;
                type = "cput";
                colkey = "items";
                msg_to_send = construct_msg(4, parentRowKey, colkey, get_response_value, newValue, "", 0);
                response_code = send_msg_to_backend(fd, msg_to_send, cput_response_value, cput_response_status,
                                                    cput_response_error_msg, parentRowKey, colkey, g_map_rowkey_to_server,
                                                    g_coordinator_addr, type);
                if (response_code == 1)
                {
                  cerr << "ERROR in communicating with coordinator" << endl;
                  continue;
                }
                else if (response_code == 2)
                {
                  cerr << "ERROR in communicating with backend" << endl;
                  continue;
                }

                if (cput_response_status == 0)
                {
                  // Delete file content
                  int delete_response_status = delete_file_chunks(fd, row_key, g_map_rowkey_to_server, g_coordinator_addr);

                  if (delete_response_status == 0)
                  {
                    // Set success flag to true to exit the loop
                    success = true;
                    // Success response
                    std::string refresh_script = "<script>window.location.reload(true);</script>";
                    send_response(client_fd, 200, "OK", "text/html", refresh_script);
                  }
                  else
                  {
                    // Error handling: send appropriate error response to the client
                    string error_message = "Failed to delete file content";
                    send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
                  }
                }
                else if (cput_response_status == 1 && strip(cput_response_error_msg) == "Rowkey does not exist")
                {
                  // Parent folder does not exist - Construct and send the HTTP response
                  std::string content = "{\"error\":\"Parent folder does not exist\"}";
                  send_response(client_fd, 404, "Not Found", "application/json", content);
                }
                else if (cput_response_status == 1 && strip(cput_response_error_msg) == "Current value is not v1")
                {
                  // Old value is wrong - Construct and send the HTTP response
                  std::string content = "{\"error\":\"Current items in parent folder are wrong!\"}";
                  send_response(client_fd, 401, "Unauthorized", "application/json", content);
                }
                else
                {
                  // Error in fetching user - Construct and send the HTTP response
                  std::string content = "{\"error\":\"Error in updating parent folder\"}";
                  send_response(client_fd, 500, "Internal Server Error", "application/json", content);
                }
              }
              else
              {
                // Error handling: send appropriate error response to the client
                string error_message = "Failed to fetch directory items";
                send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
              }
            }
          }
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }
    // Handle POST request to rename a file
    else if (html_request_map["uri"] == "/rename_file" && html_request_map["method"] == "POST")
    {
      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          // Extract file path and new file name from the request body
          map<string, string> post_data = parse_json_string_to_map(html_request_map["body"]);
          string filePath = post_data["filePath"];
          string newFileName = post_data["newFileName"];

          std::cout << "File path for rename: " << filePath << std::endl;
          std::cout << "New file name: " << newFileName << std::endl;

          // Get current directory path and file name
          size_t lastSlashPos = filePath.find_last_of('/');
          if (lastSlashPos != std::string::npos)
          {
            string fileName = filePath.substr(lastSlashPos + 1);             // Extract the file name
            std::string parentFolderPath = filePath.substr(0, lastSlashPos); // Get the parent folder path
            std::string parentRowKey = username + "_" + parentFolderPath;
            std::string new_row_key = username + "_" + parentFolderPath + "/" + newFileName;
            std::string old_row_key = username + "_" + filePath;

            // Check whether file already exists in this directory
            string get_response_value, get_response_error_msg;
            int get_response_status, response_code;
            string type = "get";
            string rowkey = new_row_key;
            string colkey = "no_chunks";
            F_2_B_Message msg_to_send_get = construct_msg(1, rowkey, colkey, "", "", "", 0);
            response_code = send_msg_to_backend(fd, msg_to_send_get, get_response_value, get_response_status,
                                                get_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);

            std::cout << "RESPONSE: " << get_response_error_msg << std::endl;

            if (get_response_status == 0)
            {
              // file already exists
              string content = "{\"error\":\"File with same name already exists!!\"}";
              send_response(client_fd, 409, "Conflict", "application/json", content);
            }
            else if (get_response_status == 1)
            {
              // GET and PUT new file contents
              int get_put_response_status = copyChunks(fd, old_row_key, new_row_key, g_map_rowkey_to_server, g_coordinator_addr);

              if (get_put_response_status != 0)
              {
                // Error handling: send appropriate error response to the client
                string error_message = "Failed to create new file path";
                send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
              }

              std::cout << "Parent directory to update: " << parentRowKey << std::endl;

              // Loop until success: GET the parent folder
              bool success = false;

              while (!success)
              {
                // Get current directory items

                string get_response_value, get_response_error_msg;
                int get_response_status, response_code;
                string type = "get";
                string rowkey = parentRowKey;
                string colkey = "items";
                F_2_B_Message msg_to_send_get = construct_msg(1, rowkey, colkey, "", "", "", 0);
                response_code = send_msg_to_backend(fd, msg_to_send_get, get_response_value, get_response_status,
                                                    get_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                    g_coordinator_addr, type);
                if (get_response_status == 0)
                {
                  // Parse items list
                  vector<string> items = parse_items_list(get_response_value);

                  // Replace the old file name with the new file name
                  for (auto &item : items)
                  {
                    if (item == "F@" + fileName)
                    {
                      item = "F@" + newFileName;
                      break;
                    }
                  }

                  // Concatenate remaining items to form the new value
                  string newValue = "[";
                  bool firstItem = true;
                  for (const auto &item : items)
                  {
                    if (!firstItem)
                    {
                      newValue += ",";
                    }
                    newValue += item;
                    firstItem = false;
                  }
                  newValue += "]";

                  std::cout << "New value to update: " << newValue << std::endl;

                  // CPUT the current directory items with the renamed file

                  string cput_response_value, cput_response_error_msg;
                  int cput_response_status, response_code;
                  string type = "cput";
                  string rowkey = parentRowKey;
                  string colkey = "items";
                  F_2_B_Message msg_to_send_cput = construct_msg(4, rowkey, colkey, get_response_value, newValue, "", 0);
                  response_code = send_msg_to_backend(fd, msg_to_send_cput, cput_response_value, cput_response_status,
                                                      cput_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                      g_coordinator_addr, type);

                  if (cput_response_status == 0)
                  {
                    // Delete file content
                    int delete_response_status = delete_file_chunks(fd, old_row_key, g_map_rowkey_to_server, g_coordinator_addr);

                    if (delete_response_status == 0)
                    {
                      // Set success flag to true to exit the loop
                      success = true;
                      // Success response
                      std::string refresh_script = "<script>window.location.reload(true);</script>";
                      send_response(client_fd, 200, "OK", "text/html", refresh_script);
                    }
                    else
                    {
                      // Error handling: send appropriate error response to the client
                      string error_message = "Failed to delete file content";
                      send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
                    }
                  }
                  else
                  {
                    // Error handling: send appropriate error response to the client
                    std::string error_message = "Failed to update parent folder";
                    send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
                    break;
                  }
                }
                else
                {
                  // Error handling: send appropriate error response to the client
                  std::string error_message = "Failed to fetch directory items";
                  send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
                  break;
                }
              }
            }
          }
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }
    // Handle POST request to move a file
    else if (html_request_map["uri"] == "/move_file" && html_request_map["method"] == "POST")
    {
      // get username from cookie
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get username from cookie
        std::string username = get_username_from_cookie(cookie, fd); // TO DO: backend address
        if (!username.empty())
        {
          // Extract file path and new file name from the request body
          map<string, string> post_data = parse_json_string_to_map(html_request_map["body"]);
          string filePath = post_data["filePath"];
          string newFilePath = post_data["newFilePath"];

          // Get current directory path and file name
          size_t lastSlashPos = filePath.find_last_of('/');
          if (lastSlashPos != std::string::npos)
          {
            string fileName = filePath.substr(lastSlashPos + 1);             // Extract the file name
            std::string parentFolderPath = filePath.substr(0, lastSlashPos); // Get the parent folder path
            std::string parentRowKey = username + "_" + parentFolderPath;
            std::string new_row_key = username + "_" + newFilePath + "/" + fileName;
            std::string old_row_key = username + "_" + filePath;
            std::string new_parent_row_key = username + "_" + newFilePath;

            string get_response_value, get_response_error_msg;
            int get_response_status, response_code;
            string type = "get";
            string rowkey = new_parent_row_key;
            string colkey = "items";
            cout << "PATH TO CHECK FOR NEW DIRECTORY" << new_parent_row_key << endl;
            // Check whether the new directory exists
            F_2_B_Message msg_to_send_get = construct_msg(1, rowkey, colkey, "", "", "", 0);
            response_code = send_msg_to_backend(fd, msg_to_send_get, get_response_value, get_response_status,
                                                get_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);

            if (response_code == 1)
            {
              cerr << "ERROR in communicating with coordinator" << endl;
              continue;
            }
            else if (response_code == 2)
            {
              cerr << "ERROR in communicating with backend" << endl;
              continue;
            }

            if (get_response_status == 1)
            {
              // directory does not exist
              string content = "{\"error\":\"New directory does not exist!!\"}";
              send_response(client_fd, 409, "Conflict", "application/json", content);
            }
            else if (get_response_status == 0)
            {
              // Check whether file already exists in this directory
              string rowkey = new_row_key;
              string colkey = "no_chunks";
              msg_to_send_get = construct_msg(1, rowkey, colkey, "", "", "", 0);
              response_code = send_msg_to_backend(fd, msg_to_send_get, get_response_value, get_response_status,
                                                  get_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                  g_coordinator_addr, type);

              std::cout << "RESPONSE: " << get_response_error_msg << std::endl;

              if (get_response_status == 0)
              {
                // file already exists
                string content = "{\"error\":\"File already exists in new directory!!\"}";
                send_response(client_fd, 409, "Conflict", "application/json", content);
              }
              else if (get_response_status == 1)
              {
                // GET and PUT new file contents
                int get_put_response_status = copyChunks(fd, old_row_key, new_row_key, g_map_rowkey_to_server, g_coordinator_addr);

                if (get_put_response_status != 0)
                {
                  // Error handling: send appropriate error response to the client
                  string error_message = "Failed to create new file path";
                  send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
                }

                std::cout << "Parent directory to update: " << parentRowKey << std::endl;

                // Loop until success: GET the parent folder
                bool success = false;

                while (!success)
                {
                  // Get new directory items
                  string get_response_value, get_response_error_msg;
                  int get_response_status, response_code;
                  string type = "get";
                  string rowkey = new_parent_row_key;
                  string colkey = "items";
                  F_2_B_Message msg_to_send_get = construct_msg(1, rowkey, colkey, "", "", "", 0);
                  response_code = send_msg_to_backend(fd, msg_to_send_get, get_response_value, get_response_status,
                                                      get_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                      g_coordinator_addr, type);

                  string old_get_response_value, old_get_response_error_msg;
                  int old_get_response_status;
                  type = "get";
                  rowkey = parentRowKey;
                  colkey = "items";
                  F_2_B_Message msg_to_send_old_get = construct_msg(1, rowkey, colkey, "", "", "", 0);
                  response_code = send_msg_to_backend(fd, msg_to_send_old_get, old_get_response_value, old_get_response_status,
                                                      old_get_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                      g_coordinator_addr, type);

                  if (get_response_status == 0 && old_get_response_status == 0)
                  {
                    // For new directory: Parse items list
                    vector<string> items = parse_items_list(get_response_value);

                    // Append the new file name to the existing items list
                    items.push_back("F@" + fileName);

                    // Concatenate remaining items to form the new value
                    string newValue = "[";
                    bool firstItem = true;
                    for (const auto &item : items)
                    {
                      if (!firstItem)
                      {
                        newValue += ",";
                      }
                      newValue += item;
                      firstItem = false;
                    }
                    newValue += "]";

                    std::cout << "New value to update: " << newValue << std::endl;

                    // For old directory: Parse items list
                    vector<string> old_items = parse_items_list(old_get_response_value);

                    // Remove the old file from the directory items
                    old_items.erase(remove(old_items.begin(), old_items.end(), "F@" + fileName), old_items.end());

                    // Concatenate remaining items to form the new value
                    string newValue_old = "[";
                    bool firstItem_old = true;
                    for (const auto &item : old_items)
                    {
                      if (!firstItem_old)
                      {
                        newValue_old += ",";
                      }
                      newValue_old += item;
                      firstItem_old = false;
                    }
                    newValue_old += "]";

                    // CPUT the new directory items with the new file
                    string cput_response_value, cput_response_error_msg;
                    int cput_response_status, response_code;
                    string type = "cput";
                    string rowkey = new_parent_row_key;
                    string colkey = "items";
                    F_2_B_Message msg_to_send_cput = construct_msg(4, rowkey, colkey, get_response_value, newValue, "", 0);
                    response_code = send_msg_to_backend(fd, msg_to_send_cput, cput_response_value, cput_response_status,
                                                        cput_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                        g_coordinator_addr, type);

                    string old_cput_response_value, old_cput_response_error_msg;
                    int old_cput_response_status;
                    type = "cput";
                    rowkey = parentRowKey;
                    colkey = "items";
                    F_2_B_Message msg_to_send_cput_old = construct_msg(4, rowkey, colkey, old_get_response_value, newValue_old, "", 0);
                    response_code = send_msg_to_backend(fd, msg_to_send_cput_old, old_cput_response_value, old_cput_response_status,
                                                        old_cput_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                        g_coordinator_addr, type);

                    if (cput_response_status == 0 && old_cput_response_status == 0)
                    {
                      // Delete file content
                      string rowkey = old_row_key;

                      int delete_response_status = delete_file_chunks(fd, old_row_key, g_map_rowkey_to_server, g_coordinator_addr);

                      if (delete_response_status == 0)
                      {
                        // Set success flag to true to exit the loop
                        success = true;
                        // Success response
                        std::string refresh_script = "<script>window.location.reload(true);</script>";
                        send_response(client_fd, 200, "OK", "text/html", refresh_script);
                      }
                      else
                      {
                        // Error handling: send appropriate error response to the client
                        string error_message = "Failed to delete file content";
                        send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
                      }
                    }
                    else
                    {
                      // Error handling: send appropriate error response to the client
                      std::string error_message = "Failed to update parent folder";
                      send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
                      break;
                    }
                  }
                  else
                  {
                    // Error handling: send appropriate error response to the client
                    std::string error_message = "Failed to fetch directory items";
                    send_response(client_fd, 500, "Internal Server Error", "application/json", error_message);
                    break;
                  }
                }
              }
            }
          }
        }
        else
        {
          // Handle unauthenticated or failed lookup
          redirect(client_fd, "/login");
        }
      }
    }

    // GET: /admin
    else if (html_request_map["uri"] == "/admin" && html_request_map["method"] == "GET")
    {
      // LIST: get status of all backend servers
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        size_t colon_pos = g_coordinator_addr_str.find(':');
        if (colon_pos == std::string::npos)
        {
          send_response(client_fd, 500, "Internal Server Error", "text/html", "Server configuration error.");
          continue;
        }

        std::string coordinator_ip = g_coordinator_addr_str.substr(0, colon_pos);
        int coordinator_port = std::stoi(g_coordinator_addr_str.substr(colon_pos + 1));
        std::vector<server_info> backend_servers = get_list_of_backend_servers(coordinator_ip, coordinator_port);
        std::vector<server_info> frontend_servers = get_list_of_frontend_servers(LOAD_BALANCER_IP, LOAD_BALANCER_PORT);
        std::string response_content = get_admin_html_from_vector(frontend_servers, backend_servers);
        send_response_with_headers(client_fd, 200, "OK", "text/html", response_content, "");
      }
    }

    // GET: /admin/<addr>
    else if (html_request_map["uri"].substr(0, 7) == "/admin/" && html_request_map["method"] == "GET")
    {
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        std::string server_addr = html_request_map["uri"].substr(7);
        size_t colon_pos = server_addr.find(':');
        if (colon_pos == std::string::npos || colon_pos == server_addr.length() - 1)
        {
          send_response(client_fd, 400, "Bad Request", "text/html", "Invalid server address format.");
          continue;
        }
        std::string ip = server_addr.substr(0, colon_pos);
        std::string port_str = server_addr.substr(colon_pos + 1);
        int port;

        if (!safe_stoi(port_str, port))
        {
          send_response(client_fd, 400, "Bad Request", "text/html", "Invalid port number.");
          continue;
        }

        auto data = fetch_data_from_server(ip, port);
        if (data.empty())
        {
          send_response(client_fd, 500, "Internal Server Error", "text/html", "Failed to fetch data from the server.");
          continue;
        }

        std::string html = generate_html_from_data(data, ip, port);
        send_response(client_fd, 200, "OK", "text/html", html);
      }
    }

    // POST: /admin?toggle=suspend&server=<addr>     or    /admin?toggle=activate&server=<addr>
    else if (html_request_map["uri"].substr(0, 13) == "/admin?toggle" && html_request_map["method"] == "POST")
    {
      std::string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        std::string query = html_request_map["uri"].substr(6);
        std::string result = handle_toggle_request(query);

        if (result.empty())
        {
          std::string redirect_html = "<html><head><meta http-equiv='refresh' content='0;url=/admin'></head></html>";
          send_response(client_fd, 302, "Found", "text/html", redirect_html);
        }
        else
        {
          send_response(client_fd, 400, "Bad Request", "text/html", result);
        }
      }
    }

    // GET: /bulletin
    else if (html_request_map["uri"] == "/bulletin" && html_request_map["method"] == "GET")
    {
      string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get (bulletin-board items) from backend
        // render_bulletin_board(uids_list)
        // parse item uids (uid1, uid2, ....)
        // for each uid, fetch (bulletin/uid owner, bulletin/uid timestamp, bulletin/uid title, bulletin/uid message)
        // store as list of BulletinMsg objects
        // render bulletin board
      }
    }

    // GET: /my-bulletins
    else if (html_request_map["uri"] == "/my-bulletins" && html_request_map["method"] == "GET")
    {
      string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // get (username_bulletin items) from backend
        // render_my_bulletins(uids_list)
        // parse item uids (uid1, uid2, ....)
        // for each uid, fetch (bulletin/uid timestamp, bulletin/uid title, bulletin/uid message)
        // store as list of BulletinMsg objects
        // render as list with edit and delete buttons
        // if click on edit button, /edit-bulletin?title=<title>&msg=<msg>
        // if click on delete button, /delete-bulletin
      }
    }

    // GET: /edit-bulletin?title=<title>&msg=<msg>
    else if (html_request_map["uri"].substr(0, 14) == "/edit-bulletin" && html_request_map["method"] == "GET")
    {
      string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // Get username from cookie
        string username = get_username_from_cookie(cookie, fd);
        if (username.empty())
        {
          redirect(client_fd, "/login");
        }
        else
        {
          map<string, string> params = parse_query_params(html_request_map["uri"]);
          string title = params["title"];
          string msg = params["msg"];
          string response_html = generate_edit_bulletin_form(username, title, msg);
          send_response(client_fd, 200, "OK", "text/html", response_html);
        }
      }
    }

    // POST: /edit-bulletin?title=<title>&msg=<msg>
    else if (html_request_map["uri"].substr(0, 14) == "/edit-bulletin" && html_request_map["method"] == "POST")
    {
      string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        redirect(client_fd, "/login");
      }
      else
      {
        string username = get_username_from_cookie(cookie, fd);
        if (username.empty())
        {
          redirect(client_fd, "/login");
        }
        else
        {
          // parse form data: title, msg
          // owner=username (from cookie)
          // ts = get_timestamp()
          // uid = compute_md5_hash(title + msg)
          // put_bulletin_item_to_backend() :
          // cput uid into (bullet-board items)
          // put bulletin/uid owner, bulletin/uid timestamp, bulletin/uid title, bulletin/uid message
          // cput uid into (username_bulletin items)
          cout << html_request_map["body"] << endl;
          map<string, string> form_data = parseQueryString(html_request_map["body"]);
          for (const auto &pair : form_data)
          {
            std::cout << pair.first << ": " << pair.second << std::endl;
          }
          string title = form_data["title"];
          string msg = form_data["msg"];
          string ts = get_timestamp();
          string uid = compute_md5_hash(title + msg + ts);

          string encoded_owner = base64_encode(username);
          string encoded_ts = base64_encode(ts);
          string encoded_title = base64_encode(title);
          string encoded_msg = base64_encode(msg);

          int result = put_bulletin_item_to_backend(username, encoded_owner, encoded_ts, encoded_title, encoded_msg, uid, g_map_rowkey_to_server, g_coordinator_addr, fd);
          if (result == 0)
          {
            // Redirect to bulletin board view or confirmation page
            redirect(client_fd, "/bulletin-board");
          }
          else
          {
            // Error handling
            send_response(client_fd, 500, "Internal Server Error", "text/plain", "Failed to post bulletin.");
          }
        }
      }
    }

    // GET: /delete-bulletin?uid=<uid>
    else if (html_request_map["uri"].substr(0, 16) == "/delete-bulletin" && html_request_map["method"] == "GET")
    {
      string cookie = get_cookie_from_header(request_header);
      if (cookie.empty())
      {
        // Redirect to login for all other pages
        redirect(client_fd, "/login");
      }
      else
      {
        // parse out uid from uri
        // delete_bulletin_item(username, uid)
      }
    }

    // unknown method
    else
    {
      send_response(client_fd, 405, "Method Not Allowed", "text/html", "");
    }
  }
  close(client_fd);
  return nullptr;
}

int main(int argc, char *argv[])
{
  if (argc == 1)
  {
    cerr << "*** PennCloud: T15" << endl;
    exit(EXIT_FAILURE);
  }

  // install sigint handler
  install_sigint_handler();

  // read and save html files
  g_endpoint_html_map = load_html_files();

  // parse commands
  // <ip>:<port> string for future use
  g_serveraddr_str = parse_commands(argc, argv);
  cout << "IP: " << g_serveraddr_str << endl;
  sockaddr_in server_sockaddr = get_socket_address(g_serveraddr_str);

  // create listening socket
  g_listen_fd = socket(PF_INET, SOCK_STREAM, 0);
  cout << "Listening fd: " << g_listen_fd << endl;
  if (g_listen_fd == -1)
  {
    cerr << "Socket creation failed.\n"
         << endl;
    exit(EXIT_FAILURE);
  }
  int opt = 1;
  if (setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt)) < 0)
  {
    cerr << "Setting socket option failed.\n";
    close(g_listen_fd);
    exit(EXIT_FAILURE);
  }

  // bind listening socket
  if (bind(g_listen_fd, (struct sockaddr *)&server_sockaddr,
           sizeof(server_sockaddr)) != 0)
  {
    cerr << "Socket binding failed.\n"
         << endl;
    close(g_listen_fd);
    exit(EXIT_FAILURE);
  }

  // check listening socket
  if (listen(g_listen_fd, SOMAXCONN) != 0)
  {
    cerr << "Socket listening failed.\n"
         << endl;
    close(g_listen_fd);
    exit(EXIT_FAILURE);
  }

  // listen to messages from client (user)
  while (true)
  {
    sockaddr_in client_sockaddr;
    socklen_t client_socklen = sizeof(client_sockaddr);
    int client_fd = accept(g_listen_fd, (struct sockaddr *)&client_sockaddr,
                           &client_socklen);

    if (client_fd < 0)
    {
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
      {
        continue;
      }
      cerr << "Failed to accept new connection: " << strerror(errno) << endl;
      break;
    }

    pthread_t thd;
    pthread_create(&thd, nullptr, handle_connection, new int(client_fd));
    pthread_detach(thd);
  }

  close(g_listen_fd);

  return EXIT_SUCCESS;
}