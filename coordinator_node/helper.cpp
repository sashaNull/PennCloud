#include "helper.h"
using namespace std;

void print_server_details(const std::vector<server_info *> &list_of_all_servers,
                          const std::unordered_map<std::string, std::vector<server_info *>> &range_to_server_map)
{
    std::cout << "Listing all servers:" << std::endl;
    for (server_info *server : list_of_all_servers)
    {
        std::cout << "Server IP: " << server->ip << ", Port: " << server->port
                  << ", Active: " << (server->is_active ? "Yes" : "No") << std::endl;
    }

    std::cout << "\nListing range to server mapping:" << std::endl;
    for (const auto &pair : range_to_server_map)
    {
        std::cout << "Range: " << pair.first << " is handled by the following servers:" << std::endl;
        for (server_info *server : pair.second)
        {
            std::cout << "  - IP: " << server->ip << ", Port: " << server->port
                      << ", Active: " << (server->is_active ? "Yes" : "No") << std::endl;
        }
    }
}

void populate_list_of_servers(const std::string &config_file_location,
                              std::vector<server_info *> &list_of_all_servers,
                              std::unordered_map<std::string, std::vector<server_info *>> &range_to_server_map)
{
    std::ifstream config_file(config_file_location);
    std::string line;

    if (!config_file.is_open())
    {
        std::cerr << "Failed to open config file at: " << config_file_location << std::endl;
        return;
    }

    while (std::getline(config_file, line))
    {
        std::stringstream ss(line);
        std::string server_details, dummy, range;
        getline(ss, server_details, ','); // Get the server details part before the first comma

        // Parse IP and port
        size_t colon_pos = server_details.find(':');
        if (colon_pos == std::string::npos)
        {
            std::cerr << "Invalid server detail format: " << server_details << std::endl;
            continue;
        }
        std::string ip = server_details.substr(0, colon_pos);
        int port = std::stoi(server_details.substr(colon_pos + 1));

        // Create new server_info struct
        server_info *server = new server_info{ip, port, true};
        list_of_all_servers.push_back(server);

        getline(ss, dummy, ',');

        // Parse ranges and update range_to_server_map
        while (getline(ss, range, ','))
        {
            if (range.empty())
                continue;
            range_to_server_map[range].push_back(server);
        }
    }

    config_file.close();
    print_server_details(list_of_all_servers, range_to_server_map);
}

bool do_read(int client_fd, char *client_buf)
{
    size_t n = MAX_BUFFER_SIZE;
    size_t bytes_left = n;
    bool r_arrived = false;

    while (bytes_left > 0)
    {
        ssize_t result = read(client_fd, client_buf + n - bytes_left, 1);

        if (result == -1)
        {
            // Handle read errors
            if ((errno == EINTR) || (errno == EAGAIN))
            {
                continue; // Retry if interrupted or non-blocking operation
            }
            return false; // Return false for other errors
        }
        else if (result == 0)
        {
            return false; // Return false if connection closed by client
        }

        // Check if \r\n sequence has arrived
        if (r_arrived && client_buf[n - bytes_left] == '\n')
        {
            client_buf[n - bytes_left + 1] = '\0'; // Null-terminate the string
            break;                                 // Exit the loop
        }
        else
        {
            r_arrived = false;
        }

        // Check if \r has arrived
        if (client_buf[n - bytes_left] == '\r')
        {
            r_arrived = true;
        }

        bytes_left -= result; // Update bytes_left counter
    }

    client_buf[MAX_BUFFER_SIZE - 1] = '\0'; // Null-terminate the string
    return true;                            // Return true indicating successful reading
}

std::string get_range_from_rowname(const std::string &rowname, const std::unordered_map<std::string, std::vector<server_info *>> &range_to_server_map)
{
    if (rowname.empty())
    {
        throw std::invalid_argument("Rowname cannot be empty.");
    }

    char first_char = tolower(rowname[0]);

    for (const auto &range_pair : range_to_server_map)
    {
        // Extract range start and end from the key assuming the format "x_y"
        size_t dash_pos = range_pair.first.find('_');
        if (dash_pos == std::string::npos || dash_pos + 1 >= range_pair.first.size())
        {
            continue; // Skip malformed entries
        }

        char range_start = tolower(range_pair.first[0]);
        char range_end = tolower(range_pair.first[dash_pos + 1]);

        if (first_char >= range_start && first_char <= range_end)
        {
            return range_pair.first;
        }
    }

    // No matching range found
    return "";
}

