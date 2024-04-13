#include <arpa/inet.h>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "../utils/utils.h"
using namespace std;

bool verbose = false;
string g_serveraddr_str;
int g_listen_fd;

void sigint_handler(int signum) {
  cout << "SIGINT received, shutting down." << endl;
  if (fd_is_open(g_listen_fd)) {
    close(g_listen_fd);
  }
  exit(EXIT_SUCCESS);
}

void install_sigint_handler() {
  if (signal(SIGINT, sigint_handler) == SIG_ERR) {
    cerr << "SIGINT handling in main thread failed" << endl;
    exit(EXIT_FAILURE);
  }
}

sockaddr_in get_socket_address(const string &addr_str) {
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  // parse ip and port
  size_t pos = addr_str.find(':');
  string ip_str = addr_str.substr(0, pos);
  int port = stoi(addr_str.substr(pos + 1));
  // set ip
  if (inet_pton(AF_INET, ip_str.c_str(), &addr.sin_addr) <= 0) {
    cerr << "Invalid IP address format." << endl;
    return addr;
  }
  // set port
  addr.sin_port = htons(static_cast<uint16_t>(port));
  return addr;
}

string parse_commands(int argc, char *argv[]) {
  int cmd;
  string ordering_str;
  while ((cmd = getopt(argc, argv, "o:v")) != -1) {
    switch (cmd) {
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
  if (argv[optind]) {
    config_file_path = argv[optind];
  } else {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }
  if (config_file_path.empty()) {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }
  if (!filepath_is_valid(config_file_path)) {
    cerr << "Config file " << config_file_path << " is not valid." << endl;
    exit(EXIT_FAILURE);
  }
  // get number of servers
  ifstream file(config_file_path);
  string line;
  vector<string> lines;
  int line_count = 0;
  while (getline(file, line)) {
    lines.push_back(line);
    line_count++;
  }
  // get server index
  optind++;
  int server_index;
  if (argv[optind]) {
    server_index = stoi(argv[optind]);
  } else {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }
  if (server_index < 0 || server_index > line_count - 1) {
    cerr << "Error: Server index " << server_index << " is not in valid range."
         << endl;
    exit(EXIT_FAILURE);
  }
  // return server address
  return lines[server_index];
}

F_2_B_Message construct_msg(int type, string rowkey, string colkey, string value, string value2, string errmsg, int status) {
  F_2_B_Message msg;
  msg.type = type;
  msg.rowkey = rowkey;
  msg.colkey = colkey;
  msg.value = value;
  msg.value2 = value2;
  msg.errorMessage = errmsg;
  msg.status = status;
  return msg;
}

F_2_B_Message send_and_receive_msg(int fd, const string &addr_str, F_2_B_Message msg) {
  F_2_B_Message msg_to_return;
  sockaddr_in addr = get_socket_address(addr_str);
  connect(fd, (struct sockaddr *)&addr,
          sizeof(addr));
  string to_send = encode_message(msg);
  cout << "to send: " << to_send << endl;
  ssize_t bytes_sent = send(fd, to_send.c_str(), to_send.length(), 0);
  if (bytes_sent == -1) {
    cerr << "Sending message failed.\n";
  } else {
    cout << "Message sent successfully: " << bytes_sent << " bytes.\n";
  }

  // Receive response from the server
  string buffer;
  while (true) {
    const unsigned int BUFFER_SIZE = 1024;
    char temp_buffer[BUFFER_SIZE];
    memset(temp_buffer, 0, BUFFER_SIZE);
    ssize_t bytes_received = recv(fd, temp_buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received == -1) {
        cerr << "Receiving message failed.\n";
        continue;
    } else if (bytes_received == 0) {
        cout << "Server closed the connection.\n";
        break;
    } else {
        temp_buffer[bytes_received] = '\0';
        buffer += string(temp_buffer);
    }

    size_t pos;
    while ((pos = buffer.find("\r\n")) != string::npos) {
        string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 2);

        if (!line.empty() && line != "WELCOME TO THE SERVER") {
            msg_to_return = decode_message(line);
            return msg_to_return;
        }
    }
  }
  return msg_to_return;
}

void redirect(int client_fd, std::string redirect_to) {
  std::string response = "HTTP/1.1 302 Found\r\n";
  response += "Location: " + redirect_to + "\r\n";
  response += "Content-Length: 0\r\n";
  response += "Connection: keep-alive\r\n";
  response += "\r\n";

  send(client_fd, response.c_str(), response.size(), 0);

  std::cout << "Sent redirection response to " << redirect_to << std::endl;
}

// Function to handle a connection.
void *handle_connection(void *arg) {
  int client_fd = *static_cast<int *>(arg);
  delete static_cast<int *>(arg);

  // Receive the request
  const unsigned int BUFFER_SIZE = 4096;
  char buffer[BUFFER_SIZE];

  // Keep listening for requests
  while (true) {
    cout << "Listening..." << endl;
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
      if (bytes_read == 0) {
        cout << "Client closed the connection." << endl;
      } else {
        cerr << "Failed to read from socket." << endl;
      }
      break;
    }
    buffer[bytes_read] = '\0';
    string request(buffer);

    // Parse the request
    istringstream request_stream(request);
    string request_line;
    getline(request_stream, request_line);
    cout << request_line << endl;
    string method, uri, http_version;
    istringstream request_line_stream(request_line);
    request_line_stream >> method >> uri >> http_version;

    // Extract the headers
    map<string, string> headers;
    string header_line;
    while (getline(request_stream, header_line) && header_line != "\r") {
        size_t delimiter_pos = header_line.find(':');
        if (delimiter_pos != string::npos) {
            string header_name = header_line.substr(0, delimiter_pos);
            string header_value = header_line.substr(delimiter_pos + 2, header_line.length() - delimiter_pos - 3);
            headers[header_name] = header_value;
        }
    }

    // Extract the body, if any
    string body = string(istreambuf_iterator<char>(request_stream), {});
    cout << "body: " << body << endl;

    // GET: rendering signup page
    if (uri == "/signup" && method == "GET") {
      ifstream file("html_files/signup.html");
      string content((istreambuf_iterator<char>(file)),
                     istreambuf_iterator<char>());
      file.close();
      string response =
          "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + content;
      send(client_fd, response.c_str(), response.length(), 0);

    // POST: new user signup
    } else if (uri == "/signup" && method == "POST") {
      cout << "POST request from /signup" << endl;
      cout << request_line << endl;

      // Parse out formData
      map<string,string> form_data = parse_json_string_to_map(body);
      string username = form_data["username"];

      // check if user exists with get
      string backend_serveraddr_str = "127.0.0.1:6000";
      int fd = socket(PF_INET, SOCK_STREAM, 0);
      if (fd == -1) {
        cerr << "Socket creation failed.\n" << endl;
        exit(EXIT_FAILURE);
      }
      F_2_B_Message msg_to_send = construct_msg(1, username+"_info", "", "", "", "", 0);
      F_2_B_Message get_response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);

      if (get_response_msg.status == 1 && strip(get_response_msg.errorMessage) == "Rowkey does not exist") {
        cout << "in if" << endl;
        // Send new user data to backend and receive response
        string firstname = form_data["firstName"];
        string lastname = form_data["lastName"];
        string email = form_data["email"];
        string password = form_data["password"];

        msg_to_send = construct_msg(2, username+"_info", "firstName", firstname, "", "", 0);
        F_2_B_Message response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        if (response_msg.status != 0) {
          cerr << "Error in PUT to backend" << endl;
        }

        msg_to_send = construct_msg(2, username+"_info", "lastName", lastname, "", "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        if (response_msg.status != 0) {
          cerr << "Error in PUT to backend" << endl;
        }
        msg_to_send = construct_msg(2, username+"_info", "email", email, "", "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        if (response_msg.status != 0) {
          cerr << "Error in PUT to backend" << endl;
        }

        msg_to_send = construct_msg(2, username+"_info", "password", password, "", "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        if (response_msg.status != 0) {
          cerr << "Error in PUT to backend" << endl;
        }

        // if successful, ask browser to redirect to /login
        std::string redirect_to = "http://" + g_serveraddr_str + "/login";
        redirect(client_fd, redirect_to);
      } else if (get_response_msg.status == 0) {
        // error: user already exists
        // TODO: alert and then redirect to login?
        // TODO: alert and load empty signup form again?
        // just alert for now
        std::string content = "{\"error\":\"User already exists\"}";
        std::string content_length = std::to_string(content.length());
        std::string http_response = "HTTP/1.1 409 Conflict\r\n"
                                    "Content-Type: application/json\r\n"
                                    "Content-Length: " + content_length + "\r\n"
                                    "\r\n";
        http_response += content;

        ssize_t bytes_sent = send(client_fd, http_response.c_str(), http_response.size(), 0);
        if (bytes_sent < 0) {
            std::cerr << "Failed to send response" << std::endl;
        } else {
            std::cout << "Sent response successfully, bytes sent: " << bytes_sent << std::endl;
        }

      } else {
        // error: some other type of error
      }
    // GET: rendering login page
    } else if (uri == "/login" && method == "GET") {
      cout << "in render login" << endl;
      ifstream file("html_files/login.html");
      string content((istreambuf_iterator<char>(file)),
                     istreambuf_iterator<char>());
      file.close();
      string response =
          "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + content;
      send(client_fd, response.c_str(), response.length(), 0);

    // POST: user login
    } else if (uri == "/login" && method == "POST") {
    } else {
      string response = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
      send(client_fd, response.c_str(), response.length(), 0);
    }
  }
  cout << "Closing connection" << endl;
  close(client_fd);
  return nullptr;
}

