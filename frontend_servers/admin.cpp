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

/**
 * @brief Retrieves a list of backend servers from the coordinator.
 *
 * This function connects to the coordinator at the given IP and port,
 * sends a LIST command, and parses the response to extract server
 * information including IP, port, and active status.
 *
 * @param coordinator_ip The IP address of the coordinator.
 * @param coordinator_port The port number of the coordinator.
 * @return A vector of server_info structures representing the backend servers.
 */
vector<server_info> get_list_of_backend_servers(const string &coordinator_ip, int coordinator_port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        cerr << "Failed to create socket." << endl;
        return {};
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(coordinator_port);
    inet_pton(AF_INET, coordinator_ip.c_str(), &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        cerr << "Connection to coordinator failed." << endl;
        close(sock);
        return {};
    }

    const char *list_command = "LIST\r\n";
    send(sock, list_command, strlen(list_command), 0);

    char buffer[4096] = {0};
    recv(sock, buffer, sizeof(buffer) - 1, 0);

    vector<server_info> servers;
    string response = buffer;

    // Parse response
    if (response.substr(0, 4) == "+OK ")
    {
        size_t start = 4, end;
        while ((end = response.find(" ", start)) != string::npos)
        {
            string server_entry = response.substr(start, end - start);
            size_t colon_pos = server_entry.find(':');
            size_t hash_pos = server_entry.find('#');
            if (colon_pos != string::npos && hash_pos != string::npos)
            {
                server_info si;
                si.ip = server_entry.substr(0, colon_pos);
                si.port = stoi(server_entry.substr(colon_pos + 1, hash_pos - colon_pos - 1));
                si.is_active = server_entry.substr(hash_pos + 1) == "1";
                servers.push_back(si);
            }
            start = end + 1;
        }
    }

    close(sock);
    return servers;
}

/**
 * @brief Retrieves a list of frontend servers from the load balancer.
 *
 * This function connects to the load balancer at the given IP and port,
 * sends an HTTP GET request to retrieve the list of servers, and parses
 * the response to extract server information including IP, port, and active status.
 *
 * @param load_balancer_ip The IP address of the load balancer.
 * @param load_balancer_port The port number of the load balancer.
 * @return A vector of server_info structures representing the frontend servers.
 */

vector<server_info> get_list_of_frontend_servers(const string &load_balancer_ip, int load_balancer_port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        cerr << "Failed to create socket." << endl;
        return {};
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(load_balancer_port);
    inet_pton(AF_INET, load_balancer_ip.c_str(), &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        cerr << "Connection to load balancer failed." << endl;
        close(sock);
        return {};
    }

    string list_command = "GET /LIST HTTP/1.1\r\nHost: " + load_balancer_ip + "\r\n\r\n";
    send(sock, list_command.c_str(), list_command.length(), 0);

    char buffer[4096] = {0};
    recv(sock, buffer, sizeof(buffer) - 1, 0);
    close(sock);

    vector<server_info> servers;
    string response = buffer;

    // Parse the response to separate headers from body
    string headers, body;
    size_t body_pos = response.find("\r\n\r\n");
    if (body_pos != string::npos)
    {
        headers = response.substr(0, body_pos);
        body = response.substr(body_pos + 4); // Skip the "\r\n\r\n"
    }
    else
    {
        // Error handling if response format is unexpected
        cerr << "Invalid HTTP response received." << endl;
        cout << response << endl;
        return {};
    }

    // Extract server entries from the body
    istringstream response_stream(body);
    string server_entry;
    while (getline(response_stream, server_entry, ','))
    {
        size_t colon_pos = server_entry.find(':');
        size_t hash_pos = server_entry.find('#');
        if (colon_pos != string::npos && hash_pos != string::npos)
        {
            server_info si;
            si.ip = server_entry.substr(0, colon_pos);
            si.port = stoi(server_entry.substr(colon_pos + 1, hash_pos - colon_pos - 1));
            si.is_active = server_entry.substr(hash_pos + 1) == "1";
            servers.push_back(si);
        }
    }

    return servers;
}

/**
 * @brief Generates HTML content from server information vectors.
 *
 * This function takes vectors of frontend and backend servers and
 * generates an HTML string displaying their status and providing
 * options to toggle their active state.
 *
 * @param frontend_servers A vector of server_info structures for frontend servers.
 * @param backend_servers A vector of server_info structures for backend servers.
 * @return A string containing the generated HTML content.
 */

