#include "./backend_communication.h"
#include "../utils/utils.h"
#include "./bulletin.h"
#include <algorithm>

using namespace std;

int render_bulletin_board(string uids)
{
    // parse item uids (uid1, uid2, ....)
    // for each uid, fetch (bulletin/uid owner, bulletin/uid timestamp, bulletin/uid title, bulletin/uid message)
    // store as list of BulletinMsg objects
    // render bulletin board
    return 1;
}

int render_my_bulletins(string uids)
{
    // parse item uids (uid1, uid2, ....)
    // for each uid, fetch (bulletin/uid timestamp, bulletin/uid title, bulletin/uid message)
    // store as list of BulletinMsg objects
    // render as list with edit and delete buttons
    // if click on edit button, /edit-bulletin?title=<title>&msg=<msg>
    // if click on delete button, /delete-bulletin
    return 1;
}

std::vector<std::string> parse_json_list_to_vector(const std::string &json_list)
{
    std::vector<std::string> result;
    if (json_list.empty() || json_list == "[]")
        return result;

    std::string trimmed = json_list.substr(1, json_list.size() - 2);
    std::stringstream ss(trimmed);
    std::string item;

    while (std::getline(ss, item, ','))
    {
        // Trim potential white spaces left by splitting
        item.erase(0, item.find_first_not_of(" \t\n\r\f\v")); // Left trim
        item.erase(item.find_last_not_of(" \t\n\r\f\v") + 1); // Right trim
        result.push_back(item);
    }

    return result;
}

std::string join(const std::vector<std::string> &items, const std::string &delimiter)
{
    if (items.empty())
        return "";
    std::ostringstream joined;
    auto iter = items.begin();

    joined << *iter++;

    while (iter != items.end())
    {
        joined << delimiter << *iter++;
    }

    return joined.str();
}

int update_bulletin_list(const string &list_key, const string &uid, int server_fd, map<string, string> &server_map, sockaddr_in coordinator_addr)
{
    string response_value, error_msg;
    int status, result;

    F_2_B_Message msg;
    msg.type = 1; // '1' for GET operation
    msg.rowkey = list_key;
    msg.colkey = "items";
    result = send_msg_to_backend(server_fd, msg, response_value, status, error_msg, msg.rowkey, msg.colkey, server_map, coordinator_addr, "get");
    if (result != 0)
        return result;

    // Determine new value
    vector<string> items;
    if (status == 1 && error_msg == "Rowkey does not exist")
    {
        items.push_back(uid);
    }
    else
    {
        if (!response_value.empty() && response_value != "[]")
        {
            items = parse_json_list_to_vector(response_value);
        }
        if (std::find(items.begin(), items.end(), uid) == items.end())
        {
            items.push_back(uid);
        }
    }
    string new_value = "[" + join(items, ",") + "]"; // Assuming function to join vector items into a string

    // Put new value
    msg.type = 2; // '2' for PUT operation
    msg.value = new_value;
    result = send_msg_to_backend(server_fd, msg, response_value, status, error_msg, msg.rowkey, msg.colkey, server_map, coordinator_addr, "put");
    return result;
}

