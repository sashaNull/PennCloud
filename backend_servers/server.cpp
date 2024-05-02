// Includes necessary header files for socket programming, file manipulation,
// and threading

// Includes custom utility functions
// #include "../utils/utils.h"
#include "utils.h"
#include <cmath>

// Namespace declaration for convenience
using namespace std;
#define COORDINATOR_PORT 7070

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
unordered_map<string, string> range_to_primary_map;

bool suspended = false;                                    // Global variable to control suspension
pthread_mutex_t suspend_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for suspended variable

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

void update_primary(const string &range)
{
  int sock;
  struct sockaddr_in serv_addr;
  char buffer[1024] = {0};

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    cerr << "Socket creation error" << endl;
    return;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(COORDINATOR_PORT);
  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
  {
    cerr << "Invalid address/ Address not supported" << endl;
    return;
  }

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    cerr << "Connection Failed" << endl;
    return;
  }

  string request = "PGET " + range + "\r\n";
  send(sock, request.c_str(), request.length(), 0);
  read(sock, buffer, 1024);
  string response(buffer);

  // Parse response
  if (response.substr(0, 3) == "+OK")
  {
    while (!response.empty() && (response.back() == '\n' || response.back() == '\r'))
    {
      response.pop_back();
    }
    range_to_primary_map[range] = response.substr(4);
  }
  else
  {
    range_to_primary_map[range] = "No primary available";
  }

  close(sock);
}

