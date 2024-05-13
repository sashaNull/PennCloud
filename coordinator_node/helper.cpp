#include "helper.h"
using namespace std;

/**
 * Reads data from the client socket until a specific sequence is encountered.
 *
 * @param client_fd The file descriptor of the client socket.
 * @param client_buf The buffer to store the read data.
 * @return bool Returns true if successful reading, false otherwise.
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

/**
 * Retrieves the range associated with the given rowname from a range-to-server
 * mapping.
 *
 * @param rowname The rowname for which to find the associated range.
 * @param range_to_server_map The mapping of range to server information.
 * @return std::string The range associated with the given rowname, or an empty
 * string if not found.
 *
 * This function searches for the range in the range_to_server_map that
 * corresponds to the given rowname. It assumes that the rowname starts with a
 * character, and the ranges in the map are represented as "x_y", where 'x' and
 * 'y' are characters indicating the start and end of the range, respectively.
 */
std::string get_range_from_rowname(
    const std::string &rowname,
    const std::unordered_map<std::string, std::vector<server_info *>>
        &range_to_server_map) {
  if (rowname.empty()) {
    throw std::invalid_argument("Rowname cannot be empty.");
  }

  char first_char = tolower(rowname[0]);

  for (const auto &range_pair : range_to_server_map) {
    // Extract range start and end from the key assuming the format "x_y"
    size_t dash_pos = range_pair.first.find('_');
    if (dash_pos == std::string::npos ||
        dash_pos + 1 >= range_pair.first.size()) {
      continue; // Skip malformed entries
    }

    char range_start = tolower(range_pair.first[0]);
    char range_end = tolower(range_pair.first[dash_pos + 1]);

    if (first_char >= range_start && first_char <= range_end) {
      return range_pair.first;
    }
  }

  // No matching range found
  return "";
}

/**
 * Retrieves an active server from the specified range based on the type of
 * operation.
 *
 * @param range_to_server_map The mapping of range to server information.
 * @param range The range for which to retrieve an active server.
 * @param type The type of operation (e.g., "get").
 * @param range_to_primary_map The mapping of range to primary server
 * information.
 * @return std::string The IP address and port of the selected active server, or
 * an empty string if no server is available.
 *
 * This function searches for an active server within the specified range based
 * on the operation type. If the operation type is "get", it randomly selects an
 * active server from the available ones. For other operation types, it returns
 * the primary server for the range if it is active. If the primary server is
 * not active, it either returns an error or selects a random active server as a
 * fallback.
 */
std::string get_active_server_from_range(
    const std::unordered_map<std::string, std::vector<server_info *>>
        &range_to_server_map,
    const std::string &range, const std::string &type,
    std::unordered_map<std::string, server_info *> &range_to_primary_map) {
  auto it = range_to_server_map.find(range);
  if (it == range_to_server_map.end() || it->second.empty()) {
    std::cerr << "No servers available for the range: " << range << std::endl;
    return "";
  }

  std::vector<server_info *> active_servers;
  for (server_info *server : it->second) {
    if (server->is_active) {
      active_servers.push_back(server);
    }
  }

  if (active_servers.empty()) {
    std::cerr << "No active servers available for the range: " << range
              << std::endl;
    return "";
  }

  if (type == "get") {
    // Randomly select an active server if type is "get"
    srand(time(NULL)); // Initialize random seed
    size_t index = rand() % active_servers.size();
    server_info *selected_server = active_servers[index];
    return selected_server->ip + ":" + std::to_string(selected_server->port);
  } else {
    // Return the primary server for other types
    server_info *primary_server = range_to_primary_map[range];
    if (primary_server && primary_server->is_active) {
      return primary_server->ip + ":" + std::to_string(primary_server->port);
    } else {
      // If primary is not active, return an error or a random active server as
      // fallback
      std::cerr << "Primary server for range " << range
                << " is not active. Providing a random active server."
                << std::endl;
      srand(time(NULL)); // Initialize random seed
      size_t index = rand() % active_servers.size();
      server_info *selected_server = active_servers[index];
      return selected_server->ip + ":" + std::to_string(selected_server->port);
    }
  }
}