string get_admin_html_from_vector(const vector<server_info> &frontend_servers, const vector<server_info> &backend_servers)
{
    stringstream html;
    html << "<!DOCTYPE html><html><head><title>Admin - Server Status</title>";
    html << "<style>";
    html << "body { font-family: Verdana, Geneva, Tahoma, sans-serif; margin: 20px; background-color: #e7cccb; color: #282525}";
    html << "h2 { text-align: center; background-color: #3737516b; padding: 10px; border-radius: 10px; margin: 20px 0; }"; // Light blue background with padding
    html << "h3{ text-align: left; }";
    html << ".server-grid { display: flex; flex-wrap: wrap; justify-content: space-around; }";
    html << ".server-card-1 { background-color: #161637; border-radius: 10px; width: 250px; margin: 10px; padding: 20px; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); text-align: center; height: 100px;  flex-direction: column; justify-content: center; align-items: center;}";
    html << ".server-card-2 { background-color: #161637; border-radius: 10px; width: 250px; margin: 10px; padding: 20px; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); text-align: center; height: 150px; flex-direction: column; justify-content: center; align-items: center;}";
    html << ".addr { font-size: 20px; color: white;}";
    html << ".activeButton { background-color: red; color: white; font-weight: bold; display: inline-block; width: 45%; margin: 0 auto; margin-bottom: 10px;}";
    html << ".inactiveButton { background-color: green; color: white; font-weight: bold; display: inline-block; width: 45%; margin: 0 auto; margin-bottom: 10px;}";
    html << ".activeStatus, .inactiveStatus { background-color: white; padding: 5px 10px; border-radius: 5px; border: 1px solid #ddd; display: inline-block; width: 50%;}";
    html << ".activeStatus { color: #228B22; margin: 10px; font-weight: bold;}";
    html << ".inactiveStatus { color: red; margin: 10px; font-weight: bold;}";
    html << "button { margin: 0 auto; padding: 8px 16px; border: none; border-radius: 5px; cursor: pointer; }";
    html << "button:hover { opacity: 0.8; }";
    html << ".button-bar { display: flex; justify-content: space-between; margin-bottom: 20px; }"; // Flex container for buttons
    html << ".new-button button { background-color: #161637; color: white; border: none; padding: 10px 20px; border-radius: 20px; cursor: pointer; }";
    html << "</style></head><body>";
    html << "<div class='button-bar'>";                                                                     // Flex container for buttons
    html << "<div class = 'new-button'><button onclick=\"location.href='/home';\">Home</button></div>";     // Home button
    html << "<div class = 'new-button'><button onclick=\"location.href='/logout';\">Logout</button></div>"; // Logout button
    html << "</div>";
    html << "<h2>Server Status</h2>";

    // Frontend Servers
    html << "<h3>Frontend Servers</h3><div class='server-grid'>";
    for (const auto &server : frontend_servers)
    {
        std::string server_addr = server.ip + ":" + std::to_string(server.port);
        html << "<div class='server-card-1'>";
        html << "<div class = 'addr'>" << server_addr << "</div>";
        html << "<div class='" << (server.is_active ? "activeStatus" : "inactiveStatus") << "'>" << (server.is_active ? "Active" : "Inactive") << "</div>";
        html << "<form method='POST' action='http://" << server_addr << (server.is_active ? "/suspend" : "/revive") << "'>";
        html << "<button type='submit' class='" << (server.is_active ? "activeButton" : "inactiveButton") << "'>TOGGLE</button></form>";
        html << "</div>";
    }
    html << "</div>";

    // Backend Servers
    html << "<h3>Backend Servers</h3><div class='server-grid'>";
    for (const auto &server : backend_servers)
    {
        std::string server_addr = server.ip + ":" + std::to_string(server.port);
        html << "<div class='server-card-2'>";
        html << "<div class = 'addr'>" << server_addr << "</div>";
        html << "<div class='" << (server.is_active ? "activeStatus" : "inactiveStatus") << "'>" << (server.is_active ? "Active" : "Inactive") << "</div>";
        html << "<form method='POST' action='/admin?toggle=" << (server.is_active ? "suspend" : "activate") << "&server=" << server_addr << "'>";
        html << "<button type='submit' class='" << (server.is_active ? "activeButton" : "inactiveButton") << "'>TOGGLE</button></form>";
        html << "<a href='/admin/" << server_addr << "' style='text-decoration: none; '><button>Show Data</button></a>";
        html << "</div>";
    }
    html << "</div></body></html>";
    return html.str();
}

