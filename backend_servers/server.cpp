// Includes necessary header files for socket programming, file manipulation,
// and threading
#include <algorithm>
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

// Includes custom utility functions
#include "../utils/utils.h"

// Namespace declaration for convenience
using namespace std;

// Global variables for server configuration and state
map<string, pthread_mutex_t> file_lock_map{}; // Map to store file locks
vector<int> client_fds{};                     // All client file descriptors
string server_ip;                             // Server IP address
string data_file_location;                    // Location of data files
int server_port;      // Port on which server listens for connections
int server_index;     // Index of the server
int listen_fd;        // File descriptor for the listening socket
bool verbose = false; // Verbosity flag for debugging

// Maximum buffer size for data transmission
constexpr int MAX_BUFFER_SIZE = 1024;

// Function prototypes for handling different types of requests
F_2_B_Message handle_get(F_2_B_Message message);
F_2_B_Message handle_put(F_2_B_Message message);
F_2_B_Message handle_cput(F_2_B_Message message);
F_2_B_Message handle_delete(F_2_B_Message message);

// Function prototypes for parsing and initializing server configuration
sockaddr_in parse_address(char *raw_line);
sockaddr_in parse_config_file(string config_file);

// Signal handler function for clean exit
void exit_handler(int sig);

// Function for initializing file locks
void initialize_file_lock();

// Function prototype for handling client connections in separate threads
void *handle_connection(void *arg);

// Function prototype for reading data from client socket
bool do_read(int client_fd, char *client_buf);

int main(int argc, char *argv[]) {
  // Check if there are enough command-line arguments
  if (argc == 1) {
    cerr << "*** PennCloud: T15" << endl;
    exit(EXIT_FAILURE);
  }

  int option;
  // Parse command-line options
  while ((option = getopt(argc, argv, "vo:")) != -1) {
    switch (option) {
    case 'v':
      verbose = true;
      break;
    default:
      // Incorrect syntax for command-line arguments
      cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
      exit(EXIT_FAILURE);
    }
  }

  // Ensure there are enough arguments after parsing options
  if (optind == argc) {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }

  // Extract configuration file name
  string config_file = argv[optind];

  // Extract server index
  optind++;
  if (optind == argc) {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }

  // Create a socket
  listen_fd = socket(PF_INET, SOCK_STREAM, 0);

  if (listen_fd == -1) {
    cerr << "Socket creation failed.\n" << endl;
    exit(EXIT_FAILURE);
  }

  // Set socket options
  int opt = 1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt)) < 0) {
    cerr << "Setting socket option failed.\n";
    close(listen_fd);
    exit(EXIT_FAILURE);
  }

  // Parse configuration file and extract server address and port
  server_index = atoi(argv[optind]);
  sockaddr_in server_sockaddr = parse_config_file(config_file);
  if (verbose) {
    cout << "Server IP: " << server_ip << ":" << server_port << endl;
    cout << "Server Port: " << ntohs(server_sockaddr.sin_port) << endl;
    cout << "Data Loc:" << data_file_location << endl;
    cout << "Server Index: " << server_index << endl;
  }

  // Bind the socket to the server address
  if (bind(listen_fd, (struct sockaddr *)&server_sockaddr,
           sizeof(server_sockaddr)) != 0) {
    cerr << "Socket binding failed.\n" << endl;
    close(listen_fd);
    exit(EXIT_FAILURE);
  }

  // Start listening for incoming connections
  if (listen(listen_fd, SOMAXCONN) != 0) {
    cerr << "Socket listening failed.\n" << endl;
    close(listen_fd);
    exit(EXIT_FAILURE);
  }

  // Initialize file lock for handling file operations
  initialize_file_lock();

  // Register signal handler for clean exit
  signal(SIGINT, exit_handler);

  // Accept and handle incoming connections
  while (true) {
    sockaddr_in client_sockaddr;
    socklen_t client_socklen = sizeof(client_sockaddr);
    int client_fd =
        accept(listen_fd, (struct sockaddr *)&client_sockaddr, &client_socklen);

    if (client_fd < 0) {
      // If accept fails due to certain errors, continue accepting
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)) {
        continue;
      }
      // Otherwise, print error and exit loop
      cerr << "Failed to accept new connection: " << strerror(errno) << endl;
      break;
    }

    // Push into the list of all client file descriptors
    client_fds.push_back(client_fd);

    if (verbose) {
      cout << "[" << client_fd << "] New connection\n";
    }

    // Create a new thread to handle the connection
    pthread_t thd;
    pthread_create(&thd, nullptr, handle_connection, new int(client_fd));
    pthread_detach(thd);
  }

  // Close the listening socket and exit
  close(listen_fd);
  return EXIT_SUCCESS;
}

