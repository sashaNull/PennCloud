#ifndef CLIENT_COMMUNICATION_H
#define CLIENT_COMMUNICATION_H

#include <unordered_map>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <sys/socket.h>

void send_response(int client_fd, int status_code, const std::string &status_message, const std::string &content_type, const std::string &body, const std::string &cookie = "");

std::unordered_map<std::string, std::string> parse_http_header(const std::string &request);

std::unordered_map<std::string, std::string> load_html_files();

void redirect(int client_fd, std::string redirect_to);

void redirect_with_cookie(int client_fd, std::string redirect_url, std::string cookie);

#endif // CLIENT_COMMUNICATION_H
