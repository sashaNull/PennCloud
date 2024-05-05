struct server_info
{
    std::string ip;
    int port;
    bool is_active;
};
std::string get_admin_html_from_vector(const std::vector<server_info> &servers);
std::vector<server_info> get_list_of_servers(const std::string &coordinator_ip, int coordinator_port);
std::string handle_toggle_request(const std::string &query);

bool safe_stoi(const std::string &str, int &out);
std::string generate_html_from_data(const std::map<std::string, std::map<std::string, std::string>> &data, const std::string &server_ip, int server_port);

std::map<std::string, std::map<std::string, std::string>> fetch_data_from_server(const std::string &ip, int port);