/**
 * Handles the GET operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message to be processed.
 * @return F_2_B_Message The processed F_2_B_Message containing the retrieved
 * value or error message.
 */
F_2_B_Message handle_get(F_2_B_Message message) {
  // Construct file path for the specified rowkey
  string file_path = data_file_location + "/" + message.rowkey + ".txt";

  // Open file for reading
  ifstream file(file_path);

  // Check if file exists and can be opened
  if (!file.is_open()) {
    // If file does not exist, set error status and message
    message.status = 1;
    message.errorMessage = "Rowkey does not exist";
    return message;
  }

  // Iterate through file lines to find the specified colkey
  string line;
  bool keyFound = false;
  while (getline(file, line)) {
    istringstream iss(line);
    string key, value;
    // Extract key-value pairs from the line
    if (getline(iss, key, ':') && getline(iss, value)) {
      // Check if key matches the requested colkey
      if (key == message.colkey) {
        // If key found, set message value and flag
        message.value = value;
        keyFound = true;
        break;
      }
    }
  }

  // If requested colkey not found, set error status and message
  if (!keyFound) {
    message.status = 1;
    message.errorMessage = "Colkey does not exist";
  } else {
    // Otherwise, clear error message and set success status
    message.status = 0;
    message.errorMessage.clear();
  }

  // Close file and return processed message
  file.close();
  return message;
}

/**
 * Handles the PUT operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message containing data to be written.
 * @return F_2_B_Message The processed F_2_B_Message containing status and error
 * message.
 */
F_2_B_Message handle_put(F_2_B_Message message) {
  // Construct file path for the specified rowkey
  string file_path = data_file_location + "/" + message.rowkey + ".txt";

  // Open file for writing in append mode
  ofstream file(file_path, ios::app);

  // Check if file can be opened
  if (!file.is_open()) {
    // If file cannot be opened, set error status and message
    message.status = 1;
    message.errorMessage = "Error opening file for rowkey";
    return message;
  }

  // Write data to file
  file << message.colkey << ":" << message.value << "\n";

  // Check for write errors
  if (file.fail()) {
    // If error occurred while writing, set error status and message
    message.status = 1;
    message.errorMessage = "Error writing to file for rowkey";
  } else {
    // Otherwise, set success status and message
    message.status = 0;
    message.errorMessage = "Data written successfully";
  }

  // Close file and return processed message
  file.close();
  return message;
}

/**
 * Handles the CPUT operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message containing data to be updated.
 * @return F_2_B_Message The processed F_2_B_Message containing status and error
 * message.
 */
F_2_B_Message handle_cput(F_2_B_Message message) {
  // Construct file path for the specified rowkey
  string file_path = data_file_location + "/" + message.rowkey + ".txt";

  // Open file for reading
  ifstream file(file_path);

  // Check if file exists and can be opened
  if (!file.is_open()) {
    // If file does not exist, set error status and message
    message.status = 1;
    message.errorMessage = "Rowkey does not exist";
    return message;
  }

  bool keyFound = false;
  bool valueUpdated = false;
  vector<string> lines;
  string line;

  // Read file line by line
  while (getline(file, line)) {
    string key, value;
    stringstream lineStream(line);
    getline(lineStream, key, ':');
    getline(lineStream, value);

    // Check if colkey matches
    if (key == message.colkey) {
      keyFound = true;
      // Check if current value matches message value
      if (value == message.value) {
        // Update value if match found
        lines.push_back(key + ":" + message.value2);
        valueUpdated = true;
      } else {
        // Otherwise, keep the original line
        lines.push_back(line);
      }
    } else {
      // Keep the original line
      lines.push_back(line);
    }
  }
  file.close();

  // Check conditions for updating value
  if (keyFound && valueUpdated) {
    // If key and value updated successfully, rewrite file
    ofstream outFile(file_path, ios::trunc);
    for (const auto &l : lines) {
      outFile << l << endl;
    }
    outFile.close();

    // Set success status and message
    message.status = 0;
    message.errorMessage = "Value updated successfully";
  } else if (keyFound && !valueUpdated) {
    // If old value does not match, set error status and message
    message.status = 1;
    message.errorMessage = "Old value does not match";
  } else {
    // If colkey does not exist, set error status and message
    message.status = 1;
    message.errorMessage = "Colkey does not exist";
  }

  // Return processed message
  return message;
}

/**
 * Handles the DELETE operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message containing data to be deleted.
 * @return F_2_B_Message The processed F_2_B_Message containing status and error
 * message.
 */
