#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <pthread.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
using namespace std;

#define PORT 8080
#define HEARTBEAT_TIME 1

struct ServerInfo {
  string ip;
  int port;
  int currentLoad;
  bool is_active;
};

vector<ServerInfo> frontend_server_list;
pthread_mutex_t server_list_lock;

void loadConfig(const std::string &configFile);
void *heartbeat(void *arg);

/**
 * Main function for a load balancer program.
 * 
 * @param argc The number of command-line arguments.
 * @param argv An array of pointers to the arguments.
 * @return int Returns 0 upon successful execution.
 *
 * This function initializes the load balancer, parses command-line options, loads configuration from a file,
 * creates a heartbeat thread, sets up a socket server to listen for incoming connections, and redirects incoming
 * requests to the least loaded server. Additionally, it handles LIST requests to provide information about the
 * available servers.
 */
int main(int argc, char *argv[]) {
  int server_fd, new_socket;
  long valread;
  struct sockaddr_in address;
  int addrlen = sizeof(address);

  char buffer[30000] = {0};

  int opt;
  std::string configFile;
  bool verbose = false;

  // Parse command line options
  while ((opt = getopt(argc, argv, "v")) != -1) {
    switch (opt) {
    case 'v':
      verbose = true;
      break;
    default: /* '?' */
      std::cerr << "Usage: " << argv[0] << " [-v] <config_file>\n";
      exit(EXIT_FAILURE);
    }
  }

  // Check if a configuration file is provided
  if (optind < argc) {
    configFile = argv[optind];
  } else {
    std::cerr << "Usage: ./loadbalancer [-v] <config_file\n";
    exit(EXIT_FAILURE);
  }
  
  // Initialize mutex for server list
  if (pthread_mutex_init(&server_list_lock, NULL) != 0) {
    cerr << "Mutex init has failed\n";
    exit(EXIT_FAILURE);
  }
  
  // Load configuration from file 
  loadConfig(configFile);
  
  // Display verbose information if enabled
  if (verbose) {
    std::cout << "Verbose: " << verbose << std::endl;
    std::cout << "Config File: " << configFile << std::endl;
    for (auto ele : frontend_server_list) {
      cout << "SERVER: " << ele.ip << " " << ele.port << " " << ele.currentLoad
           << endl;
    }
  }

  pthread_t hb_thread;
  // Create a heartbeat thread
  if (pthread_create(&hb_thread, NULL, heartbeat, (void *)&verbose) != 0) {
    cerr << "Failed to create heartbeat thread" << endl;
    exit(EXIT_FAILURE);
  }

  // Setting up the socket server
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("In socket");
    exit(EXIT_FAILURE);
  }

  // Set socket to allow reuse of the address
  int optval = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) <
      0) {
    perror("setsockopt(SO_REUSEADDR) failed");
    exit(EXIT_FAILURE);
  }

  // Configure socket address
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  memset(address.sin_zero, '\0', sizeof address.sin_zero);

  // Bind socket to address
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("In bind");
    exit(EXIT_FAILURE);
  }

  // Start listening for incoming connections
  if (listen(server_fd, 10) < 0) {
    perror("In listen");
    exit(EXIT_FAILURE);
  }

  // Main loop to accept and handle incoming connections
  while (1) {
    printf("\n+++++++ Waiting for new connection ++++++++\n\n");
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                             (socklen_t *)&addrlen)) < 0) {
      perror("In accept");
      exit(EXIT_FAILURE);
    }

    // Read request from the client
    valread = read(new_socket, buffer, 30000);
    printf("%s\n", buffer);

    string request(buffer);

    // Check if the request is for LIST
    if (request.find("GET /LIST HTTP") != string::npos) {
      stringstream response;
      pthread_mutex_lock(&server_list_lock);

      // Construct LIST response
      response << "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
      for (const auto &server : frontend_server_list) {
        response << server.ip << ":" << server.port << "#"
                 << (server.is_active ? "1" : "0") << ",";
      }

      pthread_mutex_unlock(&server_list_lock);

      // Remove the last comma
      string response_str = response.str();
      if (!response_str.empty())
        response_str.pop_back();

      // Send LIST response to the client
      write(new_socket, response_str.c_str(), response_str.length());
      close(new_socket);
      continue;
    }

    // Lock the mutex to safely access the server list
    pthread_mutex_lock(&server_list_lock);
    ServerInfo *least_loaded_server = nullptr;
    for (auto &server : frontend_server_list) {
      if (server.is_active &&
          (least_loaded_server == nullptr ||
           server.currentLoad < least_loaded_server->currentLoad)) {
        least_loaded_server = &server;
      }
    }
    if (least_loaded_server != nullptr) {
      least_loaded_server->currentLoad++;
      if (verbose)
        cout << "Redirecting to " << least_loaded_server->ip << ":"
             << least_loaded_server->port << endl;
    }
    pthread_mutex_unlock(&server_list_lock);

    // Redirect the client to the least loaded server
    if (least_loaded_server) {
      string redirect_response =
          "HTTP/1.1 307 Temporary Redirect\nLocation: http://" +
          least_loaded_server->ip + ":" + to_string(least_loaded_server->port) +
          "\n\n";

      write(new_socket, redirect_response.c_str(), redirect_response.length());
    } else {
      // If no server available, send service unavailable response
      string error_response =
          "HTTP/1.1 503 Service Unavailable\nContent-Type: "
          "text/plain\nContent-Length: 19\n\nNo available servers";
      write(new_socket, error_response.c_str(), error_response.length());
    }

    close(new_socket);
  }

  // Destroy the mutex
  pthread_mutex_destroy(&server_list_lock);
  return 0;
}

