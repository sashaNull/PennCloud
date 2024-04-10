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
string g_server_ip;
int g_server_port;
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

void send_dummy_msg_to_backend() {
  F_2_B_Message test_msg;
  test_msg.type = 1;
  test_msg.rowkey = "adwait_info";
  test_msg.colkey = "name";
  test_msg.errorMessage = "";
  test_msg.status = 0;
  string to_send = encode_message(test_msg);
  string backend_serveraddr_str = "127.0.0.1:6000";
  int fd = socket(PF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    cerr << "Socket creation failed.\n" << endl;
    exit(EXIT_FAILURE);
  }
  sockaddr_in backend_serveraddr = get_socket_address(backend_serveraddr_str);
  cout << "server ip: " << backend_serveraddr.sin_addr.s_addr << endl;

  connect(fd, (struct sockaddr *)&backend_serveraddr,
          sizeof(backend_serveraddr));

  ssize_t bytes_sent = send(fd, to_send.c_str(), to_send.length(), 0);

  if (bytes_sent == -1) {
    cerr << "Sending message failed.\n";
  } else {
    cout << "Message sent successfully: " << bytes_sent << " bytes.\n";
  }

  while (true) {
    const unsigned int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    std::fill(std::begin(buffer), std::end(buffer),
              0); // Initialize buffer to zero

    // Receive a response from the server
    ssize_t bytes_received =
        recv(fd, buffer, BUFFER_SIZE - 1, 0); // Leave space for null terminator

    if (bytes_received == -1) {
      // Receiving failed
      std::cerr << "Receiving message failed.\n";
    } else if (bytes_received == 0) {
      // The server closed the connection
      std::cout << "Server closed the connection.\n";
    } else {
      // Null-terminate the received data (important if you're expecting a
      // string)
      buffer[bytes_received] = '\0';
      std::cout << "Received message: " << buffer << std::endl;
    }
    string received_msg(buffer);
    if (received_msg.substr(0, 1) != "W") {
      F_2_B_Message received_message = decode_message(received_msg);
      cout << "Received message value: " << received_message.value << endl;
    }
  }
  close(fd);
}

// Function to handle a connection.
void *handle_connection(void *arg) {
  int client_fd = *static_cast<int *>(arg);
  delete static_cast<int *>(arg);
  // TODO: handle requests from browser here
  // Receive the request
  const unsigned int BUFFER_SIZE = 4096;
  char buffer[BUFFER_SIZE];

  // Keep listening for requests
  while (true) {
    cout << "Listning..." << endl;
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
    // Handle the request
    if (uri == "/signup" && method == "GET") {
      ifstream file("html_files/signup.html");
      string content((istreambuf_iterator<char>(file)),
                     istreambuf_iterator<char>());
      file.close();
      string response =
          "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + content;
      send(client_fd, response.c_str(), response.length(), 0);
    } else if (uri == "/signup" && method == "POST") {
      cout << "POST request from /signup" << endl;
      // Similar file reading logic for POST request
    } else if (uri == "/login" && method == "GET") {
      ifstream file("html_files/login.html");
      string content((istreambuf_iterator<char>(file)),
                     istreambuf_iterator<char>());
      file.close();
      string response =
          "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + content;
      send(client_fd, response.c_str(), response.length(), 0);
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
  string serveraddr_str = parse_commands(argc, argv);
  cout << "IP: " << serveraddr_str << endl;
  sockaddr_in server_sockaddr = get_socket_address(serveraddr_str);

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