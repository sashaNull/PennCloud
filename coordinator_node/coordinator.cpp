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
using namespace std;

// Global variables

#define HOST "127.0.0.1"
#define PORT 7070

string config_file_location;
bool verbose;
int listen_fd;

// void *handle_heartbeat(void *arg)
// {
//     ifstream(config_file_location);
//     sockaddr_in server_sockaddr;
// }

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
    while ((option = getopt(argc, argv, "vo:")) != -1)
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
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt)) < 0)
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

    // Add a sigint handler
    signal(SIGINT, exit_handler);
    // Call the heartbeat thread. Assume that a function called handle_heartbeat exists.

    // Make a while loop that listens to new requests from the frontend servers and calls a thread for each connection.

    close(listen_fd);
    return EXIT_SUCCESS;
}