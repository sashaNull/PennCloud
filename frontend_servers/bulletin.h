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

/**
 * Retrieves a single bulletin message from a backend server. This function sends multiple network requests
 * to fetch different attributes of a bulletin message such as owner, timestamp, title, and message content.
 * Each attribute is retrieved separately, and error handling is included for communication issues.
 *
 * @param fd The file descriptor for the network socket.
 * @param uid The unique identifier for the bulletin message.
 * @param g_map_rowkey_to_server A reference to a map associating row keys with server information for network communication.
 * @param g_coordinator_addr The network address configuration of the coordinator.
 * @return A BulletinMsg struct containing all the retrieved details of the bulletin message.
 */
BulletinMsg retrieve_bulletin_msg(int fd, const std::string &uid, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Retrieves multiple bulletin messages identified by their unique IDs, specifically for the requesting user's bulletins.
 * This function parses a string of comma-separated UIDs, retrieves each corresponding bulletin message using
 * the retrieve_bulletin_msg function, and aggregates them into a vector.
 *
 * @param fd The file descriptor for the network socket.
 * @param uids A string containing comma-separated unique identifiers for bulletin messages.
 * @param g_map_rowkey_to_server A reference to a map associating row keys with server information for network communication.
 * @param g_coordinator_addr The network address configuration of the coordinator.
 * @return A vector of BulletinMsg structures, each containing the details of a bulletin message.
 */
std::vector<BulletinMsg> retrieve_my_bulletin(int fd, const std::string &uids, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);  

/**
 * Retrieves multiple bulletin messages for display on a public bulletin board. Similar to retrieve_my_bulletin,
 * this function handles a list of bulletin message UIDs, fetches each message, and returns a collection of them.
 *
 * @param fd The file descriptor for the network socket.
 * @param uids A string containing comma-separated unique identifiers for bulletin messages.
 * @param g_map_rowkey_to_server A reference to a map associating row keys with server information for network communication.
 * @param g_coordinator_addr The network address configuration of the coordinator.
 * @return A vector of BulletinMsg structures, each containing the details of a bulletin message.
 */
std::vector<BulletinMsg> retrieve_bulletin_board(int fd, const std::string &uids, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);  

/**
 * Constructs an HTML document representing a bulletin board with multiple bulletin messages.
 * This function formats each message with random background colors and organizes them into a
 * visually appealing layout. It includes navigation buttons and section headers for a complete web interface.
 *
 * @param msgs A vector of BulletinMsg structures, each representing a bulletin message to be displayed.
 * @return A string containing the complete HTML document for the bulletin board.
 */
std::string construct_bulletin_board_html(std::vector<BulletinMsg> msgs);

/**
 * Constructs an HTML document representing a user's personal bulletin board with their messages.
 * This function formats each message for web display and includes interactive buttons for editing and deleting bulletins.
 * It also provides a navigation option to add new messages and to return to the main bulletin board.
 *
 * @param msgs A vector of BulletinMsg structures containing the user's bulletin messages.
 * @return A string containing the complete HTML document for the user's personal bulletin board.
 */
std::string construct_my_bulletins_html(std::vector<BulletinMsg> msgs);

/**
 * Adds a bulletin UID to a user's personal list of bulletins. This function attempts to modify the backend
 * storage to include the new UID in the user's bulletin record, handling synchronization and potential conflicts.
 *
 * @param fd The file descriptor for network communication.
 * @param username The username associated with the personal bulletin list.
 * @param uid The UID of the bulletin to add.
 * @param g_map_rowkey_to_server A reference map for server routing information.
 * @param g_coordinator_addr Network address configuration of the coordinator.
 * @return An integer status code indicating the operation success or error type.
 */
int add_to_my_bulletins(int fd, const std::string &username, const std::string &uid, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Updates a user's personal list of bulletins by adding or removing bulletin UIDs. Similar to updating the main bulletin board,
 * this function uses conditional put operations to ensure data integrity and handle synchronization conflicts.
 *
 * @param fd The file descriptor for network communication.
 * @param username The username associated with the personal bulletins.
 * @param uid The UID of the bulletin to be managed.
 * @param g_map_rowkey_to_server A reference map for server routing information.
 * @param g_coordinator_addr Network address configuration of the coordinator.
 * @return An integer status code (0 for success, 1 or 2 for errors).
 */
int update_to_my_bulletins(int fd, const std::string &username, const std::string &uid, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Submits a bulletin item to the backend storage. This function sends several requests to store different components
 * of a bulletin (owner, timestamp, title, message) under a specified UID. It handles communication and provides feedback
 * on any errors encountered during the network interactions.
 *
 * @param encoded_owner Encoded string of the bulletin's owner.
 * @param encoded_ts Encoded string of the bulletin's timestamp.
 * @param encoded_title Encoded string of the bulletin's title.
 * @param encoded_msg Encoded string of the bulletin's message content.
 * @param uid Unique identifier for the bulletin.
 * @param g_map_rowkey_to_server A reference map for server routing information.
 * @param g_coordinator_addr Network address configuration of the coordinator.
 * @return An integer status code (0 for success, 1 or 2 for errors).
 */
int put_bulletin_item_to_backend(const std::string &encoded_owner, const std::string &encoded_ts, const std::string &encoded_title, const std::string &encoded_msg, const std::string &uid,std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Adds a new bulletin UID to the main bulletin board. This function employs conditional put operations to add the UID
 * to the existing list, managing synchronization conflicts and ensuring the update is consistent across the network.
 *
 * @param fd The file descriptor for network communication.
 * @param uid The UID of the bulletin to be added.
 * @param g_map_rowkey_to_server A reference map for server routing information.
 * @param g_coordinator_addr Network address configuration of the coordinator.
 * @return An integer status code (0 for success, 1 or 2 for errors).
 */
int add_to_bulletin_board(int fd, const std::string &uid,std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Updates the main bulletin board to include a specified bulletin item. This function manages synchronization issues
 * by employing conditional put operations and handling potential conflicts. It aims to update the list of bulletin UIDs
 * on the board while ensuring consistency.
 *
 * @param fd The file descriptor for network communication.
 * @param uid The UID of the bulletin to add.
 * @param g_map_rowkey_to_server A reference map for server routing information.
 * @param g_coordinator_addr Network address configuration of the coordinator.
 * @return An integer status code (0 for success, 1 or 2 for errors).
 */
int update_to_bulletin_board(int fd, const std::string &uid,std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Removes a specific UID from a delimited string, typically used to manage lists of IDs in backend storage.
 * This utility function helps manage UIDs in strings by removing the specified UID and maintaining the delimiter format.
 *
 * @param original The original string containing UIDs separated by a delimiter.
 * @param to_remove The UID to remove from the string.
 * @param delimiter The string delimiter used between UIDs.
 * @return A new string with the specified UID removed.
 */
std::string delete_uid_from_string(const std::string& original, const std::string& to_remove, const std::string& delimiter);

/**
 * Removes a bulletin UID from a user's personal list of bulletins. This function synchronizes with the backend
 * to update the list, ensuring data consistency using conditional put operations.
 *
 * @param fd The file descriptor for network communication.
 * @param username The username whose personal bulletin list is being updated.
 * @param uid The UID of the bulletin to remove.
 * @param g_map_rowkey_to_server A reference map for server routing information.
 * @param g_coordinator_addr Network address configuration of the coordinator.
 * @return An integer status code (0 for success, 1 or 2 for errors).
 */
int delete_in_my_bulletin(int fd, const std::string &username, const std::string &uid, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Removes a bulletin UID from the main bulletin board. This function synchronizes with the backend
 * to update the board, employing conditional put operations to manage potential conflicts and ensure consistency.
 *
 * @param fd The file descriptor for network communication.
 * @param uid The UID of the bulletin to remove from the board.
 * @param g_map_rowkey_to_server A reference map for server routing information.
 * @param g_coordinator_addr Network address configuration of the coordinator.
 * @return An integer status code indicating the operation's success or error type.
 */
int delete_in_bulletin_board(int fd, const std::string &uid, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Deletes all components of a bulletin item (owner, timestamp, title, message) from the backend storage.
 * This function sends separate delete requests for each component based on the UID, handling errors and synchronization.
 *
 * @param fd The file descriptor for network communication.
 * @param uid The UID of the bulletin to delete.
 * @param g_map_rowkey_to_server A reference map for server routing information.
 * @param g_coordinator_addr Network address configuration of the coordinator.
 * @return An integer status code (0 for success, 1 or 2 for errors).
 */
int delete_bulletin_item_from_backend(int fd, const std::string &uid, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Constructs an HTML document for editing or adding a new bulletin. This function generates a form with pre-filled data
 * for title and message if available, and includes JavaScript to handle the form submission asynchronously.
 *
 * @param uid The UID of the bulletin being edited, or a placeholder for new entries.
 * @param mode Indicates whether the operation is 'edit' or 'new'.
 * @param prefill_title The title to pre-fill in the form, if any.
 * @param prefill_message The message to pre-fill in the form, if any.
 * @return A string containing the complete HTML document for editing or adding a bulletin.
 */
std::string construct_edit_bulletin_html(const std::string &uid, const std::string &mode, const std::string &prefill_title, const std::string &prefill_message);