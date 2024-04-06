#include <iostream>
#include <cstring>      // for strlen and memset
#include <sys/socket.h> // for socket functions
#include <netinet/in.h> // for sockaddr_in
#include <arpa/inet.h>  // for inet_pton
#include <unistd.h>     // for close
#include <sys/select.h> // for select()
#include <sstream>
#include "../../utils/utils.h"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <Server IP> <Port>" << std::endl;
        return 1;
    }

    const char *server_ip = argv[1];
    int server_port = std::stoi(argv[2]);

    // Create a socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "Error in socket creation" << std::endl;
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Connection Failed" << std::endl;
        return 1;
    }

    std::cout << "Connected to the server. Type 'quit' to exit." << std::endl;

    while (true)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        std::cout << "> ";
        std::cout.flush();

        if (select(sock + 1, &readfds, NULL, NULL, NULL) < 0)
        {
            std::cerr << "Select error" << std::endl;
            break;
        }

        if (FD_ISSET(sock, &readfds))
        {
            char buffer[1024] = {0};
            int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0)
            {
                std::cout << "Server closed the connection or error occurred." << std::endl;
                break;
            }
            std::cout << "\nServer: " << buffer << std::endl;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            std::string input;
            std::getline(std::cin, input);
            if (input == "quit")
            {
                input = input + "\n";
                std::cout << "Closing the connection" << std::endl;
                send(sock, input.c_str(), input.length(), 0);
                break;
            }
            std::istringstream iss(input);
            std::string command, rowkey, colkey, value, value2;
            iss >> command >> rowkey >> colkey;

            F_2_B_Message message;
            message.rowkey = rowkey;
            message.colkey = colkey;
            message.type = 0;
            message.status = 0;

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
            else
            {
                std::cerr << "Unknown command" << std::endl;
                continue;
            }

            // Simple serialization (you may need a more complex method for actual use)
            std::ostringstream oss;
            oss << message.type << "|" << message.rowkey << "|" << message.colkey << "|"
                << message.value << "|" << message.value2 << "|" << message.status << "|"
                << message.errorMessage << "\n";
            std::string serialized = oss.str();

            send(sock, serialized.c_str(), serialized.length(), 0);
        }
    }

    close(sock);
    return 0;
}