/**
 * Retrieves a random active server from the specified range.
 *
 * @param range The range for which to retrieve a random active server.
 * @param range_to_server_map The mapping of range to server information.
 * @return server_info* A pointer to the selected active server, or nullptr if
 * no active server is found.
 *
 * This function searches for a random active server within the specified range.
 * It returns nullptr if no active server is found for the range.
 */
server_info *get_random_server_for_range(
    const string &range,
    unordered_map<string, vector<server_info *>> range_to_server_map) {
  const auto it = range_to_server_map.find(range);
  if (it != range_to_server_map.end() && !it->second.empty()) {
    vector<server_info *> active_servers;
    for (server_info *server : it->second) {
      if (server->is_active)
        active_servers.push_back(server);
    }

    if (!active_servers.empty()) {
      srand(time(NULL)); // Initialize random seed
      size_t index = rand() % active_servers.size();
      return active_servers[index];
    }
  }
  return nullptr; // Return nullptr if no active server is found
}

/**
 * Prints details of all servers and the mapping of ranges to servers.
 *
 * @param list_of_all_servers A vector containing pointers to all servers.
 * @param range_to_server_map The mapping of range to server information.
 * @return void
 *
 * This function prints the details of all servers, including their IP address, port, and activity status.
 * It also prints the mapping of ranges to servers, indicating which servers handle each range.
 */
void print_server_details(
    const std::vector<server_info *> &list_of_all_servers,
    const std::unordered_map<std::string, std::vector<server_info *>>
        &range_to_server_map) {
  // Print details of all servers
  std::cout << "Listing all servers:" << std::endl;
  for (server_info *server : list_of_all_servers) {
    std::cout << "Server IP: " << server->ip << ", Port: " << server->port
              << ", Active: " << (server->is_active ? "Yes" : "No")
              << std::endl;
  }

  // Print mapping of ranges to servers
  std::cout << "\nListing range to server mapping:" << std::endl;
  for (const auto &pair : range_to_server_map) {
    std::cout << "Range: " << pair.first
              << " is handled by the following servers:" << std::endl;
    for (server_info *server : pair.second) {
      std::cout << "  - IP: " << server->ip << ", Port: " << server->port
                << ", Active: " << (server->is_active ? "Yes" : "No")
                << std::endl;
    }
  }
}

/**
 * Populates the list of all servers and the mapping of ranges to servers from a configuration file.
 *
 * @param config_file_location The location of the configuration file.
 * @param list_of_all_servers A vector to store pointers to all servers.
 * @param range_to_server_map The mapping of range to server information.
 * @return void
 *
 * This function reads server details and associated ranges from the configuration file.
 * It creates server_info structures for each server and updates both the list of all servers
 * and the mapping of ranges to servers accordingly.
 */
void populate_list_of_servers(
    const std::string &config_file_location,
    std::vector<server_info *> &list_of_all_servers,
    std::unordered_map<std::string, std::vector<server_info *>>
        &range_to_server_map) {
  std::ifstream config_file(config_file_location);
  std::string line;

  if (!config_file.is_open()) {
    std::cerr << "Failed to open config file at: " << config_file_location
              << std::endl;
    return;
  }

  while (std::getline(config_file, line)) {
    std::stringstream ss(line);
    std::string server_details, dummy, range;
    getline(ss, server_details,
            ','); // Get the server details part before the first comma

    // Parse IP and port
    size_t colon_pos = server_details.find(':');
    if (colon_pos == std::string::npos) {
      std::cerr << "Invalid server detail format: " << server_details
                << std::endl;
      continue;
    }
    std::string ip = server_details.substr(0, colon_pos);
    int port = std::stoi(server_details.substr(colon_pos + 1));

    // Create new server_info struct
    server_info *server = new server_info{ip, port, true};
    list_of_all_servers.push_back(server);

    getline(ss, dummy, ',');

    // Parse ranges and update range_to_server_map
    while (getline(ss, range, ',')) {
      if (range.empty())
        continue;
      range_to_server_map[range].push_back(server);
    }
  }

  config_file.close();
  print_server_details(list_of_all_servers, range_to_server_map);
}

