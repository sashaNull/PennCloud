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

string get_admin_html_from_vector(const vector<server_info> &frontend_servers, const vector<server_info> &backend_servers)
{
    stringstream html;
    html << "<!DOCTYPE html><html><head><title>Admin - Server Status</title>";
    html << "<style>";
    html << "body { font-family: Arial, sans-serif; margin: 40px; }";
    html << "h1 { color: #333; }";
    html << "ul { list-style-type: none; padding: 0; }";
    html << "li { margin: 10px 0; padding: 10px; background-color: #f9f9f9; border: 1px solid #ddd; display: flex; justify-content: space-between; align-items: center; }";
    html << ".activeButton { background-color: red; color: white; }";
    html << ".inactiveButton { background-color: green; color: white; }";
    html << ".activeStatus { color: green; }";
    html << ".inactiveStatus { color: red; }";
    html << "button { margin-right: 10px; padding: 5px 10px; border: none; border-radius: 5px; cursor: pointer; }";
    html << "button:hover { opacity: 0.8; }";
    html << "form { display: inline; }";
    html << "</style></head><body>";
    html << "<h1>Server Status</h1><h2>Frontend Servers</h2><ul>";

    for (const auto &server : frontend_servers)
    {
        string server_addr = server.ip + ":" + to_string(server.port);
        html << "<li>" << server_addr << " - "
             << "<span class='" << (server.is_active ? "activeStatus" : "inactiveStatus") << "'>"
             << (server.is_active ? "Active" : "Inactive") << "</span> "
             << "<form method='POST' action='http://" << server_addr
             << (server.is_active ? "/suspend" : "/revive") << "'><button type='submit' class='"
             << (server.is_active ? "activeButton" : "inactiveButton") << "'>TOGGLE</button></form><div></div>"
             << "</li>";
    }

    html << "</ul><h2>Backend Servers</h2><ul>";

    for (const auto &server : backend_servers)
    {
        string server_addr = server.ip + ":" + to_string(server.port);
        html << "<li>" << server_addr << " - "
             << "<span class='" << (server.is_active ? "activeStatus" : "inactiveStatus") << "'>"
             << (server.is_active ? "Active" : "Inactive") << "</span> "
             << "<form method='POST' action='/admin?toggle="
             << (server.is_active ? "suspend" : "activate")
             << "&server=" << server_addr
             << "'><button type='submit' class='"
             << (server.is_active ? "activeButton" : "inactiveButton") << "'>TOGGLE</button></form> "
             << "<a href='/admin/" << server_addr << "' style='text-decoration: none;'><button>Show Data</button></a>"
             << "</li>";
    }

    html << "</ul></body></html>";
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
