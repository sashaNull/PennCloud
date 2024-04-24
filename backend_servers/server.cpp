// Includes necessary header files for socket programming, file manipulation,
// and threading

// Includes custom utility functions
// #include "../utils/utils.h"
#include "utils.h"
#include <cmath>

// Namespace declaration for convenience
using namespace std;

vector<int> client_fds{};  // All client file descriptors
string server_ip;          // Server IP address
string data_file_location; // Location of data files
int server_port;           // Port on which server listens for connections
int server_index;          // Index of the server
int listen_fd;             // File descriptor for the listening socket
bool verbose = false;      // Verbosity flag for debugging

unordered_map<string, vector<sockaddr_in>> tablet_ranges_to_other_addr{};
unordered_map<string, tablet_data> cache;
vector<string> server_tablet_ranges;
vector<string> all_unique_tablet_ranges;

// Function prototypes for parsing and initializing server configuration
sockaddr_in parse_current_address(char *raw_line);
sockaddr_in parse_config_file(string config_file);

// Signal handler function for clean exit
void exit_handler(int sig);

// Function prototype for handling client connections in separate threads
void *handle_connection(void *arg);

void save_cache()
{
  for (const auto &entry : cache)
  {
    ofstream file(data_file_location + "/" + entry.first);

    for (const auto &row : entry.second.row_to_kv)
    {
      for (const auto &inner_entry : row.second)
      {
        file << row.first << " " << inner_entry.first << " "
             << inner_entry.second << "\n";
      }
    }
    file.close();
  }
}

void initialize_cache(std::unordered_map<std::string, tablet_data> &cache)
{
  // Iterate over each tablet range provided in the server_tablet_ranges
  for (const std::string &tablet_range : server_tablet_ranges)
  {
    // Initialize a new tablet data instance for each range
    tablet_data new_tablet;

    // Insert the new tablet data into the cache using the tablet range as the key
    cache[tablet_range] = new_tablet;
  }
}

