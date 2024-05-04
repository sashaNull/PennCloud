#include "../../utils/utils.h"
#include <arpa/inet.h> // for inet_pton
#include <cstring>     // for strlen and memset
#include <iostream>
#include <netinet/in.h> // for sockaddr_in
#include <sstream>
#include <sys/select.h> // for select()
#include <sys/socket.h> // for socket functions
#include <unistd.h>     // for close

using namespace std;

// Function to connect to a server and return the socket descriptor
int connect_to_server(const char *server_ip, int server_port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        cerr << "Error in socket creation" << endl;
        return -1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        cerr << "Invalid address/ Address not supported" << endl;
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        cerr << "Connection Failed" << endl;
        return -1;
    }

    return sock;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cerr << "Usage: " << argv[0] << " <Coordinator IP> <Coordinator Port>" << endl;
        return 1;
    }

    const char *coordinator_ip = argv[1];
    int coordinator_port = stoi(argv[2]);

    // Main client loop
    bool runClient = true;
    while (runClient)
    {
        string input;
        cout << "> ";
        getline(cin, input);

        if (input == "quit")
        {
            cout << "Closing the connection" << endl;
            break;
        }

        // Parse the input
        istringstream iss(input);
        string command, rowkey, colkey, value, value2;
        iss >> command >> rowkey >> colkey;

        F_2_B_Message message;
        message.rowkey = rowkey;
        message.colkey = colkey;
        message.type = 0;
        message.status = 0;
        message.isFromPrimary = 0;

        if (command == "get")
        {
            message.type = 1;
        }
        else if (command == "put")
        {
            iss >> value;
            message.value = value;
            message.type = 2;
        }
        else if (command == "delete")
        {
            message.type = 3;
        }
        else if (command == "cput")
        {
            iss >> value >> value2;
            message.value = value;
            message.value2 = value2;
            message.type = 4;
        }
        else if (command == "sus")
        {
            message.type = 5;
        }
        else if (command == "rev")
        {
            message.type = 6;
        }
        else
        {
            cerr << "Unknown command" << endl;
            continue;
        }

        // Send PGET to the coordinator to get the primary server
        string get_primary_cmd = "PGET " + rowkey + "\r\n";
        int coordinator_sock = connect_to_server(coordinator_ip, coordinator_port);
        if (coordinator_sock < 0)
        {
            return 1; // Exit if connection to the coordinator failed
        }

        cout << "Connected to the coordinator. Type 'quit' to exit." << endl;
        send(coordinator_sock, get_primary_cmd.c_str(), get_primary_cmd.length(), 0);

        char buffer[1024] = {0};
        recv(coordinator_sock, buffer, sizeof(buffer) - 1, 0);
        close(coordinator_sock);
        string response(buffer);

        if (response.find("+OK") == string::npos)
        {
            cerr << "Failed to get primary server details: " << response << endl;
            continue;
        }
        // Parse the primary server details
        auto pos = response.find(' ');
        string primary_details = response.substr(pos + 1);
        pos = primary_details.find(':');
        string primary_ip = primary_details.substr(0, pos);
        int primary_port = stoi(primary_details.substr(pos + 1));

        cout << "SELECTED SERVER: " << primary_details << endl;

        // Connect to the primary server
        int server_sock = connect_to_server(primary_ip.c_str(), primary_port);
        if (server_sock < 0)
        {
            continue; // Skip this loop if connection to the primary server failed
        }

        // Serialize the message
        string serialized = encode_message(message);
        char welcome_buffer[1024] = {0};
        read(server_sock, welcome_buffer, 1024);
        cout << "Ignored message: " << welcome_buffer << endl;

        // Send the actual request to the primary server
        send(server_sock, serialized.c_str(), serialized.length(), 0);
        memset(buffer, 0, sizeof(buffer));
        recv(server_sock, buffer, sizeof(buffer) - 1, 0);
        string buffer_str(buffer);
        if (buffer_str.find('|') != string::npos)
        {
            cout << "\nServer: " << endl;
            F_2_B_Message received_message = decode_message(buffer_str);
            print_message(received_message);
        }
        else
        {
            cout << "\nServer Other: " << buffer << endl;
        }

        close(server_sock); // Close the primary server connection
    }
    return 0;
}