F_2_B_Message handle_delete(F_2_B_Message message) {
  // Construct file path for the specified rowkey
  string file_path = data_file_location + "/" + message.rowkey + ".txt";

  // Open file for reading
  ifstream file(file_path);

  // Check if file exists and can be opened
  if (!file.is_open()) {
    // If file does not exist, set error status and message
    message.status = 1;
    message.errorMessage = "Rowkey does not exist";
    return message;
  }

  vector<string> lines;
  string line;
  bool keyFound = false;

  // Read file line by line
  while (getline(file, line)) {
    string key;
    stringstream lineStream(line);
    getline(lineStream, key, ':');
    // If colkey doesn't match, keep the line
    if (key != message.colkey) {
      lines.push_back(line);
    } else {
      // If colkey matches, mark as found
      keyFound = true;
    }
  }
  file.close();

  // If colkey not found, set error status and message
  if (!keyFound) {
    message.status = 1;
    message.errorMessage = "Colkey does not exist";
    return message;
  }

  // Open file for writing (truncating previous content)
  ofstream outFile(file_path, ios::trunc);
  // Write remaining lines to file
  for (const auto &l : lines) {
    outFile << l << endl;
  }
  outFile.close();

  // Set success status and message
  message.status = 0;
  message.errorMessage = "Colkey deleted successfully";

  // Return processed message
  return message;
}

/**
 * Parses the raw address information from a string and returns a sockaddr_in
 * structure.
 *
 * @param raw_line The raw address information in the format
 * "ip_address:port_number,data_file_location".
 * @return sockaddr_in The parsed sockaddr_in structure representing the
 * address.
 */
sockaddr_in parse_address(char *raw_line) {
  // Initialize sockaddr_in structure
  sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;

  // Parse IP address
  char *token = strtok(raw_line, ":");
  server_ip = string(token); // Assuming `server_ip` is a global variable
  inet_pton(AF_INET, token, &addr.sin_addr);

  // Parse port number
  token = strtok(NULL, ",");
  server_port = atoi(token); // Assuming `server_port` is a global variable
  addr.sin_port = htons(atoi(token));

  // Parse data file location
  data_file_location =
      strtok(NULL, "\n"); // Assuming `data_file_location` is a global variable

  return addr;
}

/**
 * Parses the configuration file to extract server address information.
 *
 * @param config_file The path to the configuration file.
 * @return sockaddr_in The sockaddr_in structure representing the server
 * address.
 */
sockaddr_in parse_config_file(string config_file) {
  // Open configuration file for reading
  ifstream config_stream(config_file);
  // Initialize sockaddr_in structure
  sockaddr_in server_sockaddr;
  // Initialize index counter
  int i = 0;
  string line;
  // Read each line of the configuration file
  while (getline(config_stream, line)) {
    // Check if current line corresponds to the server index
    if (i == server_index) {
      // Convert string line to C-style string
      char raw_line[line.length() + 1];
      strcpy(raw_line, line.c_str());
      // Parse address from the line
      server_sockaddr = parse_address(raw_line);
    }
    // Increment index counter
    i++;
  }
  // Close configuration file
  config_stream.close();
  // Return parsed server address
  return server_sockaddr;
}

/**
 * Signal handler function to handle server shutdown.
 *
 * @param sig The signal received for server shutdown.
 *            Typically SIGINT or SIGTERM.
 */
void exit_handler(int sig) {
  // Prepare shutdown message
  string shutdown_message = "Server shutting down!\r\n";
  // Iterate through client file descriptors
  for (const auto &client_fd : client_fds) {
    // Set socket to non-blocking mode
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags != -1) {
      flags |= O_NONBLOCK;
      if (fcntl(client_fd, F_SETFL, flags) == -1) {
        perror("fcntl");
      }
    }
    // Send shutdown message to client
    ssize_t bytes_sent =
        send(client_fd, shutdown_message.c_str(), shutdown_message.length(), 0);
    // Display shutdown message if in verbose mode
    if (verbose) {
      cerr << "[" << client_fd << "] S: " << shutdown_message;
      cout << "[" << client_fd << "] Connection closed\n";
    }
    // Close client socket
    close(client_fd);
  }

  // Close listening socket if it is open
  if (listen_fd >= 0) {
    close(listen_fd);
  }
  // Exit the server process with success status
  exit(EXIT_SUCCESS);
}

/**
 * Initializes file locks for all files in the data directory.
 *
 * This function creates a pthread_mutex_t for each file in the data directory
 * and stores it in the file_lock_map for later use.
 */
void initialize_file_lock() {
  // Open the data directory
  DIR *dir = opendir(data_file_location.c_str());
  if (dir) {
    struct dirent *entry;
    // Iterate over each entry in the directory
    while ((entry = readdir(dir)) != nullptr) {
      string file_name = entry->d_name;
      // Skip "." and ".." entries
      if (file_name != "." && file_name != "..") {
        // Initialize a pthread_mutex_t for the file lock
        pthread_mutex_t file_lock{};
        pthread_mutex_init(&file_lock, nullptr);
        // Store the file lock in the file_lock_map
        file_lock_map[file_name] = file_lock;
      }
    }
    // Close the directory
    closedir(dir);
  }
}