int put_bulletin_item_to_backend(string username, string encoded_owner, string encoded_ts, string encoded_title, string encoded_msg, string uid, map<string, string> global_server_map, sockaddr_in global_coordinator_addr, int server_fd)
{
    // cput uid into (bullet-board items)
    // put bulletin/uid owner, bulletin/uid timestamp, bulletin/uid title, bulletin/uid message
    // cput uid into (username_bulletin items)
    F_2_B_Message msg;
    msg.type = 2; // Assuming '2' is the type for PUT operation
    msg.rowkey = "bulletin/" + uid;
    msg.colkey = "owner";
    msg.value = encoded_owner;
    msg.value2 = "";
    msg.status = 0;
    msg.isFromPrimary = 0;
    msg.errorMessage = "";

    string response_value, error_msg;
    int status, result;

    // Store owner
    result = send_msg_to_backend(server_fd, msg, response_value, status, error_msg, msg.rowkey, msg.colkey, global_server_map, global_coordinator_addr, "put");
    if (result != 0)
        return result;

    // Store timestamp
    msg.colkey = "timestamp";
    msg.value = encoded_ts;
    result = send_msg_to_backend(server_fd, msg, response_value, status, error_msg, msg.rowkey, msg.colkey, global_server_map, global_coordinator_addr, "put");
    if (result != 0)
        return result;

    // Store title
    msg.colkey = "title";
    msg.value = encoded_title;
    result = send_msg_to_backend(server_fd, msg, response_value, status, error_msg, msg.rowkey, msg.colkey, global_server_map, global_coordinator_addr, "put");
    if (result != 0)
        return result;

    // Store message
    msg.colkey = "message";
    msg.value = encoded_msg;
    result = send_msg_to_backend(server_fd, msg, response_value, status, error_msg, msg.rowkey, msg.colkey, global_server_map, global_coordinator_addr, "put");
    if (result != 0)
        return result;

    // Add uid to bulletin-board items
    result = update_bulletin_list("bulletin-board", uid, server_fd, global_server_map, global_coordinator_addr);
    if (result != 0)
        return result;

    // Add uid to username_bulletin items
    result = update_bulletin_list(username + "_bulletin", uid, server_fd, global_server_map, global_coordinator_addr);
    if (result != 0)
        return result;

    return 0; // Success
}

int delete_bulletin_item(string username, string uid)
{
    // cput new (bullet-board items)
    // cput new (username_bulletin items)
    // delete bulletin/uid owner, timestamp, title, message
    return 1;
}

map<string, string> parse_query_params(const string &uri)
{
    map<string, string> params;
    size_t start = uri.find('?');
    if (start == string::npos)
        return params;
    start++;
    size_t end = uri.find('&', start);
    while (end != string::npos)
    {
        string param = uri.substr(start, end - start);
        size_t eq = param.find('=');
        if (eq != string::npos)
        {
            string key = param.substr(0, eq);
            string value = param.substr(eq + 1);
            params[key] = value;
        }
        start = end + 1;
        end = uri.find('&', start);
    }
    string lastParam = uri.substr(start);
    size_t eq = lastParam.find('=');
    if (eq != string::npos)
    {
        string key = lastParam.substr(0, eq);
        string value = lastParam.substr(eq + 1);
        params[key] = value;
    }
    return params;
}

string generate_edit_bulletin_form(const string &username, const string &title, const string &message)
{
    string html = "<html><head><title>Edit Bulletin</title></head><body>";
    html += "<h1>Welcome, " + username + "</h1>";
    html += "<form action='/edit-bulletin' method='POST'>";
    html += "Title: <input type='text' name='title' value='" + title + "'><br>";
    html += "Message: <textarea name='msg'>" + message + "</textarea><br>";
    html += "<input type='submit' value='Submit'>";
    html += "</form>";
    html += "</body></html>";
    return html;
}

std::string trim(const std::string &str)
{
    size_t first = str.find_first_not_of(" \t\n\v\f\r");
    if (std::string::npos == first)
    {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\n\v\f\r");
    return str.substr(first, (last - first + 1));
}

std::map<std::string, std::string> parseQueryString(const std::string &queryString)
{
    std::map<std::string, std::string> result;

    size_t pos = 0;
    while (pos < queryString.size())
    {
        size_t keyStart = queryString.find_first_not_of("=&", pos);
        if (keyStart == std::string::npos)
            break;

        size_t keyEnd = queryString.find('=', keyStart);
        if (keyEnd == std::string::npos)
            break;

        size_t valueStart = queryString.find_first_not_of("=&", keyEnd + 1);
        if (valueStart == std::string::npos)
            break;

        size_t valueEnd = queryString.find('&', valueStart);
        std::string key = trim(queryString.substr(keyStart, keyEnd - keyStart));
        std::string value = trim(queryString.substr(valueStart, valueEnd - valueStart));
        result[key] = value;

        if (valueEnd == std::string::npos)
            break;
        pos = valueEnd + 1;
    }

    return result;
}