void get_latest_tablet_and_log()
{
  for (const auto &range : server_tablet_ranges)
  {
    update_primary(range);
    auto it = range_to_primary_map.find(range);
    if (it != range_to_primary_map.end() && it->second != "No primary available" && it->second != server_ip + ":" + to_string(server_port))
    {
      string primary = it->second;
      size_t colon_pos = primary.find(':');
      string primary_ip = primary.substr(0, colon_pos);
      int primary_port = stoi(primary.substr(colon_pos + 1));

      int sock;
      struct sockaddr_in serv_addr;
      if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      {
        cerr << "Socket creation error" << endl;
        continue;
      }

      serv_addr.sin_family = AF_INET;
      serv_addr.sin_port = htons(primary_port);
      if (inet_pton(AF_INET, primary_ip.c_str(), &serv_addr.sin_addr) <= 0)
      {
        cerr << "Invalid address/ Address not supported" << endl;
        close(sock);
        continue;
      }

      if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
      {
        cerr << "Connection Failed with " << primary_ip << ":" << primary_port << endl;
        close(sock);
        continue;
      }

      // Read and ignore the welcome message from the server
      char welcome_buffer[1024] = {0};
      read(sock, welcome_buffer, 1024);
      // cout << "Ignored message: " << welcome_buffer << endl; // Optionally log the ignored message
      bool runLogGet = false;
      // Send GET command
      string get_command = "GET " + range + "\r\n";
      send(sock, get_command.c_str(), get_command.length(), 0);
      char buffer[1024] = {0};
      read(sock, buffer, 1024);
      cout << "GET Response from primary: " << range << " " << buffer << endl;

      // Check version response and possibly send TABGET
      string response(buffer);
      string response_prefix = "VER ";
      size_t ver_pos = response.find(response_prefix);
      if (ver_pos != string::npos)
      {
        int received_version = stoi(response.substr(ver_pos + response_prefix.length()));
        if (received_version != cache[range].tablet_version)
        {
          runLogGet = true;
          string tabget_command = "TABGET " + range + "\r\n";
          send(sock, tabget_command.c_str(), tabget_command.length(), 0);

          string new_file = data_file_location + "/" + range + "_" + to_string(received_version) + ".txt";
          ofstream out(new_file);
          string accumulatedData; // To store incomplete data that may not end with a newline
          bool reading = true;

          while (reading)
          {
            char buffer[1024] = {0};
            int bytesRead = read(sock, buffer, sizeof(buffer) - 1);
            if (bytesRead <= 0)
            {
              reading = false; // Connection closed or error
              continue;
            }

            accumulatedData.append(buffer, bytesRead);
            size_t pos;
            // Process complete lines
            while ((pos = accumulatedData.find('\n')) != string::npos)
            {
              string line = accumulatedData.substr(0, pos + 1); // Include the newline
              accumulatedData.erase(0, pos + 1);
              if (line.find("##########\n") != string::npos)
              {
                cout << "End-of-data marker received, stopping read." << endl;
                reading = false;
                break;
              }
              out << line; // Write the complete line to file
              cout << "READ THE LINE: " << line;
            }
          }
          if (!accumulatedData.empty())
          {
            out << accumulatedData; // Write any remaining data that didn't end with a newline
          }
          out.close();

          // Delete old version file
          string old_file = data_file_location + "/" + range + "_" + to_string(cache[range].tablet_version) + ".txt";
          cache[range].tablet_version = received_version;
          remove(old_file.c_str());
        }
      }

      // Send LGET command
      string lget_command = "LGET " + range + "\r\n";
      send(sock, lget_command.c_str(), lget_command.length(), 0);
      memset(buffer, 0, sizeof(buffer));
      read(sock, buffer, 1024);
      cout << "LGET Response from primary: " << range << " " << buffer << endl;

      // Check requests_since_checkpoint response and possibly send LOGGET
      string lget_response(buffer);
      size_t checkpoint_pos = lget_response.find("VER ");
      if (checkpoint_pos != string::npos)
      {
        int received_checkpoint = stoi(lget_response.substr(checkpoint_pos + 4));
        if (received_checkpoint != cache[range].requests_since_checkpoint || runLogGet)
        {

          string logget_command = "LOGGET " + range + "\r\n";
          send(sock, logget_command.c_str(), logget_command.length(), 0);

          string tempLogFile = data_file_location + "/logs/" + range + "_log_temp.txt";
          ofstream tempOut(tempLogFile);
          string accumulatedData;
          bool reading = true;
          while (reading)
          {
            char buffer[1024] = {0};
            int bytesRead = read(sock, buffer, sizeof(buffer) - 1);
            if (bytesRead <= 0)
            {
              reading = false;
              continue;
            }

            accumulatedData.append(buffer, bytesRead);
            size_t pos;
            while ((pos = accumulatedData.find('\n')) != string::npos)
            {
              string line = accumulatedData.substr(0, pos + 1);
              accumulatedData.erase(0, pos + 1);
              if (line.find("##########\n") != string::npos)
              {
                cout << "End-of-data marker received, stopping read." << endl;
                reading = false;
                break;
              }
              tempOut << line;
            }
          }
          if (!accumulatedData.empty())
          {
            tempOut << accumulatedData; // Write any remaining data that didn't end with a newline
          }
          tempOut.close();

          // Rename the temporary log file to the old log file location
          string oldLogFile = data_file_location + "/logs/" + range + "_logs.txt";
          remove(oldLogFile.c_str());                      // Remove the old file
          rename(tempLogFile.c_str(), oldLogFile.c_str()); // Rename temp to old
          cout << "Log file updated for " << range << endl;
        }
      }

      // Send quit command at the end of all interactions
      string quit_command = "quit\r\n";
      send(sock, quit_command.c_str(), quit_command.length(), 0);

      // Close the socket
      close(sock);
    }
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
  recover(cache, data_file_location, server_tablet_ranges);
  // cout << "PRINTING: " << cache["aa_am"].requests_since_checkpoint << endl;
  // cout << "PRINTING: " << cache["aa_am"].tablet_version << endl;

  // TODO: Get the latest tablet and log files from the primary

  // Get primary for every tablet.
  for (const auto &range : server_tablet_ranges)
  {
    update_primary(range);
  }
  if (verbose)
  {
    cout << "Range to Primary Map:" << endl;
    for (const auto &entry : range_to_primary_map)
    {
      cout << "Range: " << entry.first << " - Primary: " << entry.second << endl;
    }
  }
  get_latest_tablet_and_log();

  // Reload data to cache
  load_cache(cache, data_file_location);

  // Perform Recovery
  recover(cache, data_file_location, server_tablet_ranges);

  // cout << "PRINTING: " << cache["aa_am"].requests_since_checkpoint << endl;
  // cout << "PRINTING: " << cache["aa_am"].tablet_version << endl;

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
    // pthread_detach(thd);
  }

  // Close the listening socket and exit
  close(listen_fd);
  return EXIT_SUCCESS;
}

