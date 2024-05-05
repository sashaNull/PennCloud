#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <map>

#include "admin.h"
#include "../utils/utils.h"
using namespace std;

#define MAX_BUFFER_SIZE 1024

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
    html << "<!DOCTYPE html><html><head><title>Admin - Server Status</title>";
    html << "<style>";
    html << "body { font-family: Arial, sans-serif; margin: 40px; }";
    html << "h1 { color: #333; }";
    html << "ul { list-style-type: none; padding: 0; }";
    html << "li { margin: 10px 0; padding: 10px; background-color: #f9f9f9; border: 1px solid #ddd; }";
    html << "button { margin-right: 10px; padding: 5px 10px; background-color: #4CAF50; color: white; border: none; border-radius: 5px; cursor: pointer; }";
    html << "button:hover { background-color: #45a049; }";
    html << "form { display: inline; }";
    html << "</style></head><body>";
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
             << "<a href='/admin/" << server_addr << "' style='text-decoration: none;'><button>Show Data</button></a>"
             << "</li>";
    }

    html << "</ul>";
    html << "</body></html>";
    return html.str();
}

map<string, map<string, string>> fetch_data_from_server(const string &ip, int port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        cerr << "Failed to create socket." << endl;
        return {};
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        cerr << "Connection failed." << endl;
        close(sockfd);
        return {};
    }
    char welcome_buffer[MAX_BUFFER_SIZE] = {0};
    read(sockfd, welcome_buffer, MAX_BUFFER_SIZE);
    // cout << "Ignored message: " << welcome_buffer << endl; // Optionally log the ignored message

    F_2_B_Message request_msg;
    request_msg.type = 10;
    request_msg.rowkey = "list";
    request_msg.colkey = "list";
    request_msg.value = "";
    request_msg.value2 = "";
    request_msg.status = 0;
    request_msg.isFromPrimary = 0;
    request_msg.errorMessage = "";

    string serialized = encode_message(request_msg);
    send(sockfd, serialized.c_str(), serialized.length(), 0);

    map<string, map<string, string>> data;
    string total_data;
    bool breakingFlag = false;

    while (true)
    {
        if (breakingFlag)
            break;

        char buffer[MAX_BUFFER_SIZE] = {0};
        int bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0)
            break;

        total_data += string(buffer, bytes_received);
        // cout << total_data << endl;

        size_t pos = 0;
        while ((pos = total_data.find("\r\n")) != string::npos)
        {
            string message = total_data.substr(0, pos);
            total_data.erase(0, pos + 2);

            F_2_B_Message response = decode_message(message);
            if (response.status == 2)
            {
                breakingFlag = true;
                break;
            }
            if (response.rowkey == "terminate" && response.colkey == "terminate")
            {
                breakingFlag = true;
                break;
            }

            data[response.rowkey][response.colkey] = response.value;
        }
    }

    close(sockfd);
    return data;
}

string generate_html_from_data(const map<string, map<string, string>> &data, const string &server_ip, int server_port)
{
    stringstream html;
    html << "<!DOCTYPE html><html><head><title>Server Data</title>";
    html << "<style>";
    html << "body { font-family: Arial, sans-serif; margin: 20px; }";
    html << "table { width: 100%; border-collapse: collapse; }";
    html << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; word-wrap: break-word; }";
    html << "th { background-color: #f2f2f2; }";
    html << "tr:nth-child(even) { background-color: #f9f9f9; }";
    html << "td { max-width: 400px; }"; // Sets a maximum width for table cells
    html << "</style></head><body>";
    html << "<h1>Data from Server</h1>";
    html << "<h2>Server: " << server_ip << ":" << server_port << "</h2>"; // Header indicating which server the data is from

    for (const auto &row_pair : data)
    {
        html << "<h3>Row: " << row_pair.first << "</h3>";
        html << "<table>";
        html << "<tr><th>Column Key</th><th>Value</th></tr>";
        for (const auto &col_pair : row_pair.second)
        {
            html << "<tr><td>" << col_pair.first << "</td><td>" << col_pair.second << "</td></tr>";
        }
        html << "</table>";
    }

    html << "</body></html>";
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
    size_t colon_pos = server_addr.find(':');
    if (colon_pos == std::string::npos)
    {
        return "Invalid server address";
    }

    std::string ip = server_addr.substr(0, colon_pos);
    int port = std::stoi(server_addr.substr(colon_pos + 1));
    bool is_suspend = (toggle == "suspend");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        return "Failed to create socket";
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        close(sockfd);
        return "Connection failed";
    }
    char welcome_buffer[MAX_BUFFER_SIZE] = {0};
    read(sockfd, welcome_buffer, MAX_BUFFER_SIZE);
    // cout << "Ignored message: " << welcome_buffer << endl; // Optionally log the ignored message
    // Prepare the message
    F_2_B_Message msg;
    msg.type = is_suspend ? 5 : 6;
    msg.rowkey = is_suspend ? "suspend" : "revive";
    msg.colkey = msg.rowkey;
    msg.value = "";
    msg.value2 = "";
    msg.status = 0;
    msg.isFromPrimary = 0;
    msg.errorMessage = "";

    std::string serialized = encode_message(msg);
    send(sockfd, serialized.c_str(), serialized.length(), 0);

    char buffer[MAX_BUFFER_SIZE] = {0};
    recv(sockfd, buffer, sizeof(buffer), 0);
    // cout << "Admin response: " << buffer << endl;

    F_2_B_Message response = decode_message(buffer);
    print_message(response);

    close(sockfd);

    // Wait for 1.5 seconds before responding back to the browser
    sleep(1.5);

    return "";
}

bool safe_stoi(const std::string &str, int &out)
{
    try
    {
        out = std::stoi(str);
        return true;
    }
    catch (const std::invalid_argument &e)
    {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
        return false;
    }
    catch (const std::out_of_range &e)
    {
        std::cerr << "Out of range: " << e.what() << std::endl;
        return false;
    }
}
