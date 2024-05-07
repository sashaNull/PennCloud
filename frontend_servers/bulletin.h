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
#include <cstdlib>
#include <ctime>

struct BulletinMsg
{
    std::string uid;
    std::string owner;
    std::string timestamp;
    std::string title;
    std::string message;
};


BulletinMsg retrieve_bulletin_msg(int fd, const std::string &uid, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

std::vector<BulletinMsg> retrieve_my_bulletin(int fd, const std::string &uids, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);  

std::vector<BulletinMsg> retrieve_bulletin_board(int fd, const std::string &uids, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);  

std::string construct_bulletin_board_html(std::vector<BulletinMsg> msgs);

std::string construct_my_bulletins_html(std::vector<BulletinMsg> msgs);

int add_to_my_bulletins(int fd, const std::string &username, const std::string &uid, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

int update_to_my_bulletins(int fd, const std::string &username, const std::string &uid, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

int put_bulletin_item_to_backend(const std::string &encoded_owner, const std::string &encoded_ts, const std::string &encoded_title, const std::string &encoded_msg, const std::string &uid,std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

int add_to_bulletin_board(int fd, const std::string &uid,std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

int update_to_bulletin_board(int fd, const std::string &uid,std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);


std::string delete_uid_from_string(const std::string& original, const std::string& to_remove, const std::string& delimiter);


int delete_in_my_bulletin(int fd, const std::string &username, const std::string &uid, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

int delete_in_bulletin_board(int fd, const std::string &uid, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

int delete_bulletin_item_from_backend(int fd, const std::string &uid, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

std::string construct_edit_bulletin_html(const std::string &uid, const std::string &mode, const std::string &prefill_title, const std::string &prefill_message);