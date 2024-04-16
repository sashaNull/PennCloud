// Includes necessary header files for socket programming, file manipulation,
// and threading

// Includes custom utility functions
// #include "../utils/utils.h"
#include "utils.h"

// Namespace declaration for convenience
using namespace std;

#define CHECKPOINT_SIZE 20
vector<int> client_fds{};  // All client file descriptors
string server_ip;          // Server IP address
string data_file_location; // Location of data files
int server_port;           // Port on which server listens for connections
int server_index;          // Index of the server
int listen_fd;             // File descriptor for the listening socket
bool verbose = false;      // Verbosity flag for debugging

unordered_map<string, tablet_data> cache;

// Function prototypes for parsing and initializing server configuration
sockaddr_in parse_address(char *raw_line);
sockaddr_in parse_config_file(string config_file);

// Signal handler function for clean exit
void exit_handler(int sig);

// Function prototype for handling client connections in separate threads
void *handle_connection(void *arg);

void load_cache();

void save_cache();

void load_cache()
{
  for (auto &entry : cache)
  {
    ifstream file(data_file_location + "/" + entry.first);
    string line;

    while (getline(file, line))
    {
      stringstream ss(line);
      string key, inner_key, value;

      ss >> key >> inner_key >> value;
      entry.second.row_to_kv[key][inner_key] = value;
    }

    file.close();
  }
}

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

void printPrefixToFileMap(
    const std::map<std::string, fileRange> &prefix_to_file)
{
  for (const auto &entry : prefix_to_file)
  {
    std::cout << "Prefix: " << entry.first << std::endl;
    std::cout << "  Range Start: " << entry.second.range_start << std::endl;
    std::cout << "  Range End: " << entry.second.range_end << std::endl;
    std::cout << "  Filename: " << entry.second.filename << std::endl;
  }
}

string get_file_name(string row_key)
{
  if (row_key.at(0) >= 'a' && row_key.at(0) <= 'c')
  {
    return "a_c.txt";
  }
  else if (row_key.at(0) >= 'd' && row_key.at(0) <= 'f')
  {
    return "d_f.txt";
  }
  else if (row_key.at(0) >= 'g' && row_key.at(0) <= 'i')
  {
    return "g_i.txt";
  }
  else if (row_key.at(0) >= 'j' && row_key.at(0) <= 'l')
  {
    return "j_l.txt";
  }
  else if (row_key.at(0) >= 'm' && row_key.at(0) <= 'o')
  {
    return "m_o.txt";
  }
  else if (row_key.at(0) >= 'p' && row_key.at(0) <= 'r')
  {
    return "p_r.txt";
  }
  else if (row_key.at(0) >= 's' && row_key.at(0) <= 'u')
  {
    return "s_u.txt";
  }
  else if (row_key.at(0) >= 's' && row_key.at(0) <= 'u')
  {
    return "v_x.txt";
  }
  else
  {
    return "y_z.txt";
  }
}

/**
 * Handles the GET operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message to be processed.
 * @return F_2_B_Message The processed F_2_B_Message containing the retrieved
 * value or error message.
 */
F_2_B_Message handle_get(F_2_B_Message message, string tablet_name)
{
  if (cache[tablet_name].row_to_kv.contains(message.rowkey))
  {
    if (cache[tablet_name].row_to_kv[message.rowkey].contains(message.colkey))
    {
      message.value =
          cache[tablet_name].row_to_kv[message.rowkey][message.colkey];
      message.status = 0;
      message.errorMessage.clear();
    }
    else
    {
      message.status = 1;
      message.errorMessage = "Colkey does not exist";
    }
  }
  else
  {
    message.status = 1;
    message.errorMessage = "Rowkey does not exist";
  }

  return message;
}

/**
 * Handles the PUT operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message containing data to be written.
 * @return F_2_B_Message The processed F_2_B_Message containing status and error
 * message.
 */
F_2_B_Message handle_put(F_2_B_Message message, string tablet_name)
{
  std::cout << tablet_name << " " << message.rowkey << " " << message.colkey << " " << message.value;
  cache[tablet_name].row_to_kv[message.rowkey][message.colkey] = message.value;
  message.status = 0;
  message.errorMessage = "Data written successfully";
  return message;
}

