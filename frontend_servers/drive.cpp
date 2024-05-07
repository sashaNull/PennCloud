#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <map>

#include "drive.h"
#include "../utils/utils.h"
#include "backend_communication.h"

using namespace std;

int delete_file_chunks(int fd, const std::string &rowkey, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr)
{
    std::string value, error_msg;
    int status, result;

    // Step 1: Get the number of chunks
    F_2_B_Message get_msg;
    get_msg.type = 1; // GET request
    get_msg.rowkey = rowkey;
    get_msg.colkey = "no_chunks";

    result = send_msg_to_backend(fd, get_msg, value, status, error_msg, rowkey, "no_chunks", g_map_rowkey_to_server, g_coordinator_addr, "get");
    if (result == 1)
    {
        cerr << "ERROR in communicating with coordinator" << endl;
        return 1;
    }
    else if (result == 2)
    {
        cerr << "ERROR in communicating with backend" << endl;
        return 2;
    }
    if (result != 0 || status != 0)
    {
        std::cerr << "Failed to retrieve number of chunks: " << error_msg << std::endl;
        return 1;
    }

    int no_chunks = 0;
    try
    {
        no_chunks = std::stoi(value);
    }
    catch (const std::invalid_argument &e)
    {
        std::cerr << "Invalid number of chunks: " << e.what() << std::endl;
        return 1; // Return error if conversion fails
    }
    catch (const std::out_of_range &e)
    {
        std::cerr << "Number of chunks out of range: " << e.what() << std::endl;
        return 1; // Return error if conversion fails
    }

    // Step 2: Delete each chunk
    for (int i = 1; i <= no_chunks; ++i)
    {
        F_2_B_Message delete_msg;
        delete_msg.type = 3; // DELETE request
        delete_msg.rowkey = rowkey;
        delete_msg.colkey = "content_" + std::to_string(i);

        result = send_msg_to_backend(fd, delete_msg, value, status, error_msg, rowkey, "content_" + std::to_string(i), g_map_rowkey_to_server, g_coordinator_addr, "delete");
        if (result != 0 || status != 0)
        {
            std::cerr << "Failed to delete chunk " << i << ": " << error_msg << std::endl;
            return 1; // Stop and return failure if any deletion fails
        }
    }

    std::cout << "All chunks deleted successfully." << std::endl;
    return 0; // Success
}

int copyChunks(int fd, const std::string &old_row_key, const std::string &new_row_key, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr)
{
    // Get no. of chunks
    string get_response_value, get_response_error_msg;
    int get_response_status, response_code;
    string type = "get";
    string rowkey = old_row_key;
    string colkey = "no_chunks";

    // GET request to retrieve no. of chunks
    F_2_B_Message msg_to_send_get = construct_msg(1, rowkey, colkey, "", "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send_get, get_response_value, get_response_status,
                                        get_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                        g_coordinator_addr, type);

    if (response_code == 1)
    {
        cerr << "ERROR in communicating with coordinator" << endl;
        return 1;
    }
    else if (response_code == 2)
    {
        cerr << "ERROR in communicating with backend" << endl;
        return 2;
    }

    if (get_response_status == 0)
    {
        int no_chunks = 0;
        try
        {
            no_chunks = std::stoi(get_response_value);
        }
        catch (const std::invalid_argument &e)
        {
            std::cerr << "Invalid number of chunks: " << e.what() << std::endl;
            return 1; // Return error if conversion fails
        }
        catch (const std::out_of_range &e)
        {
            std::cerr << "Number of chunks out of range: " << e.what() << std::endl;
            return 1; // Return error if conversion fails
        }

        // Copy each chunk
        for (int i = 1; i <= no_chunks; ++i)
        {
            rowkey = old_row_key;
            colkey = "content_" + std::to_string(i);

            // Get chunk content from old row
            msg_to_send_get = construct_msg(1, rowkey, colkey, "", "", "", 0);
            response_code = send_msg_to_backend(fd, msg_to_send_get, get_response_value, get_response_status,
                                                get_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);
            if (response_code == 1)
            {
                cerr << "ERROR in communicating with coordinator" << endl;
                return 1;
            }
            else if (response_code == 2)
            {
                cerr << "ERROR in communicating with backend" << endl;
                return 2;
            }

            if (get_response_status == 0)
            {
                // Put chunk content to new row
                string put_response_value, put_response_error_msg;
                int put_response_status;
                type = "put";
                rowkey = new_row_key;
                F_2_B_Message msg_to_send_put = construct_msg(2, new_row_key, colkey, get_response_value, "", "", 0);
                response_code = send_msg_to_backend(fd, msg_to_send_put, put_response_value, put_response_status,
                                                    put_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                    g_coordinator_addr, type);

                if (response_code == 1)
                {
                    cerr << "ERROR in communicating with coordinator" << endl;
                    return 1;
                }
                else if (response_code == 2)
                {
                    cerr << "ERROR in communicating with backend" << endl;
                    return 2;
                }

                if (put_response_status != 0)
                {
                    // Error handling
                    return put_response_status;
                }
                else
                {
                    return 0;
                }
            }
            else
            {
                // Error handling
                return get_response_status;
            }
        }
    }
    else
    {
        // Error handling
        return get_response_status;
    }
}