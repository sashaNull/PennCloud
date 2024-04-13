// Includes necessary header files for socket programming, file manipulation,
// and threading

// Includes custom utility functions
// #include "../utils/utils.h"
#include "utils.h"

// Namespace declaration for convenience
using namespace std;

// Global variables for server configuration and state
map<string, pthread_mutex_t> file_lock_map{}; // Map to store file locks
vector<int> client_fds{};                     // All client file descriptors
string server_ip;                             // Server IP address
string data_file_location;                    // Location of data files
int server_port;                              // Port on which server listens for connections
int server_index;                             // Index of the server
int listen_fd;                                // File descriptor for the listening socket
bool verbose = false;                         // Verbosity flag for debugging

struct tablet_cache_struct
{
  string tablet_name;
  map<string, map<string, string>> kv_map;
} tablet_cache;

map<string, fileRange> prefix_to_file;

// Function prototypes for parsing and initializing server configuration
sockaddr_in parse_address(char *raw_line);
sockaddr_in parse_config_file(string config_file);

// Signal handler function for clean exit
void exit_handler(int sig);

// Function for initializing file locks
void initialize_file_lock();

// Function prototype for handling client connections in separate threads
void *handle_connection(void *arg);

void load_tablet_cache(string row_key);

void save_cache();

void save_cache()
{
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
  while ((option = getopt(argc, argv, "vo:")) != -1)
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
  if (verbose)
  {
    cout << "Server IP: " << server_ip << ":" << server_port << endl;
    cout << "Server Port: " << ntohs(server_sockaddr.sin_port) << endl;
    cout << "Data Loc:" << data_file_location << endl;
    cout << "Server Index: " << server_index << endl;
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

  // Initialize file lock for handling file operations
  initialize_file_lock();

  // Register signal handler for clean exit
  signal(SIGINT, exit_handler);

  // Create map from filenames
  createPrefixToFileMap(data_file_location, prefix_to_file);

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
sockaddr_in parse_address(char *raw_line)
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
    // Check if current line corresponds to the server index
    if (i == server_index)
    {
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

/**
 * Initializes file locks for all files in the data directory.
 *
 * This function creates a pthread_mutex_t for each file in the data directory
 * and stores it in the file_lock_map for later use.
 */
void initialize_file_lock()
{
  // Open the data directory
  DIR *dir = opendir(data_file_location.c_str());
  if (dir)
  {
    struct dirent *entry;
    // Iterate over each entry in the directory
    while ((entry = readdir(dir)) != nullptr)
    {
      string file_name = entry->d_name;
      // Skip "." and ".." entries
      if (file_name != "." && file_name != "..")
      {
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
    cout << "This row is in file: " << findFileNameInRange(prefix_to_file, f2b_message.rowkey) << endl;

    // Add the message to the LOG
    log_message(f2b_message, data_file_location);
    // Handle message based on its type
    switch (f2b_message.type)
    {
    case 1:
      pthread_mutex_lock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      f2b_message = handle_get(f2b_message, data_file_location);
      pthread_mutex_unlock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      break;
    case 2:
      pthread_mutex_lock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      f2b_message = handle_put(f2b_message, data_file_location);
      pthread_mutex_unlock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      break;
    case 3:
      pthread_mutex_lock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      f2b_message = handle_delete(f2b_message, data_file_location);
      pthread_mutex_unlock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      break;
    case 4:
      pthread_mutex_lock(&file_lock_map[f2b_message.rowkey + ".txt"]);
      f2b_message = handle_cput(f2b_message, data_file_location);
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

/*
TODO: function that does checkpointing.
This function should block access to the cache.
Save cache into a file for the checkpointing.
Clear the log.
Release lock to the cache.
*/

/*
TODO: a function that recovers from failure.
This function should run on startup.
Load cache from checkpoint file,  and replay any logged entries.
*/