/**
 * Function to handle client connections.
 *
 * @param arg Pointer to the client file descriptor.
 * @return nullptr
 */
void *handle_connection(void *arg) {
  // Extract client file descriptor from argument
  int client_fd = *static_cast<int *>(arg);
  delete static_cast<int *>(arg); // Delete memory allocated for the argument

  // Send welcome message to client
  string response = "WELCOME TO THE SERVER\r\n";
  ssize_t bytes_sent = send(client_fd, response.c_str(), response.length(), 0);
  // Check for send errors
  if (bytes_sent < 0) {
    cerr << "[" << client_fd << "] Error in send(). Exiting" << endl;
    return nullptr;
  }

  char buffer[MAX_BUFFER_SIZE];
  // Continue reading client messages until quit command received
  while (do_read(client_fd, buffer)) {
    string message(buffer);
    // Print received message if in verbose mode
    if (verbose) {
      cout << "[" << client_fd << "] C: " << message;
    }

    // Check for quit command
    if (message == "quit\r\n") {
      string goodbye = "Quit command received. Server goodbye!\r\n";
      bytes_sent = send(client_fd, goodbye.c_str(), goodbye.length(), 0);
      // Check for send errors
      if (bytes_sent < 0) {
        cerr << "[" << client_fd << "] Error in send(). Exiting" << endl;
        break;
      }

      // Print goodbye message if in verbose mode
      if (verbose) {
        cout << "[" << client_fd << "] S: " << goodbye;
      }
      break;
    }

    // Decode received message into F_2_B_Message
    F_2_B_Message f2b_message = decode_message(message);

    // Handle message based on its type
    switch (f2b_message.type) {
    case 1:
      pthread_mutex_lock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      f2b_message = handle_get(f2b_message);
      pthread_mutex_unlock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      break;
    case 2:
      pthread_mutex_lock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      f2b_message = handle_put(f2b_message);
      pthread_mutex_unlock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      break;
    case 3:
      pthread_mutex_lock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      f2b_message = handle_delete(f2b_message);
      pthread_mutex_unlock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      break;
    case 4:
      pthread_mutex_lock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      f2b_message = handle_cput(f2b_message);
      pthread_mutex_unlock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      break;
    default:
      cout << "Unknown command type received" << endl;
      break;
    }

    // Encode response message
    string serialized = encode_message(f2b_message);
    // Send response to client
    bytes_sent = send(client_fd, serialized.c_str(), serialized.length(), 0);
    // Check for send errors
    if (bytes_sent < 0) {
      cerr << "[" << client_fd << "] Error in send(). Exiting" << endl;
      break;
    }

    // Print sent message if in verbose mode
    if (verbose) {
      cout << "[" << client_fd << "] S: " << serialized;
    }
  }

  // Remove client file descriptor from the list
  auto it = find(client_fds.begin(), client_fds.end(), client_fd);
  if (it != client_fds.end()) {
    client_fds.erase(it);
  }

  // Close client socket
  close(client_fd);

  // Print connection closed message if in verbose mode
  if (verbose) {
    cout << "[" << client_fd << "] Connection closed!\n";
  }
  return nullptr;
}

/**
 * Reads data from the client socket into the buffer.
 *
 * @param client_fd The file descriptor of the client socket.
 * @param client_buf The buffer to store the received data.
 * @return bool True if reading is successful, false otherwise.
 */
bool do_read(int client_fd, char *client_buf) {
  size_t n = MAX_BUFFER_SIZE;
  size_t bytes_left = n;
  bool r_arrived = false;

  while (bytes_left > 0) {
    ssize_t result = read(client_fd, client_buf + n - bytes_left, 1);

    if (result == -1) {
      // Handle read errors
      if ((errno == EINTR) || (errno == EAGAIN)) {
        continue; // Retry if interrupted or non-blocking operation
      }
      return false; // Return false for other errors
    } else if (result == 0) {
      return false; // Return false if connection closed by client
    }

    // Check if \r\n sequence has arrived
    if (r_arrived && client_buf[n - bytes_left] == '\n') {
      client_buf[n - bytes_left + 1] = '\0'; // Null-terminate the string
      break;                                 // Exit the loop
    } else {
      r_arrived = false;
    }

    // Check if \r has arrived
    if (client_buf[n - bytes_left] == '\r') {
      r_arrived = true;
    }

    bytes_left -= result; // Update bytes_left counter
  }

  client_buf[MAX_BUFFER_SIZE - 1] = '\0'; // Null-terminate the string
  return true; // Return true indicating successful reading
}