/**
 * Handles the CPUT operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message containing data to be updated.
 * @return F_2_B_Message The processed F_2_B_Message containing status and error
 * message.
 */
F_2_B_Message handle_cput(F_2_B_Message message, string tablet_name)
{
  if (cache[tablet_name].row_to_kv.contains(message.rowkey))
  {
    if (cache[tablet_name].row_to_kv[message.rowkey].contains(message.colkey))
    {
      if (cache[tablet_name].row_to_kv[message.rowkey][message.colkey] ==
          message.value)
      {
        cache[tablet_name].row_to_kv[message.rowkey][message.colkey] =
            message.value2;
        message.status = 0;
        message.errorMessage = "Colkey updated successfully";
      }
      else
      {
        message.status = 1;
        message.errorMessage = "Current value is not v1";
      }
    }
    else
    {
      message.status = 1;
      message.errorMessage = "Colkey does not exist";
    }
  }
  else
  {
    message.status = 1;
    message.errorMessage = "Rowkey does not exist";
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
F_2_B_Message handle_delete(F_2_B_Message message, string tablet_name)
{
  if (cache[tablet_name].row_to_kv.contains(message.rowkey))
  {
    if (cache[tablet_name].row_to_kv[message.rowkey].erase(message.colkey))
    {
      message.status = 0;
      message.errorMessage = "Colkey deleted successfully";
    }
    else
    {
      message.status = 1;
      message.errorMessage = "Colkey does not exist";
    }
  }
  else
  {
    message.status = 1;
    message.errorMessage = "Rowkey does not exist";
  }

  // Return processed message
  return message;
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

  // Initialize cache for handling file operations
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
        tablet_data new_tablet;
        cache[file_name] = new_tablet;
      }
    }
    // Close the directory
    closedir(dir);
  }

  // Load data to cache
  load_cache();

  // Register signal handler for clean exit
  signal(SIGINT, exit_handler);

  // Create map from filenames
  // createPrefixToFileMap(data_file_location, prefix_to_file);
  // printPrefixToFileMap(prefix_to_file);

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

    string tablet_name = get_file_name(f2b_message.rowkey);
    cout << "This row is in file: " << tablet_name << endl;

    // Handle message based on its type
    switch (f2b_message.type)
    {
    case 1:
      f2b_message = handle_get(f2b_message, tablet_name);
      break;
    case 2:
      // Add the message to the LOG
      pthread_mutex_lock(&cache[tablet_name].tablet_lock);
      log_message(f2b_message, data_file_location, tablet_name);
      f2b_message = handle_put(f2b_message, tablet_name);
      cache[tablet_name].requests_since_checkpoint++;

      pthread_mutex_unlock(&cache[tablet_name].tablet_lock);
      break;
    case 3:
      // Add the message to the LOG
      pthread_mutex_lock(&cache[tablet_name].tablet_lock);
      log_message(f2b_message, data_file_location, tablet_name);
      f2b_message = handle_delete(f2b_message, tablet_name);
      cache[tablet_name].requests_since_checkpoint++;
      pthread_mutex_unlock(&cache[tablet_name].tablet_lock);
      break;
    case 4:
      // Add the message to the LOG
      pthread_mutex_lock(&cache[tablet_name].tablet_lock);
      log_message(f2b_message, data_file_location, tablet_name);
      f2b_message = handle_cput(f2b_message, tablet_name);
      cache[tablet_name].requests_since_checkpoint++;
      pthread_mutex_unlock(&cache[tablet_name].tablet_lock);
      break;
    default:
      cout << "Unknown command type received" << endl;
      break;
    }
    if (cache[tablet_name].requests_since_checkpoint > CHECKPOINT_SIZE)
    {
      cout << "Needs Checkpoint: " << tablet_name << " "
           << cache[tablet_name].requests_since_checkpoint;
      pthread_mutex_lock(&cache[tablet_name].tablet_lock);
      checkpoint_tablet(cache[tablet_name], tablet_name, data_file_location);
      pthread_mutex_unlock(&cache[tablet_name].tablet_lock);
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
