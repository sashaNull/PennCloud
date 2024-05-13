#ifndef DRIVE_H
#define DRIVE_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <netinet/in.h>

/**
 * Deletes all chunks associated with a given row key in the backend storage. The function first retrieves the
 * total number of chunks and then proceeds to delete each chunk individually. It includes extensive error handling
 * to manage and report communication issues with the coordinator or backend services.
 *
 * @param fd The file descriptor for the network connection.
 * @param rowkey The row key identifier for which chunks are stored.
 * @param g_map_rowkey_to_server A map linking row keys to server information for network communication.
 * @param g_coordinator_addr The network address configuration of the coordinator.
 * @return An integer status code (0 for success, 1 for coordinator communication errors, 2 for backend errors).
 */
int delete_file_chunks(int fd, const std::string &rowkey, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Copies file chunks from one row key to another within the backend storage. This function handles the entire
 * process of copying, including retrieving the number of chunks, copying each chunk individually, and ensuring
 * all new entries are accurately replicated. Each step includes handling for potential errors in communication
 * or data integrity issues.
 *
 * @param fd The file descriptor for network communications.
 * @param old_row_key The source row key from which to copy chunks.
 * @param new_row_key The destination row key to which chunks are copied.
 * @param g_map_rowkey_to_server A reference map for server routing information.
 * @param g_coordinator_addr Network address configuration of the coordinator.
 * @return An integer status code indicating the overall success of the copy operation, or the specific error encountered.
 */
int copyChunks(int fd, const std::string &old_row_key, const std::string &new_row_key, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

#endif // DRIVE_H