/**
 * @brief Fetches data from a server.
 *
 * This function connects to a server at the given IP and port,
 * sends a request to list data, and parses the response to
 * extract key-value pairs of data.
 *
 * @param ip The IP address of the server.
 * @param port The port number of the server.
 * @return A map containing the data organized by row and column keys.
 */
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

/**
 * @brief Generates HTML content from server data.
 *
 * This function takes a map of server data and generates an HTML
 * string displaying the data in a table format.
 *
 * @param data A map containing the server data organized by row and column keys.
 * @param server_ip The IP address of the server.
 * @param server_port The port number of the server.
 * @return A string containing the generated HTML content.
 */

string generate_html_from_data(const map<string, map<string, string>> &data, const string &server_ip, int server_port)
{
    stringstream html;
    html << "<!DOCTYPE html><html><head><title>Server Data</title>";
    html << "<style>";
    html << "body { font-family: Verdana, Geneva, Tahoma, sans-serif; margin: 20px; background-color: #e7cccb; color: #282525}";
    html << "h2 { text-align: center; }";
    html << "table { width: 100%; border-collapse: collapse; }";
    html << "th, td { border: 1px solid #ccc; padding: 10px; text-align: left; }";
    html << "th { background-color: #161637; color: white; }";
    html << "tr:nth-child(even) { background-color: #e9e9e9; }";
    html << "tr:nth-child(odd) { background-color: #d1d1d1; }";
    html << "tr:hover { background-color: #ffffff; }";
    html << "div.scrollable { max-width: 300px; word-wrap: break-word; max-height: 100px; overflow: auto; width: 100%}";
    html << ".button-bar { display: flex; justify-content: space-between; margin-bottom: 20px; }"; // Flex container for buttons
    html << "button { background-color: #161637; color: white; border: none; padding: 10px 20px; border-radius: 20px; cursor: pointer; }";
    html << "button:hover { background-color: #27274a; }";
    html << "</style></head><body>";
    html << "<div class='button-bar'>";                                     // Flex container for buttons
    html << "<button onclick=\"location.href='/home';\">Home</button>";     // Home button
    html << "<button onclick=\"location.href='/logout';\">Logout</button>"; // Logout button
    html << "</div>";
    html << "<h2>Data from Server " << server_ip << ":" << server_port << "</h2>";
    html << "<table>";
    html << "<tr><th>Row Key</th><th>Column Key</th><th>Value</th></tr>";

    for (const auto &row_pair : data)
    {
        for (const auto &col_pair : row_pair.second)
        {
            html << "<tr><td><div class='scrollable'>" << row_pair.first << "</div></td><td><div class='scrollable'>" << col_pair.first << "</div></td><td><div class='scrollable'>" << col_pair.second << "</div></td></tr>";
        }
    }

    html << "</table>";
    html << "</body></html>";
    return html.str();
}

/**
 * @brief Handles a toggle request for a server.
 *
 * This function parses the query string to determine the server
 * and the action (suspend or revive), sends the appropriate
 * command to the server, and returns a response.
 *
 * @param query The query string containing the toggle action and server address.
 * @return A string indicating the result of the toggle request.
 */

string handle_toggle_request(const string &query)
{
    size_t toggle_pos = query.find("toggle=");
    size_t server_pos = query.find("&server=");
    if (toggle_pos == string::npos || server_pos == string::npos)
    {
        return "Invalid parameters";
    }

    string toggle = query.substr(toggle_pos + 7, server_pos - (toggle_pos + 7));
    string server_addr = query.substr(server_pos + 8);
    size_t colon_pos = server_addr.find(':');
    if (colon_pos == string::npos)
    {
        return "Invalid server address";
    }

    string ip = server_addr.substr(0, colon_pos);
    int port = stoi(server_addr.substr(colon_pos + 1));
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

    string serialized = encode_message(msg);
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

/**
 * @brief Safely converts a string to an integer.
 *
 * This function attempts to convert the given string to an integer
 * and stores the result in the provided output parameter. It handles
 * exceptions and returns false if the conversion fails.
 *
 * @param str The string to convert.
 * @param out The integer output parameter to store the result.
 * @return True if the conversion is successful, false otherwise.
 */

bool safe_stoi(const string &str, int &out)
{
    try
    {
        out = stoi(str);
        return true;
    }
    catch (const invalid_argument &e)
    {
        cerr << "Invalid argument: " << e.what() << endl;
        return false;
    }
    catch (const out_of_range &e)
    {
        cerr << "Out of range: " << e.what() << endl;
        return false;
    }
}
