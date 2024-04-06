/*
1. Server reads the config file and reads arguments: -v --> verbose,
serverconfig file, serverIndex, datafolder location.

2. It listens to TCP connections on the port

3. On a connection, it creates a new thread.

4. Thread reads the command and retrieves the key value pair and sends back a
message.

*/

// Imports
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

// Global Varibles
string server_ip;
int server_port;
string data_file_location;
bool verbose = false;
int server_index;
int listen_fd;

map<string, pthread_mutex_t> file_lock_map{};

constexpr int MAX_BUFFER_SIZE = 1024;

F_2_B_Message process_message(const string &serialized) {
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

F_2_B_Message handle_get(F_2_B_Message message) {
  string file_path = data_file_location + "/" + message.rowkey + ".txt";
  ifstream file(file_path);
  if (!file.is_open()) {
    message.status = 1;
    message.errorMessage = "Rowkey does not exist";
    return message;
  }

  string line;
  bool keyFound = false;
  while (getline(file, line)) {
    istringstream iss(line);
    string key, value;
    if (getline(iss, key, ':') && getline(iss, value)) {
      if (key == message.colkey) {
        message.value = value;
        keyFound = true;
        break;
      }
    }
  }

  if (!keyFound) {
    message.status = 1;
    message.errorMessage = "Colkey does not exist";
  } else {
    message.status = 0;
    message.errorMessage.clear();
  }

  file.close();
  return message;
}

F_2_B_Message handle_put(F_2_B_Message message) {
  string file_path = data_file_location + "/" + message.rowkey + ".txt";
  ofstream file(file_path, ios::app);
  if (!file.is_open()) {
    message.status = 1;
    message.errorMessage = "Error opening file for rowkey";
    return message;
  }
  file << message.colkey << ":" << message.value << "\n";
  if (file.fail()) {
    message.status = 1;
    message.errorMessage = "Error writing to file for rowkey";
  } else {
    message.status = 0;
    message.errorMessage = "Data written successfully";
  }

  file.close();
  return message;
}

F_2_B_Message handle_delete(F_2_B_Message message) {
  string file_path = data_file_location + "/" + message.rowkey + ".txt";

  ifstream file(file_path);
  if (!file.is_open()) {
    message.status = 1;
    message.errorMessage = "Rowkey does not exist";
    return message;
  }

  vector<string> lines;
  string line;
  bool keyFound = false;
  while (getline(file, line)) {
    string key;
    stringstream lineStream(line);
    getline(lineStream, key, ':');
    if (key != message.colkey) {
      lines.push_back(line);
    } else {
      keyFound = true;
    }
  }
  file.close();

  if (!keyFound) {
    message.status = 1;
    message.errorMessage = "Colkey does not exist";
    return message;
  }

  // Rewrite the file without the deleted colkey
  ofstream outFile(file_path, ios::trunc);
  for (const auto &l : lines) {
    outFile << l << endl;
  }
  outFile.close();

  message.status = 0; // assuming 0 means success
  message.errorMessage = "Colkey deleted successfully";

  return message;
}

F_2_B_Message handle_cput(F_2_B_Message message) {
  string file_path = data_file_location + "/" + message.rowkey + ".txt";
  ifstream file(file_path);
  if (!file.is_open()) {
    message.status = 1;
    message.errorMessage = "Rowkey does not exist";
    return message;
  }

  bool keyFound = false;
  bool valueUpdated = false;
  vector<string> lines;
  string line;

  while (getline(file, line)) {
    string key, value;
    stringstream lineStream(line);
    getline(lineStream, key, ':');
    getline(lineStream, value);

    if (key == message.colkey) {
      keyFound = true;
      if (value == message.value) {
        lines.push_back(key + ":" + message.value2);
        valueUpdated = true;
      } else {
        lines.push_back(line);
      }
    } else {
      lines.push_back(line);
    }
  }
  file.close();

  if (keyFound && valueUpdated) {
    ofstream outFile(file_path, ios::trunc);
    for (const auto &l : lines) {
      outFile << l << endl;
    }
    outFile.close();

    message.status = 0;
    message.errorMessage = "Value updated successfully";
  } else if (keyFound && !valueUpdated) {
    message.status = 1;
    message.errorMessage = "Old value does not match";
  } else {
    message.status = 1;
    message.errorMessage = "Colkey does not exist";
  }

  return message;
}

// Function for reliable message receipt
bool do_read(int client_fd, char *client_buf) {
  size_t n = MAX_BUFFER_SIZE;
  size_t bytes_left = n;
  bool r_arrived = false;

  while (bytes_left > 0) {
    ssize_t result = read(client_fd, client_buf + n - bytes_left, 1);

    if (result == -1) {
      if ((errno == EINTR) || (errno == EAGAIN)) {
        continue;
      }

      return false;
    } else if (result == 0) {
      return false;
    }

    if (r_arrived && client_buf[n - bytes_left] == '\n') {
      client_buf[n - bytes_left + 1] = '\0';
      break;
    } else {
      r_arrived = false;
    }

    if (client_buf[n - bytes_left] == '\r') {
      r_arrived = true;
    }

    bytes_left -= result;
  }

  client_buf[MAX_BUFFER_SIZE - 1] = '\0';
  return true;
}

// Function to handle a connection.
void *handle_connection(void *arg) {
  int client_fd = *static_cast<int *>(arg);
  delete static_cast<int *>(arg);

  char buffer[MAX_BUFFER_SIZE];

  string response = "WELCOME TO THE SERVER";
  ssize_t bytes_sent = send(client_fd, response.c_str(), response.length(), 0);
  if (bytes_sent < 0) {
    cerr << "Error in send(). Exiting" << endl;
    return nullptr;
  }

  while (do_read(client_fd, buffer)) {
    string message(buffer);
    if (verbose) {
      cout << "[" << client_fd << "] C: " << message;
    }

    if (message == "quit\r\n") {
      cout << "Quit command received. Closing connection." << endl;
      break;
    }

    F_2_B_Message f2b_message = process_message(message);
    if (verbose) {
      cout << "Message details:" << endl;
      cout << "Type: " << f2b_message.type << endl;
      cout << "Rowkey: " << f2b_message.rowkey << endl;
      cout << "Colkey: " << f2b_message.colkey << endl;
      cout << "Value: " << f2b_message.value << endl;
      cout << "Value2: " << f2b_message.value2 << endl;
      cout << "Status: " << f2b_message.status << endl;
      cout << "ErrorMessage: " << f2b_message.errorMessage << endl;
    }

    // Process the message based on type
    switch (f2b_message.type) {
    case 1: // get
      pthread_mutex_lock(&file_lock_map[f2b_message.rowkey+".txt"]);
      f2b_message = handle_get(f2b_message);
      pthread_mutex_unlock(&file_lock_map[f2b_message.rowkey+".txt"]);
      break;
    case 2: // put
      pthread_mutex_lock(&file_lock_map[f2b_message.rowkey+".txt"]);
      f2b_message = handle_put(f2b_message);
      pthread_mutex_unlock(&file_lock_map[f2b_message.rowkey+".txt"]);
      break;
    case 3: // delete
      pthread_mutex_lock(&file_lock_map[f2b_message.rowkey+".txt"]);
      f2b_message = handle_delete(f2b_message);
      pthread_mutex_unlock(&file_lock_map[f2b_message.rowkey+".txt"]);
      break;
    case 4: // cput
      pthread_mutex_lock(&file_lock_map[f2b_message.rowkey+".txt"]);
      f2b_message = handle_cput(f2b_message);
      pthread_mutex_unlock(&file_lock_map[f2b_message.rowkey+".txt"]);
      break;
    default:
      cout << "Unknown command type received" << endl;
      break;
    }

    ostringstream oss;
    oss << f2b_message.type << "|" << f2b_message.rowkey << "|"
        << f2b_message.colkey << "|" << f2b_message.value << "|"
        << f2b_message.value2 << "|" << f2b_message.status << "|"
        << f2b_message.errorMessage << "\r\n";
    string serialized = oss.str();

    bytes_sent = send(client_fd, serialized.c_str(), serialized.length(), 0);
    if (bytes_sent < 0) {
      cerr << "Error in send(). Exiting" << endl;
      break;
    }
  }

  cout << "Closing connection" << endl;
  close(client_fd);
  return nullptr;
}

sockaddr_in parse_address(char *raw_line) {
  sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;

  char *token = strtok(raw_line, ":");
  server_ip = string(token);
  inet_pton(AF_INET, token, &addr.sin_addr);

  token = strtok(NULL, ",");
  server_port = atoi(token);
  addr.sin_port = htons(atoi(token));

  data_file_location = strtok(NULL, "\n");
  return addr;
}

sockaddr_in parse_config_file(string config_file) {
  ifstream config_stream(config_file);
  sockaddr_in server_sockaddr;
  int i = 0;
  string line;
  while (getline(config_stream, line)) {
    if (i == server_index) {
      char raw_line[line.length() + 1];
      strcpy(raw_line, line.c_str());

      server_sockaddr = parse_address(raw_line);
    }
    i++;
  }
  return server_sockaddr;
}

void exit_handler(int sig) {
  cout << "SIGINT received, shutting down." << endl;
  if (listen_fd >= 0) {
    close(listen_fd);
  }
  exit(EXIT_SUCCESS);
}

void initialize_file_lock() {
  DIR *dir = opendir(data_file_location.c_str());
  if (dir) {
    struct dirent *entry;

    while ((entry = readdir(dir)) != nullptr) {
      string file_name = entry->d_name;
      if (file_name != "." && file_name != "..") {
        pthread_mutex_t file_lock{};
        pthread_mutex_init(&file_lock, nullptr);
        file_lock_map[file_name] = file_lock;
      }
    }

    closedir(dir);
  }
}

// Main Function
int main(int argc, char *argv[]) {
  if (argc == 1) {
    cerr << "*** PennCloud: T15" << endl;
    exit(EXIT_FAILURE);
  }

  int option;
  // parsing using getopt()
  while ((option = getopt(argc, argv, "vo:")) != -1) {
    switch (option) {
    case 'v':
      verbose = true;
      break;
    default:
      // Print usage example for incorrect command-line options
      cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
      exit(EXIT_FAILURE);
    }
  }
  // get config file
  if (optind == argc) {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }
  string config_file = argv[optind];

  optind++;
  if (optind == argc) {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }

  listen_fd = socket(PF_INET, SOCK_STREAM, 0);

  if (listen_fd == -1) {
    cerr << "Socket creation failed.\n" << endl;
    exit(EXIT_FAILURE);
  }

  int opt = 1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt)) < 0) {
    cerr << "Setting socket option failed.\n";
    close(listen_fd);
    exit(EXIT_FAILURE);
  }

  server_index = atoi(argv[optind]);
  sockaddr_in server_sockaddr = parse_config_file(config_file);
  if (verbose) {
    cout << "IP: " << server_ip << ":" << server_port << endl;
    cout << "Data Loc:" << data_file_location << endl;
    cout << "Server Index: " << server_index << endl;
    cout << server_sockaddr.sin_addr.s_addr << endl;
    cout << ntohs(server_sockaddr.sin_port) << endl;
  }

  if (bind(listen_fd, (struct sockaddr *)&server_sockaddr,
           sizeof(server_sockaddr)) != 0) {
    cerr << "Socket binding failed.\n" << endl;
    close(listen_fd);
    exit(EXIT_FAILURE);
  }
  signal(SIGINT, exit_handler);

  if (listen(listen_fd, SOMAXCONN) != 0) {
    cerr << "Socket listening failed.\n" << endl;
    close(listen_fd);
    exit(EXIT_FAILURE);
  }

  initialize_file_lock();

  while (true) {
    sockaddr_in client_sockaddr;
    socklen_t client_socklen = sizeof(client_sockaddr);
    int client_fd =
        accept(listen_fd, (struct sockaddr *)&client_sockaddr, &client_socklen);

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

  close(listen_fd);
  return EXIT_SUCCESS;
}