std::string get_active_server_from_range(const std::unordered_map<std::string, std::vector<server_info *>> &range_to_server_map, const std::string &range, const std::string &type, std::unordered_map<std::string, server_info *> &range_to_primary_map)
{
    auto it = range_to_server_map.find(range);
    if (it == range_to_server_map.end() || it->second.empty())
    {
        std::cerr << "No servers available for the range: " << range << std::endl;
        return "";
    }

    std::vector<server_info *> active_servers;
    for (server_info *server : it->second)
    {
        if (server->is_active)
        {
            active_servers.push_back(server);
        }
    }

    if (active_servers.empty())
    {
        std::cerr << "No active servers available for the range: " << range << std::endl;
        return "";
    }

    if (type == "get")
    {
        // Randomly select an active server if type is "get"
        srand(time(NULL)); // Initialize random seed
        size_t index = rand() % active_servers.size();
        server_info *selected_server = active_servers[index];
        return selected_server->ip + ":" + std::to_string(selected_server->port);
    }
    else
    {
        // Return the primary server for other types
        server_info *primary_server = range_to_primary_map[range];
        if (primary_server && primary_server->is_active)
        {
            return primary_server->ip + ":" + std::to_string(primary_server->port);
        }
        else
        {
            // If primary is not active, return an error or a random active server as fallback
            std::cerr << "Primary server for range " << range << " is not active. Providing a random active server." << std::endl;
            srand(time(NULL)); // Initialize random seed
            size_t index = rand() % active_servers.size();
            server_info *selected_server = active_servers[index];
            return selected_server->ip + ":" + std::to_string(selected_server->port);
        }
    }
}

void print_primaries(unordered_map<string, server_info *> &range_to_primary_map)
{
    std::cout << "Listing primary servers for each range:" << std::endl;
    for (const auto &entry : range_to_primary_map)
    {
        const std::string &range = entry.first;
        server_info *primary_server = entry.second;
        if (primary_server)
        { // Check if the primary server pointer is not null
            std::cout << "Range: " << range
                      << " -> Primary Server: " << primary_server->ip
                      << ":" << primary_server->port
                      << ", Active: " << (primary_server->is_active ? "Yes" : "No") << std::endl;
        }
        else
        {
            std::cout << "Range: " << range << " -> No primary server assigned." << std::endl;
        }
    }
}

server_info *get_random_server_for_range(const string &range, unordered_map<string, vector<server_info *>> range_to_server_map)
{
    const auto it = range_to_server_map.find(range);
    if (it != range_to_server_map.end() && !it->second.empty())
    {
        vector<server_info *> active_servers;
        for (server_info *server : it->second)
        {
            if (server->is_active)
                active_servers.push_back(server);
        }

        if (!active_servers.empty())
        {
            srand(time(NULL)); // Initialize random seed
            size_t index = rand() % active_servers.size();
            return active_servers[index];
        }
    }
    return nullptr; // Return nullptr if no active server is found
}

void update_primary(unordered_map<string, server_info *> &range_to_primary_map, unordered_map<string, vector<server_info *>> range_to_server_map, pthread_mutex_t &map_and_list_mutex)
{
    pthread_mutex_lock(&map_and_list_mutex);
    for (auto &entry : range_to_primary_map)
    {
        const string &range = entry.first;
        server_info *current_primary = entry.second;

        if (!current_primary->is_active)
        {
            server_info *new_primary = get_random_server_for_range(range, range_to_server_map);
            if (new_primary)
            {
                entry.second = new_primary;
                cout << "Updated primary for range " << range << " to server " << new_primary->ip << ":" << new_primary->port << endl;
            }
            else
            {
                cerr << "No active servers available for range " << range << endl;
            }
        }
    }
    pthread_mutex_unlock(&map_and_list_mutex);
}

void initialize_primaries(unordered_map<string, server_info *> &range_to_primary_map, unordered_map<string, vector<server_info *>> range_to_server_map, pthread_mutex_t &map_and_list_mutex)
{
    pthread_mutex_lock(&map_and_list_mutex); // Ensure thread safety

    range_to_primary_map.clear(); // Clear existing entries to avoid duplicates

    for (const auto &entry : range_to_server_map)
    {
        const std::string &range = entry.first;
        const std::vector<server_info *> &servers = entry.second;

        if (!servers.empty())
        {
            range_to_primary_map[range] = servers[0]; // Set the first server as primary
        }
        else
        {
            range_to_primary_map[range] = nullptr; // Set nullptr if no servers are available
        }
    }

    pthread_mutex_unlock(&map_and_list_mutex);
}