int main(int argc, char *argv[]) {
  if (argc == 1) {
    cerr << "*** PennCloud: T15" << endl;
    exit(EXIT_FAILURE);
  }

  // install sigint handler
  install_sigint_handler();

  // parse commands
  // <ip>:<port> string for future use
  g_serveraddr_str = parse_commands(argc, argv);
  cout << "IP: " << g_serveraddr_str << endl;
  sockaddr_in server_sockaddr = get_socket_address(g_serveraddr_str);

  // create listening socket
  g_listen_fd = socket(PF_INET, SOCK_STREAM, 0);
  cout << "Listening fd: " << g_listen_fd << endl;
  if (g_listen_fd == -1) {
    cerr << "Socket creation failed.\n" << endl;
    exit(EXIT_FAILURE);
  }
  int opt = 1;
  if (setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt)) < 0) {
    cerr << "Setting socket option failed.\n";
    close(g_listen_fd);
    exit(EXIT_FAILURE);
  }

  // bind listening socket
  if (bind(g_listen_fd, (struct sockaddr *)&server_sockaddr,
           sizeof(server_sockaddr)) != 0) {
    cerr << "Socket binding failed.\n" << endl;
    close(g_listen_fd);
    exit(EXIT_FAILURE);
  }

  // check listening socket
  if (listen(g_listen_fd, SOMAXCONN) != 0) {
    cerr << "Socket listening failed.\n" << endl;
    close(g_listen_fd);
    exit(EXIT_FAILURE);
  }

  // send_dummy_msg_to_backend();

  // TODO: is connection from client supposed to be TCP? Or just UDP
  // listen to messages from client (user)
  while (true) {
    sockaddr_in client_sockaddr;
    socklen_t client_socklen = sizeof(client_sockaddr);
    int client_fd = accept(g_listen_fd, (struct sockaddr *)&client_sockaddr,
                           &client_socklen);

    if (client_fd < 0) {
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)) {
        // Retry if interrupted or non-blocking operation would block
        continue;
      }
      cerr << "Failed to accept new connection: " << strerror(errno) << endl;
      break; // Break the loop on other errors
    }

    pthread_t thd;
    pthread_create(&thd, nullptr, handle_connection, new int(client_fd));
    pthread_detach(thd);
  }

  close(g_listen_fd);

  return EXIT_SUCCESS;
}