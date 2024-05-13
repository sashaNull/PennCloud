/**
 * @brief Represents server information including IP, port, and active status.
 */
struct server_info
{
    std::string ip;
    int port;
    bool is_active;
};
/**
 * @brief Generates HTML content displaying server status and options.
 */
std::string get_admin_html_from_vector(const std::vector<server_info> &frontend_servers, const std::vector<server_info> &backend_servers);

/**
 * @brief Retrieves a list of backend servers from the coordinator.
 */
std::vector<server_info> get_list_of_backend_servers(const std::string &coordinator_ip, int coordinator_port);

/**
 * @brief Retrieves a list of frontend servers from the load balancer.
 */
std::vector<server_info> get_list_of_frontend_servers(const std::string &load_balancer_ip, int load_balancer_port);

/**
 * @brief Handles a toggle request for a server (suspend or revive).
 */
std::string handle_toggle_request(const std::string &query);

/**
 * @brief Safely converts a string to an integer.
 */
bool safe_stoi(const std::string &str, int &out);

/**
 * @brief Generates HTML content from server data.
 */
std::string generate_html_from_data(const std::map<std::string, std::map<std::string, std::string>> &data, const std::string &server_ip, int server_port);

/**
 * @brief Fetches data from a server.
 */
std::map<std::string, std::map<std::string, std::string>> fetch_data_from_server(const std::string &ip, int port);