/**
 * Loads configuration from a specified file and populates the frontend_server_list.
 *
 * @param configFile The path to the configuration file.
 * @return void
 */
void loadConfig(const std::string &configFile) {
  ifstream file(configFile);
  if (!file) {
    cerr << "Unable to open config file: " << configFile << endl;
    exit(EXIT_FAILURE);
  }

  string line;
  while (getline(file, line)) {
    if (line.empty())
      continue; // Skip empty lines

    stringstream ss(line);
    string ip;
    int port;

    // Splitting IP and port
    getline(ss, ip, ':');
    ss >> port;

    // Lock the mutex before accessing the shared vector
    pthread_mutex_lock(&server_list_lock);

    // Adding server information to the list
    frontend_server_list.push_back(
        {ip, port, 0}); // Initialize currentLoad as 0

    // Unlock the mutex after modifying the vector
    pthread_mutex_unlock(&server_list_lock);
  }
}

/**
 * Monitors the health status of frontend servers by periodically sending heartbeat requests.
 *
 * @param arg A pointer to a boolean flag indicating whether verbose output is enabled.
 * @return void* Returns NULL upon completion.
 */
void *heartbeat(void *arg) {

  bool verbose = *(bool *)arg;

  while (true) {
    pthread_mutex_lock(&server_list_lock);
    for (auto &server : frontend_server_list) {
      int sock = socket(AF_INET, SOCK_STREAM, 0);
      struct sockaddr_in server_addr;
      memset(&server_addr, 0, sizeof(server_addr));
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(server.port);
      server_addr.sin_addr.s_addr = inet_addr(server.ip.c_str());

      // Check server connectivity
      if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
          0) {
        server.is_active = false;
        if (verbose)
          cout << "Server " << server.ip << ":" << server.port << " is down."
               << endl;
      } else {
        // Send status request to server
        std::string request =
            "GET /status HTTP/1.1\r\nHost: " + server.ip + "\r\n\r\n";
        send(sock, request.c_str(), request.size(), 0);

        // Receive response from server
        char response[1024] = {0};
        read(sock, response, 1024);
        string response_str(response);

        // Check response status
        if (response_str.find("200 OK") != string::npos) {
          server.is_active = true;
          if (verbose)
            cout << "Server " << server.ip << ":" << server.port
                 << " is active." << endl;
        } else {
          server.is_active = false;
          if (verbose)
            cout << "Server " << server.ip << ":" << server.port << " is down."
                 << endl;
        }

        close(sock);
      }
    }
    pthread_mutex_unlock(&server_list_lock);

    // Sleep for heartbeat interval
    sleep(HEARTBEAT_TIME);
  }

  return NULL;
}
