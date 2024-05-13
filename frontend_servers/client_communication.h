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

/**
 * Sends an HTTP response to a client socket. This function constructs a complete HTTP response
 * using provided parameters such as status code, status message, content type, and response body.
 * It also allows for setting a cookie if provided. The function reports the status of the response
 * transmission, noting any errors that occur.
 *
 * @param client_fd The file descriptor for the client socket to which the response is sent.
 * @param status_code The HTTP status code (e.g., 200, 404).
 * @param status_message The HTTP status message corresponding to the status code (e.g., "OK", "Not Found").
 * @param content_type The MIME type of the response content (e.g., "text/html").
 * @param body The content of the HTTP response body.
 * @param cookie An optional cookie string to be included in the response headers; if provided, it sets a session cookie.
 */
void send_response(int client_fd, int status_code, const std::string &status_message, const std::string &content_type, const std::string &body, const std::string &cookie = "");

/**
 * Parses an HTTP request into a key-value map that includes the request method, URI, HTTP version, and headers.
 * This function is designed to split the initial request line to get the method, URI, and version, and then
 * iteratively processes each subsequent line to extract headers until the end of the header section is reached.
 *
 * @param request A string containing the raw HTTP request.
 * @return An unordered map where each key is a header field or line item (e.g., method, uri), and the value is the corresponding content.
 */
std::unordered_map<std::string, std::string> parse_http_header(const std::string &request);

/**
 * Loads HTML content from files into a map that associates endpoint names with HTML content. This utility function
 * is used to prepare content for quick retrieval by a web server, reducing file I/O operations during runtime.
 *
 * @return An unordered_map where keys are endpoint identifiers and values are the corresponding HTML content loaded from files.
 */
std::unordered_map<std::string, std::string> load_html_files();

/**
 * Sends an HTTP redirect response to the client. This function constructs and sends a 302 Found response,
 * specifying a new location for the client to fetch. It also sets the connection to keep-alive.
 *
 * @param client_fd The file descriptor for the client socket.
 * @param redirect_to The URL to which the client should be redirected.
 */
void redirect(int client_fd, std::string redirect_to);

/**
 * Sends an HTTP redirect response to the client, including a cookie in the response headers. This function is used
 * for scenarios where client state needs to be maintained across the redirect, typically in web authentication flows.
 *
 * @param client_fd The file descriptor for the client socket.
 * @param redirect_url The URL to which the client should be redirected.
 * @param cookie A cookie string to be included in the response headers.
 */
void redirect_with_cookie(int client_fd, std::string redirect_url, std::string cookie);

#endif // CLIENT_COMMUNICATION_H
