#include <iostream>
#include <string>
#include <stdio.h>
#include <unistd.h>
#include <fstream>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <vector>
using namespace std;

// Global variables

#define HOST "127.0.0.1"
#define PORT 7070

const int MAX_BUFFER_SIZE = 1024;

string config_file_location;
bool verbose;
int listen_fd;

struct server_info
{
    string ip;
    int port;
    bool is_active;
};

/*
1. Funciton that converts row to range.
2. Main loop that listens to new request and returns the server.
*/

// Contains the mapping of which range of rownames is in what servers
unordered_map<string, vector<server_info *>> range_to_server_map;
pthread_mutex_t map_and_list_mutex;

// Vector containing one struct per server in the config file.
vector<server_info *> list_of_all_servers;

void print_server_details()
{
    cout << "Listing all servers:" << endl;
    for (server_info *server : list_of_all_servers)
    {
        cout << "Server IP: " << server->ip << ", Port: " << server->port
             << ", Active: " << (server->is_active ? "Yes" : "No") << endl;
    }

    cout << "\nListing range to server mapping:" << endl;
    for (const auto &pair : range_to_server_map)
    {
        cout << "Range: " << pair.first << " is handled by the following servers:" << endl;
        for (server_info *server : pair.second)
        {
            cout << "  - IP: " << server->ip << ", Port: " << server->port
                 << ", Active: " << (server->is_active ? "Yes" : "No") << endl;
        }
    }
}

void populate_list_of_servers()
{
    ifstream config_file(config_file_location);
    string line;

    if (!config_file.is_open())
    {
        cerr << "Failed to open config file at: " << config_file_location << endl;
        return;
    }

    while (getline(config_file, line))
    {
        stringstream ss(line);
        string server_details, dummy, range;
        getline(ss, server_details, ','); // Get the server details part before the first comma

        // Parse IP and port
        size_t colon_pos = server_details.find(':');
        if (colon_pos == string::npos)
        {
            cerr << "Invalid server detail format: " << server_details << endl;
            continue;
        }
        string ip = server_details.substr(0, colon_pos);
        int port = stoi(server_details.substr(colon_pos + 1));

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
    print_server_details();
}

void *handle_heartbeat(void *arg)
{
    while (true)
    {
        for (server_info *server : list_of_all_servers)
        {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0)
            {
                cerr << "Error: Unable to create socket." << endl;
                continue;
            }
            struct sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(server->port);
            server_addr.sin_addr.s_addr = inet_addr(server->ip.c_str());
            if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
            {
                cerr << "Server " << server->ip << ":" << server->port << " is down." << endl;
                server->is_active = false;
            }
            else
            {
                cout << "Server " << server->ip << ":" << server->port << " is up." << endl;
                server->is_active = true;
                close(sock);
            }
        }
        // Wait for 5 seconds before next check
        sleep(5);
    }
    return NULL;
}