sockaddr_in parse_addr(char *raw_line)
{
  sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;

  char *token = strtok(raw_line, ":");
  server_ip = string(token); // Assuming `server_ip` is a global variable
  inet_pton(AF_INET, token, &addr.sin_addr);

  token = strtok(NULL, ":");
  server_port = atoi(token); // Assuming `server_port` is a global variable
  addr.sin_port = htons(atoi(token));
  return addr;
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

F_2_B_Message handle_list(int client_fd)
{
  for (auto &entry : cache)
  {
    const string &tablet_name = entry.first;
    tablet_data &data = entry.second;
    pthread_mutex_lock(&data.tablet_lock);
    for (auto &row_pair : data.row_to_kv)
    {
      for (auto &col_pair : row_pair.second)
      {
        cout << row_pair.first << " " << col_pair.first << " " << col_pair.second << endl;
        F_2_B_Message list_message;
        list_message.type = 10;
        list_message.rowkey = row_pair.first;
        list_message.colkey = col_pair.first;
        list_message.status = 0;
        list_message.value = col_pair.second;
        list_message.errorMessage = "";
        list_message.value2 = "";
        string serialized = encode_message(list_message);
        send(client_fd, serialized.c_str(), serialized.length(), 0);
        sleep(0.01);
      }
    }

    // Unlock the tablet
    pthread_mutex_unlock(&data.tablet_lock);
  }

  // Send a final success message
  F_2_B_Message success_message;
  success_message.status = 0; // Indicate success
  success_message.errorMessage = "success";
  success_message.rowkey = "terminate";
  success_message.colkey = "terminate";
  success_message.type = 10;
  return success_message;
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

  string lockedTablet = "";
  bool isLocked = false;
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
      if (isLocked)
      {
        pthread_mutex_unlock(&cache[lockedTablet].tablet_lock);
        isLocked = false;
      }
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
    if (message.substr(0, 4) == "GET " && !isLocked)
    {
      size_t endpos = message.find_last_not_of("\r\n");
      if (endpos != string::npos)
      {
        message = message.substr(0, endpos + 1);
      }
      lockedTablet = message.substr(4);
      pthread_mutex_lock(&cache[lockedTablet].tablet_lock);
      isLocked = true;

      int version_number = cache[lockedTablet].tablet_version;
      response = "VER " + to_string(version_number) + "\r\n";
      cout << lockedTablet << endl;
      cout
          << "GET Response:" << response << endl;
      send(client_fd, response.c_str(), response.length(), 0);
      continue;
    }
    else if (message.substr(0, 5) == "LGET " && isLocked)
    {
      size_t endpos = message.find_last_not_of("\r\n");
      if (endpos != string::npos)
      {
        message = message.substr(0, endpos + 1);
      }
      int requests_since_checkpoint = cache[lockedTablet].requests_since_checkpoint;
      response = "VER " + to_string(requests_since_checkpoint) + "\r\n";
      cout << "LGET Response:" << response << endl;
      send(client_fd, response.c_str(), response.length(), 0);
      continue;
    }
    else if (message.substr(0, 7) == "LOGGET " && isLocked)
    {
      size_t endpos = message.find_last_not_of("\r\n");
      if (endpos != string::npos)
      {
        message = message.substr(0, endpos + 1);
      }
      string logFilename = data_file_location + "/logs/" + lockedTablet + "_logs.txt"; // Assume log files are named like this
      ifstream logFile(logFilename);
      if (!logFile.is_open())
      {
        cerr << "Failed to open log file: " << logFilename << endl;
        send(client_fd, "##########\n", 11, 0); // Send end-of-data marker even if file fails to open
      }
      else
      {
        string line;
        while (getline(logFile, line))
        {
          line += "\n"; // Ensure each line ends with a newline for consistent transmission
          send(client_fd, line.c_str(), line.length(), 0);
        }
        logFile.close();
        send(client_fd, "##########\n", 11, 0); // Send the special end-of-data marker
      }
      continue;
    }
    else if (message.substr(0, 7) == "TABGET " && isLocked)
    {
      size_t endpos = message.find_last_not_of("\r\n");
      if (endpos != string::npos)
      {
        message = message.substr(0, endpos + 1);
      }

      string filename = data_file_location + "/" + lockedTablet + "_" + to_string(cache[lockedTablet].tablet_version) + ".txt";
      ifstream file(filename);
      if (!file.is_open())
      {
        cerr << "Failed to open file: " << filename << endl;
        send(client_fd, "##########\n", 11, 0);
      }
      else
      {
        string line;
        while (getline(file, line))
        {
          line += "\n";
          cout << "SENT LINE: " << line << endl;
          send(client_fd, line.c_str(), line.length(), 0);
        }
        file.close();
        send(client_fd, "##########\n", 11, 0);
      }
      continue;
    }

    // Decode received message into F_2_B_Message
    F_2_B_Message f2b_message = decode_message(message);
    F_2_B_Message f2b_message_for_other_server = f2b_message;

    if (suspended && f2b_message.type != 6)
    {
      f2b_message.status = 2;
      f2b_message.errorMessage = "Server Suspended";
      string serialized = encode_message(f2b_message);
      bytes_sent = send(client_fd, serialized.c_str(), serialized.length(), 0);
      if (bytes_sent < 0)
      {
        cerr << "[" << client_fd << "] Error in send(). Exiting" << endl;
        break;
      }
      if (verbose)
      {
        cout << "[" << client_fd << "] S: " << serialized;
      }
      break;
    }
    else if (suspended && f2b_message.type == 6)
    {
      pthread_mutex_lock(&suspend_mutex);
      get_latest_tablet_and_log();
      load_cache(cache, data_file_location);
      recover(cache, data_file_location, server_tablet_ranges);
      suspended = false;
      pthread_mutex_unlock(&suspend_mutex);
      f2b_message.status = 0;
      f2b_message.errorMessage = "Server back online";
      string serialized = encode_message(f2b_message);
      bytes_sent = send(client_fd, serialized.c_str(), serialized.length(), 0);
      if (bytes_sent < 0)
      {
        cerr << "[" << client_fd << "] Error in send(). Exiting" << endl;
        break;
      }
      if (verbose)
      {
        cout << "[" << client_fd << "] S: " << serialized;
      }

      break;
    }
    else if (f2b_message.type == 5)
    {
      pthread_mutex_lock(&suspend_mutex);
      suspended = true;
      pthread_mutex_unlock(&suspend_mutex);
      f2b_message.status = 0;
      f2b_message.errorMessage = "Server successfully Suspended";
      string serialized = encode_message(f2b_message);
      bytes_sent = send(client_fd, serialized.c_str(), serialized.length(), 0);
      if (bytes_sent < 0)
      {
        cerr << "[" << client_fd << "] Error in send(). Exiting" << endl;
        break;
      }
      if (verbose)
      {
        cout << "[" << client_fd << "] S: " << serialized;
      }
      break;
    }

    string tablet_name = get_new_file_name(f2b_message.rowkey, server_tablet_ranges);
    cout << "This row is in new file: " << tablet_name << endl;

    string primary_ip_port = range_to_primary_map[tablet_name];
    string curr_ip_port = server_ip + ":" + to_string(server_port);
    bool amIPrimary = primary_ip_port == curr_ip_port;

    // if req not from primary and I am not the primary and the type of req is not get
    // FOrward to primary
    // Wait for a response
    // Fotward response to sender
    // continue

    if (f2b_message_for_other_server.isFromPrimary == 0 && f2b_message_for_other_server.type != 1 && !amIPrimary && f2b_message_for_other_server.type != 10)
    {

      string serialized_to_primary = encode_message(f2b_message);

      int sock = socket(AF_INET, SOCK_STREAM, 0);
      if (sock < 0)
      {
        cerr << "Error in socket creation" << endl;
        break;
      }

      sockaddr_in primary_sockaddr = parse_addr(const_cast<char *>(primary_ip_port.c_str()));
      cout << "Primary Port:" << ntohs(primary_sockaddr.sin_port) << endl;

      if (connect(sock, (struct sockaddr *)&primary_sockaddr, sizeof(primary_sockaddr)) < 0)
      {
        cerr << "Connection Failed" << endl;
        break;
      }
      char welcome_buffer[1024] = {0};
      read(sock, welcome_buffer, 1024);
      cout << "Ignored message: " << welcome_buffer << endl; // Optionally log the ignored message
      bytes_sent = send(sock, serialized_to_primary.c_str(), serialized_to_primary.length(), 0);
      if (bytes_sent < 0)
      {
        cerr << "Error in send(). Exiting" << endl;
        break;
      }

      if (verbose)
      {
        cout << "[" << client_fd << ", " << sock << "] S: " << serialized_to_primary;
      }

      char buffer[1024] = {0};
      ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
      if (bytes_received <= 0)
      {
        cout << "Server closed the connection or error occurred." << endl;
        break;
      }

      string buffer_str(buffer);
      F_2_B_Message received_message{};
      cout << buffer_str << " HELoOOOOO" << endl;

      if (buffer_str.find('|') != string::npos)
      {
        cout << "\nServer: " << endl;
        F_2_B_Message received_message = decode_message(buffer_str);
      }

      string client_response = encode_message(received_message);
      bytes_sent = send(client_fd, client_response.c_str(), client_response.length(), 0);

      if (bytes_sent < 0)
      {
        cerr << "[" << client_fd << "] Error in send(). Exiting" << endl;
        break;
      }

      if (verbose)
      {
        cout << "[" << client_fd << "] S: " << client_response;
      }
      close(sock);
      continue;
    }

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
    case 10:
      f2b_message = handle_list(client_fd);
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

    if (f2b_message_for_other_server.isFromPrimary == 0 && f2b_message_for_other_server.type != 1 && amIPrimary && f2b_message_for_other_server.type != 10)
    {
      f2b_message_for_other_server.isFromPrimary = 1;
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
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
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