int main(int argc, char *argv[])
{
  // Check if there are enough command-line arguments
  if (argc == 1)
  {
    cerr << "*** PennCloud: T15" << endl;
    exit(EXIT_FAILURE);
  }

  int option;
  // Parse command-line options
  while ((option = getopt(argc, argv, "v")) != -1)
  {
    switch (option)
    {
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
  if (optind == argc)
  {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }

  // Extract configuration file name
  string config_file = argv[optind];

  // Extract server index
  optind++;
  if (optind == argc)
  {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }

  // Create a socket
  listen_fd = socket(PF_INET, SOCK_STREAM, 0);

  if (listen_fd == -1)
  {
    cerr << "Socket creation failed.\n"
         << endl;
    exit(EXIT_FAILURE);
  }

  // Set socket options
  int opt = 1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt)) < 0)
  {
    cerr << "Setting socket option failed.\n";
    close(listen_fd);
    exit(EXIT_FAILURE);
  }

  // Parse configuration file and extract server address and port
  server_index = atoi(argv[optind]);
  sockaddr_in server_sockaddr = parse_config_file(config_file);

  set<string> unique_ranges_set{};

  for (const auto s : server_tablet_ranges)
  {
    unique_ranges_set.insert(s);
  }

  for (const auto e : tablet_ranges_to_other_addr)
  {
    unique_ranges_set.insert(e.first);
  }

  all_unique_tablet_ranges = vector<string>(unique_ranges_set.begin(), unique_ranges_set.end());

  update_server_tablet_ranges(server_tablet_ranges);
  if (verbose)
  {
    cout << "Server IP: " << server_ip << ":" << server_port << endl;
    cout << "Server Port: " << ntohs(server_sockaddr.sin_port) << endl;
    cout << "Data Loc:" << data_file_location << endl;
    cout << "Server Index: " << server_index << endl;
    cout << "All Unique Ranges:\n";
    for (const auto &range : all_unique_tablet_ranges)
    {
      cout << range << "\n";
    }
    cout << "Server Tablet Ranges:\n";
    for (const auto &range : server_tablet_ranges)
    {
      cout << range << "\n";
    }
  }

  // Bind the socket to the server address
  if (bind(listen_fd, (struct sockaddr *)&server_sockaddr,
           sizeof(server_sockaddr)) != 0)
  {
    cerr << "Socket binding failed.\n"
         << endl;
    close(listen_fd);
    exit(EXIT_FAILURE);
  }

  // Start listening for incoming connections
  if (listen(listen_fd, SOMAXCONN) != 0)
  {
    cerr << "Socket listening failed.\n"
         << endl;
    close(listen_fd);
    exit(EXIT_FAILURE);
  }

  // INIT the cache.
  initialize_cache(cache);

  // Load data to cache
  load_cache(cache, data_file_location);

  // TODO: Get the latest tablet and log files from the primary

  // Reload data to cache
  load_cache(cache, data_file_location);

  // Perform Recovery
  recover(cache, data_file_location, server_tablet_ranges);

  // Register signal handler for clean exit
  signal(SIGINT, exit_handler);

  // Accept and handle incoming connections
  while (true)
  {
    sockaddr_in client_sockaddr;
    socklen_t client_socklen = sizeof(client_sockaddr);
    int client_fd =
        accept(listen_fd, (struct sockaddr *)&client_sockaddr, &client_socklen);

    if (client_fd < 0)
    {
      // If accept fails due to certain errors, continue accepting
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
      {
        continue;
      }
      // Otherwise, print error and exit loop
      cerr << "Failed to accept new connection: " << strerror(errno) << endl;
      break;
    }

    // Push into the list of all client file descriptors
    client_fds.push_back(client_fd);

    if (verbose)
    {
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
 * Parses the raw address information from a string and returns a sockaddr_in
 * structure.
 *
 * @param raw_line The raw address information in the format
 * "ip_address:port_number,data_file_location".
 * @return sockaddr_in The parsed sockaddr_in structure representing the
 * address.
 */
sockaddr_in parse_current_address(char *raw_line)
{
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
      strtok(NULL, ","); // Assuming `data_file_location` is a global variable

  server_tablet_ranges.clear();

  // Parse tablet ranges and add to the global vector
  while ((token = strtok(NULL, ",")) != nullptr)
  {
    server_tablet_ranges.push_back(string(token));
  }

  return addr;
}

void parse_other_addresses(char *raw_line)
{
  sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;

  // Parse IP address
  char *token = strtok(raw_line, ":");
  inet_pton(AF_INET, token, &addr.sin_addr);

  // Parse port number
  token = strtok(NULL, ",");
  addr.sin_port = htons(atoi(token));

  // Parse data file location
  strtok(NULL, ",");

  while ((token = strtok(NULL, ",")) != nullptr)
  {
    tablet_ranges_to_other_addr[string(token)].push_back(addr);
  }
}

/**
 * Parses the configuration file to extract server address information.
 *
 * @param config_file The path to the configuration file.
 * @return sockaddr_in The sockaddr_in structure representing the server
 * address.
 */
sockaddr_in parse_config_file(string config_file)
{
  // Open configuration file for reading
  ifstream config_stream(config_file);
  // Initialize sockaddr_in structure
  sockaddr_in server_sockaddr;
  // Initialize index counter
  int i = 0;
  string line;
  // Read each line of the configuration file
  while (getline(config_stream, line))
  {
    // Convert string line to C-style string
    char raw_line[line.length() + 1];
    strcpy(raw_line, line.c_str());
    // Check if current line corresponds to the server index
    if (i == server_index)
    {
      // Parse address from the line
      server_sockaddr = parse_current_address(raw_line);
    }
    else
    {
      parse_other_addresses(raw_line);
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
void exit_handler(int sig)
{
  // Prepare shutdown message
  string shutdown_message = "Server shutting down!\r\n";
  // Iterate through client file descriptors
  for (const auto &client_fd : client_fds)
  {
    // Set socket to non-blocking mode
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags != -1)
    {
      flags |= O_NONBLOCK;
      if (fcntl(client_fd, F_SETFL, flags) == -1)
      {
        perror("fcntl");
      }
    }
    // Send shutdown message to client
    ssize_t bytes_sent =
        send(client_fd, shutdown_message.c_str(), shutdown_message.length(), 0);
    // Display shutdown message if in verbose mode
    if (verbose)
    {
      cerr << "[" << client_fd << "] S: " << shutdown_message;
      cout << "[" << client_fd << "] Connection closed\n";
    }
    // Close client socket
    close(client_fd);
  }

  // Close listening socket if it is open
  if (listen_fd >= 0)
  {
    close(listen_fd);
  }
  // Exit the server process with success status
  exit(EXIT_SUCCESS);
}

string get_tablet_range_from_row_key(string row_key)
{
  for (const string s : all_unique_tablet_ranges)
  {
    if (row_key.at(0) >= s.at(0) && row_key.at(0) <= s.back())
    {
      return s;
    }
  }
  return "-ERR";
}

/**
 * Function to handle client connections.
 *
 * @param arg Pointer to the client file descriptor.
 * @return nullptr
 */
void *handle_connection(void *arg)
{
  // Extract client file descriptor from argument
  int client_fd = *static_cast<int *>(arg);
  delete static_cast<int *>(arg); // Delete memory allocated for the argument

  // Send welcome message to client
  string response = "WELCOME TO THE SERVER\r\n";
  ssize_t bytes_sent = send(client_fd, response.c_str(), response.length(), 0);
  // Check for send errors
  if (bytes_sent < 0)
  {
    cerr << "[" << client_fd << "] Error in send(). Exiting" << endl;
    return nullptr;
  }

  char buffer[MAX_BUFFER_SIZE];
  // Continue reading client messages until quit command received
  while (do_read(client_fd, buffer))
  {
    string message(buffer);
    // Print received message if in verbose mode
    if (verbose)
    {
      cout << "[" << client_fd << "] C: " << message;
    }

    // Check for quit command
    if (message == "quit\r\n")
    {
      string goodbye = "Quit command received. Server goodbye!\r\n";
      bytes_sent = send(client_fd, goodbye.c_str(), goodbye.length(), 0);
      // Check for send errors
      if (bytes_sent < 0)
      {
        cerr << "[" << client_fd << "] Error in send(). Exiting" << endl;
        break;
      }

      // Print goodbye message if in verbose mode
      if (verbose)
      {
        cout << "[" << client_fd << "] S: " << goodbye;
      }
      break;
    }

    // Decode received message into F_2_B_Message
    F_2_B_Message f2b_message = decode_message(message);
    F_2_B_Message f2b_message_for_other_server = f2b_message;

    string tablet_name = get_new_file_name(f2b_message.rowkey, server_tablet_ranges);
    cout << "This row is in new file: " << tablet_name << endl;

    // Handle message based on its type
    switch (f2b_message.type)
    {
    case 1:
      pthread_mutex_lock(&cache[tablet_name].tablet_lock);
      f2b_message = handle_get(f2b_message, tablet_name, cache);
      pthread_mutex_unlock(&cache[tablet_name].tablet_lock);
      break;
    case 2:
      // Add the message to the LOG
      pthread_mutex_lock(&cache[tablet_name].tablet_lock);
      log_message(f2b_message, data_file_location, tablet_name);
      f2b_message = handle_put(f2b_message, tablet_name, cache);
      cache[tablet_name].requests_since_checkpoint++;
      pthread_mutex_unlock(&cache[tablet_name].tablet_lock);
      break;
    case 3:
      // Add the message to the LOG
      pthread_mutex_lock(&cache[tablet_name].tablet_lock);
      log_message(f2b_message, data_file_location, tablet_name);
      f2b_message = handle_delete(f2b_message, tablet_name, cache);
      cache[tablet_name].requests_since_checkpoint++;
      pthread_mutex_unlock(&cache[tablet_name].tablet_lock);
      break;
    case 4:
      // Add the message to the LOG
      pthread_mutex_lock(&cache[tablet_name].tablet_lock);
      log_message(f2b_message, data_file_location, tablet_name);
      f2b_message = handle_cput(f2b_message, tablet_name, cache);
      cache[tablet_name].requests_since_checkpoint++;
      pthread_mutex_unlock(&cache[tablet_name].tablet_lock);
      break;
    default:
      cout << "Unknown command type received" << endl;
      break;
    }
    if (cache[tablet_name].requests_since_checkpoint > CHECKPOINT_SIZE)
    {
      cout << "Checkpointing the file: " << tablet_name << " "
           << cache[tablet_name].requests_since_checkpoint << endl;
      pthread_mutex_lock(&cache[tablet_name].tablet_lock);
      checkpoint_tablet(cache[tablet_name], tablet_name, data_file_location);
      cache[tablet_name].requests_since_checkpoint = 0;
      pthread_mutex_unlock(&cache[tablet_name].tablet_lock);
    }

    if (f2b_message_for_other_server.isFromBackend == 0 && f2b_message_for_other_server.type != 1)
    {
      f2b_message_for_other_server.isFromBackend = 1;
      string tablet_range = get_tablet_range_from_row_key(f2b_message_for_other_server.rowkey);
      for (auto other_addr : tablet_ranges_to_other_addr[tablet_range])
      {
        string serialized_to_backend = encode_message(f2b_message_for_other_server);

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
          cerr << "Error in socket creation" << endl;
          break;
        }

        if (connect(sock, (struct sockaddr *)&other_addr, sizeof(other_addr)) < 0)
        {
          cerr << "Connection Failed" << endl;
          break;
        }

        bytes_sent = send(sock, serialized_to_backend.c_str(), serialized_to_backend.length(), 0);
        if (bytes_sent < 0)
        {
          cerr << "Error in send(). Exiting" << endl;
          break;
        }

        if (verbose)
        {
          cout << "[" << client_fd << ", " << sock << "] S: " << serialized_to_backend;
        }

        char buffer[1024] = {0};
        size_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0)
        {
          cout << "Does not receive response from other servers." << endl;
          break;
        }
        else
        {
          if (verbose)
          {
            cout << "[" << client_fd << ", " << sock << "] S: "
                 << "Received Response" << endl;
          }
        }
      }
    }

    // Encode response message
    string serialized = encode_message(f2b_message);
    // Send response to client
    bytes_sent = send(client_fd, serialized.c_str(), serialized.length(), 0);
    // Check for send errors
    if (bytes_sent < 0)
    {
      cerr << "[" << client_fd << "] Error in send(). Exiting" << endl;
      break;
    }

    // Print sent message if in verbose mode
    if (verbose)
    {
      cout << "[" << client_fd << "] S: " << serialized;
    }
  }

  // Remove client file descriptor from the list
  auto it = find(client_fds.begin(), client_fds.end(), client_fd);
  if (it != client_fds.end())
  {
    client_fds.erase(it);
  }

  // Close client socket
  close(client_fd);

  // Print connection closed message if in verbose mode
  if (verbose)
  {
    cout << "[" << client_fd << "] Connection closed!\n";
  }
  return nullptr;
}