void exit_handler(int sig)
{
    // Prepare shutdown message
    string shutdown_message = "Server shutting down!\r\n";

    // Close listening socket if it is open
    if (listen_fd >= 0)
    {
        close(listen_fd);
    }
    // Exit the server process with success status
    exit(EXIT_SUCCESS);
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

string get_range_from_rowname(const string &rowname)
{
    if (rowname.empty())
    {
        throw std::invalid_argument("Rowname cannot be empty.");
    }
    char first_char = tolower(rowname[0]);
    if (first_char >= 'a' && first_char <= 'e')
    {
        return "a_e";
    }
    else if (first_char >= 'f' && first_char <= 'j')
    {
        return "f_j";
    }
    else if (first_char >= 'k' && first_char <= 'o')
    {
        return "k_o";
    }
    else if (first_char >= 'p' && first_char <= 't')
    {
        return "p_t";
    }
    else if (first_char >= 'u' && first_char <= 'z')
    {
        return "u_z";
    }
    return "";
}

string get_active_server_from_range(const string &range)
{
    if (range_to_server_map.find(range) == range_to_server_map.end() || range_to_server_map[range].empty())
    {
        cerr << "No servers available for the range: " << range << endl;
        return "";
    }
    vector<server_info *> &servers = range_to_server_map[range];
    vector<server_info *> active_servers;
    for (server_info *server : servers)
    {
        if (server->is_active)
        {
            active_servers.push_back(server);
        }
    }

    if (active_servers.empty())
    {
        cerr << "No active servers available for the range: " << range << endl;
        return "";
    }
    srand(time(NULL));
    size_t index = rand() % active_servers.size();

    server_info *selected_server = active_servers[index];
    return selected_server->ip + ":" + to_string(selected_server->port);
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        cerr << "*** PennCloud: T15" << endl;
        exit(EXIT_FAILURE);
    }

    // Read the parameters --> Includes the server config file location and store as a global variable.
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
            cerr << "Syntax: " << argv[0] << " -v <config_file_name>" << endl;
            exit(EXIT_FAILURE);
        }
    }
    // Ensure there are enough arguments after parsing options
    if (optind == argc)
    {
        cerr << "Syntax: " << argv[0] << " -v <config_file_name>" << endl;
        exit(EXIT_FAILURE);
    }
    // Extract configuration file name
    config_file_location = argv[optind];

    // Create the socket
    listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
        cerr << "Socket creation failed.\n"
             << endl;
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    {
        cerr << "Setting socket option failed.\n";
        close(listen_fd);
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_sockaddr;
    bzero(&server_sockaddr, sizeof(server_sockaddr));
    server_sockaddr.sin_family = AF_INET;
    inet_pton(AF_INET, HOST, &server_sockaddr.sin_addr);
    server_sockaddr.sin_port = htons(PORT);
    if (verbose)
    {
        cout << "Server IP: " << HOST << ":" << PORT << endl;
        cout << "Server Port: " << ntohs(server_sockaddr.sin_port) << endl;
        cout << "Config Location: " << config_file_location << endl;
    }
    if (bind(listen_fd, (struct sockaddr *)&server_sockaddr,
             sizeof(server_sockaddr)) != 0)
    {
        cerr << "Socket binding failed.\n"
             << endl;
        close(listen_fd);
        exit(EXIT_FAILURE);
    }
    if (listen(listen_fd, SOMAXCONN) != 0)
    {
        cerr << "Socket listening failed.\n"
             << endl;
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    populate_list_of_servers();

    pthread_mutex_init(&map_and_list_mutex, NULL);

    // Add a sigint handler
    signal(SIGINT, exit_handler);
    // Call the heartbeat thread. Assume that a function called handle_heartbeat exists.
    pthread_t heartbeat_thread;
    pthread_create(&heartbeat_thread, nullptr, handle_heartbeat, nullptr);
    pthread_detach(heartbeat_thread);

    // Make a while loop that listens to new requests from the frontend servers and calls a thread for each connection.

    char buffer[MAX_BUFFER_SIZE];
    while (true)
    {
        sockaddr_in client_sockaddr;
        socklen_t client_socklen = sizeof(client_sockaddr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_sockaddr, &client_socklen);

        if (client_fd < 0)
        {
            if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                continue;
            }
            cerr << "Failed to accept new connection: " << strerror(errno) << endl;
            break;
        }

        if (verbose)
        {
            cout << "[" << client_fd << "] New connection\n";
        }

        bzero(&buffer, sizeof(buffer));
        do_read(client_fd, buffer);

        if (strncmp(buffer, "GET ", 4) == 0)
        {
            char row_key[MAX_BUFFER_SIZE];
            row_key[MAX_BUFFER_SIZE - 1] = '\0';
            if (sscanf(buffer, "GET %1023s\r\n", row_key) != -1)
            {
                string range = get_range_from_rowname(string(row_key));
                pthread_mutex_lock(&map_and_list_mutex);
                string server_with_range = get_active_server_from_range(range);
                pthread_mutex_unlock(&map_and_list_mutex);
                string response;
                if (server_with_range == "")
                {
                    response = "-ERR No server for this range\r\n";
                }
                else
                {
                    response = "+OK RESP " + server_with_range + "\r\n";
                }
                size_t n = send(client_fd, response.c_str(), response.length(), 0);
                if (n < 0)
                {
                    cerr << "[" << client_fd << "] Error in send(). Exiting" << endl;
                    break;
                }
            }
            else
            {
                // Command param not implemented
                string message = "-ERR Command parameter not implemented\r\n";
                size_t n = send(client_fd, message.c_str(), message.length(), 0);
                if (n < 0)
                {
                    cerr << "[" << client_fd << "] Error in send(). Exiting" << endl;
                    break;
                }
            }
        }
        else
        {
            // Incorrect command
            string message = "-ERR Command not recognized\r\n";
            size_t n = send(client_fd, message.c_str(), message.length(), 0);
            if (n < 0)
            {
                cerr << "[" << client_fd << "] Error in send(). Exiting" << endl;
                break;
            }
        }

        close(client_fd);
    }

    close(listen_fd);
    return EXIT_SUCCESS;
}

// API for frontend to the coordinator
/*

From Frontend: GET rowname

From Coordinator:
+OK RESP 127.0.0.1:port
-ERR No server for this range
-ERR Incorrect Command

*/