struct server_info
{
    std::string ip;
    int port;
    bool is_active;
};
std::string get_admin_html_from_vector(const std::vector<server_info> &servers);
std::vector<server_info> get_list_of_servers(const std::string &coordinator_ip, int coordinator_port);
std::string handle_show_data_request(const std::string &server);
std::string handle_toggle_request(const std::string &query);