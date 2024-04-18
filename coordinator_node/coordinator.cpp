// API for frontend to the coordinator
/*

From Frontend: GET rowname type

From Coordinator:
+OK RESP 127.0.0.1:port
-ERR No server for this range
-ERR Incorrect Command

*/

#include "helper.h"
using namespace std;

string config_file_location;
bool verbose;
int listen_fd;

// Contains the mapping of which range of rownames is in what servers
unordered_map<string, vector<server_info *>> range_to_server_map;
pthread_mutex_t map_and_list_mutex;

// Vector containing one struct per server in the config file.
vector<server_info *> list_of_all_servers;

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

    populate_list_of_servers(config_file_location, list_of_all_servers, range_to_server_map);

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
            char type[MAX_BUFFER_SIZE];
            memset(row_key, 0, MAX_BUFFER_SIZE);
            memset(type, 0, MAX_BUFFER_SIZE);
            row_key[MAX_BUFFER_SIZE - 1] = '\0';
            type[MAX_BUFFER_SIZE - 1] = '\0';
            if (sscanf(buffer, "GET %1023s %1023s\r\n", row_key, type) != -1)
            {
                string response;
                if (type[0] == '\0')
                {
                    response = "-ERR type not specified";
                }
                else
                {
                    string reqType(type);
                    string range = get_range_from_rowname(string(row_key));
                    pthread_mutex_lock(&map_and_list_mutex);
                    string server_with_range = get_active_server_from_range(range_to_server_map, range, reqType);
                    pthread_mutex_unlock(&map_and_list_mutex);

                    if (server_with_range == "")
                    {
                        response = "-ERR No server for this range\r\n";
                    }
                    else
                    {
                        response = "+OK RESP " + server_with_range + "\r\n";
                    }
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