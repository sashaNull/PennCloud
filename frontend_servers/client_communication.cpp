#include "./client_communication.h"

using namespace std;

void send_response(int client_fd, int status_code, const std::string &status_message, const std::string &content_type, const std::string &body)
{
    std::ostringstream response_stream;
    response_stream << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";
    response_stream << "Content-Type: " << content_type << "\r\n";
    response_stream << "Content-Length: " << body.length() << "\r\n";
    response_stream << "\r\n";
    response_stream << body;

    std::string response = response_stream.str();
    ssize_t bytes_sent = send(client_fd, response.c_str(), response.length(), 0);
    if (bytes_sent < 0)
    {
        std::cerr << "Failed to send response" << std::endl;
    }
    else
    {
        std::cout << "Sent response successfully, bytes sent: " << bytes_sent << std::endl;
    }
}

unordered_map<string, string> parse_http_request(const string &request)
{
    istringstream request_stream(request);
    string request_line;
    getline(request_stream, request_line);
    cout << request_line << endl;
    istringstream request_line_stream(request_line);
    string method, uri, http_version;
    request_line_stream >> method >> uri >> http_version;

    // TODO: save headers if needed
    map<string, string> headers;
    string header_line;
    while (getline(request_stream, header_line) && header_line != "\r")
    {
        size_t delimiter_pos = header_line.find(':');
        if (delimiter_pos != string::npos)
        {
            string header_name = header_line.substr(0, delimiter_pos);
            string header_value = header_line.substr(delimiter_pos + 2, header_line.length() - delimiter_pos - 3);
            headers[header_name] = header_value;
        }
    }

    string body = string(istreambuf_iterator<char>(request_stream), {});

    unordered_map<string, string> result;
    result["method"] = method;
    result["uri"] = uri;
    result["http_version"] = http_version;
    result["headers"] = "";
    result["body"] = body;

    cout << "end of parse" << endl;
    return result;
}

std::unordered_map<std::string, std::string> load_html_files()
{
    std::unordered_map<std::string, std::string> endpoint_html_map;
    // Define the list of endpoints and corresponding HTML file names
    std::vector<std::pair<std::string, std::string>> endpoints_html_files = {
        {"signup", "html_files/signup.html"},
        {"login", "html_files/login.html"},
        {"home", "html_files/home.html"},
        {"reset-password", "html_files/reset_password.html"},
        {"drive", "html_files/drive.html"}};

    for (const auto &pair : endpoints_html_files)
    {
        std::ifstream file(pair.second);
        if (file)
        {
            std::stringstream buffer;
            buffer << file.rdbuf();
            endpoint_html_map[pair.first] = buffer.str();
            file.close();
        }
        else
        {
            std::cerr << "Failed to open HTML file: " << pair.second << std::endl;
        }
    }
    return endpoint_html_map;
}

void redirect(int client_fd, std::string redirect_to)
{
    std::string response = "HTTP/1.1 302 Found\r\n";
    response += "Location: " + redirect_to + "\r\n";
    response += "Content-Length: 0\r\n";
    response += "Connection: keep-alive\r\n";
    response += "\r\n";

    send(client_fd, response.c_str(), response.size(), 0);

    std::cout << "Sent redirection response to " << redirect_to << std::endl;
}