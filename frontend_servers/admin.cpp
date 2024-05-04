#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>

#include "admin.h" // Include the header where server_info is defined

std::vector<server_info> get_list_of_servers(const std::string &coordinator_ip, int coordinator_port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "Failed to create socket." << std::endl;
        return {};
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(coordinator_port);
    inet_pton(AF_INET, coordinator_ip.c_str(), &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Connection to coordinator failed." << std::endl;
        close(sock);
        return {};
    }

    const char *list_command = "LIST\r\n";
    send(sock, list_command, strlen(list_command), 0);

    char buffer[4096] = {0};
    recv(sock, buffer, sizeof(buffer) - 1, 0);

    std::vector<server_info> servers;
    std::string response = buffer;

    // Parse response
    if (response.substr(0, 4) == "+OK ")
    {
        size_t start = 4, end;
        while ((end = response.find(" ", start)) != std::string::npos)
        {
            std::string server_entry = response.substr(start, end - start);
            size_t colon_pos = server_entry.find(':');
            size_t hash_pos = server_entry.find('#');
            if (colon_pos != std::string::npos && hash_pos != std::string::npos)
            {
                server_info si;
                si.ip = server_entry.substr(0, colon_pos);
                si.port = std::stoi(server_entry.substr(colon_pos + 1, hash_pos - colon_pos - 1));
                si.is_active = server_entry.substr(hash_pos + 1) == "1";
                servers.push_back(si);
            }
            start = end + 1;
        }
    }

    close(sock);
    return servers;
}

std::string get_admin_html_from_vector(const std::vector<server_info> &servers)
{
    std::stringstream html;
    html << "<html><head><title>Admin - Server Status</title></head><body>";
    html << "<h1>Server Status</h1>";
    html << "<ul>";

    for (const auto &server : servers)
    {
        std::string server_addr = server.ip + ":" + std::to_string(server.port);
        html << "<li>" << server_addr << " - "
             << (server.is_active ? "Active" : "Inactive") << " "
             << "<form method='POST' action='/admin?toggle="
             << (server.is_active ? "suspend" : "activate")
             << "&server=" << server_addr
             << "'><button type='submit'>"
             << (server.is_active ? "Suspend" : "Activate")
             << "</button></form> "
             << "<a href='/admin/" << server_addr << "'><button>Show Data</button></a>"
             << "</li>";
    }

    html << "</ul>";
    html << "</body></html>";
    return html.str();
}

std::string handle_show_data_request(const std::string &server)
{

    std::stringstream html;
    html << "<html><body><h1>Hello World from " << server << "</h1></body></html>";
    return html.str();
}

std::string handle_toggle_request(const std::string &query)
{

    size_t toggle_pos = query.find("toggle=");
    size_t server_pos = query.find("&server=");
    if (toggle_pos == std::string::npos || server_pos == std::string::npos)
    {
        return "Invalid parameters";
    }

    std::string toggle = query.substr(toggle_pos + 7, server_pos - (toggle_pos + 7));
    std::string server_addr = query.substr(server_pos + 8);

    if (toggle != "suspend" && toggle != "activate")
    {
        return "Invalid toggle value";
    }

    std::cout << "Toggling " << toggle << " for server " << server_addr << std::endl;

    return "";
}
