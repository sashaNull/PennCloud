#include <string>
#include <vector>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <pthread.h>
#include <map>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <regex>

struct BulletinMsg
{
    std::string owner;
    std::string timestamp;
    std::string title;
    std::string message;
};

int render_bulletin_board(std::string uids);

int render_my_bulletins(std::string uids);

int put_bulletin_item_to_backend(std::string username, std::string encoded_owner, std::string encoded_ts, std::string encoded_title, std::string encoded_msg, std::string uid, std::map<std::string, std::string> g_map_rowkey_to_server, sockaddr_in g_coordinator_addr, int fd);

int delete_bulletin_item(std::string username, std::string uid);

std::map<std::string, std::string> parse_query_params(const std::string &uri);
std::map<std::string, std::string> parseQueryString(const std::string &queryString);

std::string generate_edit_bulletin_form(const std::string &username, const std::string &title, const std::string &message);