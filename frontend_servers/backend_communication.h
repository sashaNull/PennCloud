#ifndef BACKEND_COMMUNICATION_H
#define BACKEND_COMMUNICATION_H

#include <string>
#include <vector>
#include <iostream>
#include <cstdlib> // for EXIT_FAILURE
#include <sys/socket.h> // for socket() and related constants
#include <netinet/in.h>
#include "../utils/utils.h"


/**
 * Creates a socket using the Internet Protocol family. If the socket creation fails, the program
 * will print an error message and exit. This function ensures a socket descriptor is returned only
 * if the socket is successfully created.
 *
 * @return The file descriptor of the newly created socket.
 */
int create_socket();

/**
 * Sends a message over a network socket. This function takes a file descriptor and a string message,
 * sends the message over the network, and logs the outcome. It handles the potential error if the message
 * cannot be sent.
 *
 * @param fd The file descriptor of the socket through which the message is sent.
 * @param to_send The message to be sent.
 */
void send_message(int fd, const std::string &to_send);

/**
 * Receives a message from a network socket into a provided buffer. It extracts one complete message
 * based on a delimiter ("\r\n") from the buffer. This function handles possible receive errors and logs
 * if the server closes the connection.
 *
 * @param fd The file descriptor of the socket from which the message is received.
 * @param buffer A reference to a string where the received data is accumulated.
 * @param BUFFER_SIZE The maximum number of bytes to read in a single recv operation.
 * @return A complete message up to the delimiter if available, or an empty string if an error occurs or the connection is closed.
 */
std::string receive_one_message(int fd, const std::string& buffer, const unsigned int BUFFER_SIZE);

/**
 * Establishes a connection to a specified address and sends a message, then waits to receive a response.
 * This function is responsible for setting up a connection, sending a message, and processing the received
 * data to extract meaningful responses based on predefined delimiters. It closes the socket once the communication
 * is complete or if an error occurs.
 *
 * @param fd The file descriptor for the original socket (not used for the connection).
 * @param addr_str The IP address and port as a string to which the connection should be established.
 * @param msg The message to be sent, encapsulated in an F_2_B_Message structure.
 * @return The received message, also encapsulated in an F_2_B_Message structure, indicating the outcome of the operation.
 */
F_2_B_Message send_and_receive_msg(int fd, const std::string &addr_str, F_2_B_Message msg);

/**
 * Constructs a message structure for backend communication. This utility function packages various components
 * of a message into a structured format, primarily used for sending requests or commands to a backend system.
 *
 * @param type The type of message or command.
 * @param rowkey The primary identifier for the row in a database or storage system.
 * @param colkey The column identifier associated with the rowkey.
 * @param value The primary data or value associated with the colkey.
 * @param value2 Additional data or value relevant to the operation.
 * @param errmsg An error message if the operation encounters problems.
 * @param status The status code representing the success or failure of the operation.
 * @return An F_2_B_Message structure filled with the provided parameters.
 */
F_2_B_Message construct_msg(int type, const std::string &rowkey, const std::string &colkey,const std::string &value, const std::string &value2, const std::string &errmsg, int status);

/**
 * Queries the coordinator for information about which backend server to connect to for a specific rowkey and operation type.
 * This function establishes a connection to the coordinator, sends the request, and interprets the response to extract
 * the backend server address. It handles connection failures and incomplete message receptions.
 *
 * @param coordinator_addr The network address configuration of the coordinator.
 * @param rowkey The specific rowkey for which backend server information is requested.
 * @param type The type of operation (e.g., read, write) for which the backend server address is needed.
 * @return The backend server address as a string if successful, or an empty string if an error occurs.
 */
std::string ask_coordinator(sockaddr_in coordinator_addr, const std::string &rowkey, const std::string &type);

/**
 * Checks the connectivity to a specified backend server by attempting a simple fetch operation.
 * This function is primarily used to validate that a cached server address is still valid before attempting
 * more complex operations.
 *
 * @param fd The file descriptor for network communication.
 * @param backend_serveraddr_str The backend server address as a string.
 * @param rowkey The rowkey associated with the check.
 * @param colkey The column key associated with the check.
 * @return true if the connection is confirmed, false otherwise.
 */
bool check_backend_connection(int fd, const std::string &backend_serveraddr_str, const std::string &rowkey, const std::string &colkey);

/**
 * Determines the appropriate backend server address for a given rowkey and column key, consulting a local cache first
 * and falling back to querying the coordinator if necessary. This function ensures that communication with the backend
 * server is routed correctly, especially when a cached address is outdated or invalid.
 *
 * @param fd The file descriptor for network communication.
 * @param rowkey The rowkey for which the backend server address is needed.
 * @param colkey The column key for which the backend server address is needed.
 * @param g_map_rowkey_to_server A reference to a map storing cached backend server addresses.
 * @param g_coordinator_addr The network address configuration of the coordinator.
 * @param type The type of operation to be performed, affecting which server might be appropriate.
 * @return The backend server address as a string if successful, or an empty string if an error occurs.
 */
std::string get_backend_server_addr(int fd, const std::string &rowkey, const std::string &colkey, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr, const std::string &type);

/**
 * Sends a structured message to a backend server, automatically determining or verifying the server address as needed.
 * This function handles the full cycle of sending a message, including any necessary preliminary checks or follow-ups
 * to ensure the message reaches the appropriate backend server and that a response is received.
 *
 * @param fd The file descriptor for network communication.
 * @param msg_to_send The structured message (F_2_B_Message) to send to the backend.
 * @param value A reference to a string to capture any direct return value from the backend.
 * @param status A reference to an integer to capture the status of the backend operation.
 * @param err_msg A reference to a string to capture any error message from the backend.
 * @param rowkey The rowkey involved in the backend operation.
 * @param colkey The column key involved in the backend operation.
 * @param g_map_rowkey_to_server A reference to a map storing backend server addresses.
 * @param g_coordinator_addr The network address configuration of the coordinator.
 * @param type The type of operation to be performed, influencing server selection.
 * @return An integer status code indicating the success of the operation or the nature of any failure.
 */
int send_msg_to_backend(int fd, F_2_B_Message msg_to_send, std::string &value, int &status, std::string &err_msg,
                        const std::string &rowkey, const std::string &colkey, 
                        std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr,
                        const std::string &type);

#endif // BACKEND_COMMUNICATION_H
