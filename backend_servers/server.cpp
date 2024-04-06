/*
1. Server reads the config file and reads arguments: -v --> verbose, serverconfig file, serverIndex, datafolder location.

2. It listens to TCP connections on the port

3. On a connection, it creates a new thread.

4. Thread reads the command and retrieves the key value pair and sends back a message.

*/

// Imports
#include <fstream>
#include <string>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sstream>

#include "../utils/utils.h"
using namespace std;

// Global Varibles
string server_ip;
int server_port;
string data_file_location;
bool verbose = false;
int server_index;
int listen_fd;

F_2_B_Message process_message(const string &serialized)
{
    F_2_B_Message message;
    istringstream iss(serialized);
    string token;

    getline(iss, token, '|');
    message.type = stoi(token);

    getline(iss, message.rowkey, '|');
    getline(iss, message.colkey, '|');
    getline(iss, message.value, '|');
    getline(iss, message.value2, '|');

    getline(iss, token, '|');
    message.status = stoi(token);

    // errorMessage might contain '|' characters, but since it's the last field,
    // we use the remainder of the string.
    getline(iss, message.errorMessage);

    return message;
}

// Function to handle a connection.
void *handle_connection(void *arg)
{
    int client_fd = *static_cast<int *>(arg);
    delete static_cast<int *>(arg);

    const int buffer_size = 1024;
    char buffer[buffer_size];
    string response = "WELCOME TO THE SERVER";
    ssize_t bytes_sent = send(client_fd, response.c_str(), response.length(), 0);
    if (bytes_sent < 0)
    {
        cerr << "Error in send(). Exiting" << endl;
        return nullptr;
    }

    while (true)
    {
        memset(buffer, 0, buffer_size);
        ssize_t bytes_received = recv(client_fd, buffer, buffer_size - 1, 0);
        if (bytes_received <= 0)
        {
            cerr << "Error in recv(). Exiting" << endl;
            break;
        }

        string message(buffer);
        if (message == "quit\n")
        {
            cout << "Quit command received. Closing connection." << endl;
            break;
        }

        cout << "Received message: " << message << endl;

        F_2_B_Message f2b_message = process_message(message);
        if (verbose)
        {
            cout << "Message details:" << endl;
            cout << "Type: " << f2b_message.type << endl;
            cout << "Rowkey: " << f2b_message.rowkey << endl;
            cout << "Colkey: " << f2b_message.colkey << endl;
            cout << "Value: " << f2b_message.value << endl;
            cout << "Value2: " << f2b_message.value2 << endl;
            cout << "Status: " << f2b_message.status << endl;
            cout << "ErrorMessage: " << f2b_message.errorMessage << endl;
        }

        // Process the message based on type
        switch (f2b_message.type)
        {
        case 1: // get
            // handle get
            break;
        case 2: // put
            // handle put
            break;
        case 3: // delete
            // handle delete
            break;
        case 4: // cput
            // handle cput
            break;
        default:
            cout << "Unknown command type received" << endl;
            break;
        }

        response = "ECHO: " + message;
        bytes_sent = send(client_fd, response.c_str(), response.length(), 0);
        if (bytes_sent < 0)
        {
            cerr << "Error in send(). Exiting" << endl;
            break;
        }
    }

    cout << "Closing connection" << endl;
    close(client_fd);
    return nullptr;
}

sockaddr_in parse_address(char *raw_line)
{
    sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;

    char *token = strtok(raw_line, ":");
    server_ip = string(token);
    inet_pton(AF_INET, token, &addr.sin_addr);

    token = strtok(NULL, ",");
    server_port = atoi(token);
    addr.sin_port = htons(atoi(token));

    data_file_location = strtok(NULL, "\n");
    return addr;
}

sockaddr_in parse_config_file(string config_file)
{
    ifstream config_stream(config_file);
    sockaddr_in server_sockaddr;
    int i = 0;
    string line;
    while (getline(config_stream, line))
    {
        if (i == server_index)
        {
            char raw_line[line.length() + 1];
            strcpy(raw_line, line.c_str());

            server_sockaddr = parse_address(raw_line);
        }
        i++;
    }
    return server_sockaddr;
}

void exit_handler(int sig)
{
    cout << "SIGINT received, shutting down." << endl;
    if (listen_fd >= 0)
    {
        close(listen_fd);
    }
    exit(EXIT_SUCCESS);
}

// Main Function
int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        cerr << "*** PennCloud: T15" << endl;
        exit(EXIT_FAILURE);
    }

    int option;
    // parsing using getopt()
    while ((option = getopt(argc, argv, "vo:")) != -1)
    {
        switch (option)
        {
        case 'v':
            verbose = true;
            break;
        default:
            // Print usage example for incorrect command-line options
            cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>"
                 << endl;
            exit(EXIT_FAILURE);
        }
    }
    // get config file
    if (optind == argc)
    {
        cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>"
             << endl;
        exit(EXIT_FAILURE);
    }
    string config_file = argv[optind];

    optind++;
    if (optind == argc)
    {
        cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>"
             << endl;
        exit(EXIT_FAILURE);
    }

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

    server_index = atoi(argv[optind]);
    sockaddr_in server_sockaddr = parse_config_file(config_file);
    if (verbose)
    {
        cout << "IP: " << server_ip << ":" << server_port << endl;
        cout << "Data Loc:" << data_file_location << endl;
        cout << "Server Index: " << server_index << endl;
        cout << server_sockaddr.sin_addr.s_addr << endl;
        cout << ntohs(server_sockaddr.sin_port) << endl;
    }

    if (bind(listen_fd, (struct sockaddr *)&server_sockaddr, sizeof(server_sockaddr)) != 0)
    {
        cerr << "Socket binding failed.\n"
             << endl;
        close(listen_fd);
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, exit_handler);

    if (listen(listen_fd, SOMAXCONN) != 0)
    {
        cerr << "Socket listening failed.\n"
             << endl;
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    while (true)
    {
        sockaddr_in client_sockaddr;
        socklen_t client_socklen = sizeof(client_sockaddr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_sockaddr, &client_socklen);

        if (client_fd < 0)
        {
            if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                // Retry if interrupted or non-blocking operation would block
                continue;
            }
            cerr << "Failed to accept new connection: " << strerror(errno)
                 << endl;
            break; // Break the loop on other errors
        }

        pthread_t thd;
        pthread_create(&thd, nullptr, handle_connection, new int(client_fd));
        pthread_detach(thd);
    }

    close(listen_fd);
    return EXIT_SUCCESS;
}