/**
 * Prints details of primary servers assigned to each range.
 *
 * @param range_to_primary_map The mapping of range to primary server information.
 * @return void
 *
 * This function prints the details of primary servers assigned to each range.
 * It indicates the IP address, port, and activity status of the primary server for each range.
 * If no primary server is assigned for a range, it indicates that as well.
 */
void print_primaries(
    unordered_map<string, server_info *> &range_to_primary_map) {
  std::cout << "Listing primary servers for each range:" << std::endl;
  for (const auto &entry : range_to_primary_map) {
    const std::string &range = entry.first;
    server_info *primary_server = entry.second;
    if (primary_server) { // Check if the primary server pointer is not null
      std::cout << "Range: " << range
                << " -> Primary Server: " << primary_server->ip << ":"
                << primary_server->port
                << ", Active: " << (primary_server->is_active ? "Yes" : "No")
                << std::endl;
    } else {
      std::cout << "Range: " << range << " -> No primary server assigned."
                << std::endl;
    }
  }
}

/**
 * Updates the primary server for each range if the current primary is inactive.
 *
 * @param range_to_primary_map The mapping of range to primary server information.
 * @param range_to_server_map The mapping of range to server information.
 * @param map_and_list_mutex The mutex to ensure thread safety while updating maps and lists.
 * @return void
 *
 * This function updates the primary server for each range if the current primary server is inactive.
 * It acquires a mutex lock to ensure thread safety while accessing and modifying the maps and lists.
 * If an active server is found for a range, it updates the primary server and prints the update message.
 * If no active server is available for a range, it prints an error message.
 */
void update_primary(
    unordered_map<string, server_info *> &range_to_primary_map,
    unordered_map<string, vector<server_info *>> range_to_server_map,
    pthread_mutex_t &map_and_list_mutex) {
  pthread_mutex_lock(&map_and_list_mutex);
  for (auto &entry : range_to_primary_map) {
    const string &range = entry.first;
    server_info *current_primary = entry.second;

    if (!current_primary->is_active) {
      server_info *new_primary =
          get_random_server_for_range(range, range_to_server_map);
      if (new_primary) {
        entry.second = new_primary;
        cout << "Updated primary for range " << range << " to server "
             << new_primary->ip << ":" << new_primary->port << endl;
      } else {
        cerr << "No active servers available for range " << range << endl;
      }
    }
  }
  pthread_mutex_unlock(&map_and_list_mutex);
}

/**
 * Initializes primary servers for each range based on the first server in the server list.
 *
 * @param range_to_primary_map The mapping of range to primary server information.
 * @param range_to_server_map The mapping of range to server information.
 * @param map_and_list_mutex The mutex to ensure thread safety while updating maps and lists.
 * @return void
 *
 * This function initializes primary servers for each range based on the first server in the server list.
 * It acquires a mutex lock to ensure thread safety while accessing and modifying the maps.
 * Existing entries in the range_to_primary_map are cleared to avoid duplicates.
 * If servers are available for a range, the first server in the list is set as the primary server.
 * If no servers are available for a range, nullptr is set as the primary server.
 */
void initialize_primaries(
    unordered_map<string, server_info *> &range_to_primary_map,
    unordered_map<string, vector<server_info *>> range_to_server_map,
    pthread_mutex_t &map_and_list_mutex) {
  pthread_mutex_lock(&map_and_list_mutex); // Ensure thread safety

  range_to_primary_map.clear(); // Clear existing entries to avoid duplicates

  for (const auto &entry : range_to_server_map) {
    const std::string &range = entry.first;
    const std::vector<server_info *> &servers = entry.second;

    if (!servers.empty()) {
      range_to_primary_map[range] =
          servers[0]; // Set the first server as primary
    } else {
      range_to_primary_map[range] =
          nullptr; // Set nullptr if no servers are available
    }
  }

  pthread_mutex_unlock(&map_and_list_mutex);
}
