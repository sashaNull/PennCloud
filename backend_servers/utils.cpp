#include "utils.h"
using namespace std;

// Maximum buffer size for data transmission
const int MAX_BUFFER_SIZE = 1024;

/**
 * Reads data from the client socket into the buffer.
 *
 * @param client_fd The file descriptor of the client socket.
 * @param client_buf The buffer to store the received data.
 * @return bool True if reading is successful, false otherwise.
 */
bool do_read(int client_fd, char *client_buf)
{
    size_t n = MAX_BUFFER_SIZE;
    size_t bytes_left = n;
    bool r_arrived = false;

    while (bytes_left > 0)
    {
        ssize_t result = read(client_fd, client_buf + n - bytes_left, 1);

        if (result == -1)
        {
            // Handle read errors
            if ((errno == EINTR) || (errno == EAGAIN))
            {
                continue; // Retry if interrupted or non-blocking operation
            }
            return false; // Return false for other errors
        }
        else if (result == 0)
        {
            return false; // Return false if connection closed by client
        }

        // Check if \r\n sequence has arrived
        if (r_arrived && client_buf[n - bytes_left] == '\n')
        {
            client_buf[n - bytes_left + 1] = '\0'; // Null-terminate the string
            break;                                 // Exit the loop
        }
        else
        {
            r_arrived = false;
        }

        // Check if \r has arrived
        if (client_buf[n - bytes_left] == '\r')
        {
            r_arrived = true;
        }

        bytes_left -= result; // Update bytes_left counter
    }

    client_buf[MAX_BUFFER_SIZE - 1] = '\0'; // Null-terminate the string
    return true;                            // Return true indicating successful reading
}

/**
 * Handles the GET operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message to be processed.
 * @return F_2_B_Message The processed F_2_B_Message containing the retrieved
 * value or error message.
 */
F_2_B_Message handle_get(F_2_B_Message message, string tablet_name, unordered_map<string, tablet_data> &cache)
{
    if (cache[tablet_name].row_to_kv.contains(message.rowkey))
    {
        if (cache[tablet_name].row_to_kv[message.rowkey].contains(message.colkey))
        {
            message.value =
                cache[tablet_name].row_to_kv[message.rowkey][message.colkey];
            message.status = 0;
            message.errorMessage.clear();
        }
        else
        {
            message.status = 1;
            message.errorMessage = "Colkey does not exist";
        }
    }
    else
    {
        message.status = 1;
        message.errorMessage = "Rowkey does not exist";
    }

    return message;
}

/**
 * Handles the PUT operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message containing data to be written.
 * @return F_2_B_Message The processed F_2_B_Message containing status and error
 * message.
 */
F_2_B_Message handle_put(F_2_B_Message message, string tablet_name, unordered_map<string, tablet_data> &cache)
{
    // std::cout << tablet_name << " " << message.rowkey << " " << message.colkey << " " << message.value;
    cache[tablet_name].row_to_kv[message.rowkey][message.colkey] = message.value;
    message.status = 0;
    message.errorMessage = "Data written successfully";
    return message;
}

/**
 * Handles the CPUT operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message containing data to be updated.
 * @return F_2_B_Message The processed F_2_B_Message containing status and error
 * message.
 */
F_2_B_Message handle_cput(F_2_B_Message message, string tablet_name, unordered_map<string, tablet_data> &cache)
{
    if (cache[tablet_name].row_to_kv.contains(message.rowkey))
    {
        if (cache[tablet_name].row_to_kv[message.rowkey].contains(message.colkey))
        {
            if (cache[tablet_name].row_to_kv[message.rowkey][message.colkey] ==
                message.value)
            {
                cache[tablet_name].row_to_kv[message.rowkey][message.colkey] =
                    message.value2;
                message.status = 0;
                message.errorMessage = "Colkey updated successfully";
            }
            else
            {
                message.status = 1;
                message.errorMessage = "Current value is not v1";
            }
        }
        else
        {
            message.status = 1;
            message.errorMessage = "Colkey does not exist";
        }
    }
    else
    {
        message.status = 1;
        message.errorMessage = "Rowkey does not exist";
    }

    // Return processed message
    return message;
}

/**
 * Handles the DELETE operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message containing data to be deleted.
 * @return F_2_B_Message The processed F_2_B_Message containing status and error
 * message.
 */
F_2_B_Message handle_delete(F_2_B_Message message, string tablet_name, unordered_map<string, tablet_data> &cache)
{
    if (cache[tablet_name].row_to_kv.contains(message.rowkey))
    {
        if (cache[tablet_name].row_to_kv[message.rowkey].erase(message.colkey))
        {
            message.status = 0;
            message.errorMessage = "Colkey deleted successfully";
        }
        else
        {
            message.status = 1;
            message.errorMessage = "Colkey does not exist";
        }
    }
    else
    {
        message.status = 1;
        message.errorMessage = "Rowkey does not exist";
    }

    // Return processed message
    return message;
}

void trim_trailing_whitespaces(string &str)
{
    // Find the last character that's not a whitespace
    auto it = find_if(str.rbegin(), str.rend(), [](unsigned char ch)
                      { return !isspace(ch); });

    // Erase trailing whitespaces
    str.erase(it.base(), str.end());
}

std::string get_log_file_name(const std::string &filename)
{
    // Find the position of the last occurrence of '.'
    size_t dot_position = filename.find_last_of('.');
    std::string name_without_extension = filename.substr(0, dot_position);

    // Append "_logs.txt" to the filename without extension
    return "logs/" + name_without_extension + "_logs.txt";
}

void log_message(const F_2_B_Message &f2b_message, string data_file_location, string tablet_name)
{
    // Serialize the message using the provided serialize function
    string serialized_message = encode_message(f2b_message);
    trim_trailing_whitespaces(serialized_message);

    // Construct the full path to the log file
    string log_file_path = data_file_location + "/" + get_log_file_name(tablet_name);

    // Open the log file in append mode
    ofstream log_file(log_file_path, ios::app);

    // Check if the file is open
    if (!log_file.is_open())
    {
        cerr << "Failed to open log file: " << log_file_path << endl;
        return;
    }

    // Write the serialized message to a new line in the log file
    log_file << serialized_message << endl;

    // Optionally check for write errors
    if (log_file.fail())
    {
        cerr << "Failed to write to log file: " << log_file_path << endl;
    }

    // Close the log file
    log_file.close();
}

void clearLogFile(const string &folderPath, string &fileName)
{
    string filePath = folderPath + "/" + fileName;
    ofstream ofs(filePath, ofstream::out | ofstream::trunc);
    ofs.close();
    cout << "Logs cleared successfully." << endl;
}

void save_tablet(tablet_data &checkpoint_tablet_data, const std::string &tablet_name, const std::string &data_file_location)
{
    // Ensure the directory exists
    mkdir(data_file_location.c_str(), 0777);

    // Construct file paths
    std::string temp_file_path = data_file_location + "/temp_" + tablet_name;
    std::string final_file_path = data_file_location + "/" + tablet_name;

    // Create and open the temporary file
    std::ofstream temp_file(temp_file_path);
    if (!temp_file.is_open())
    {
        std::cerr << "Failed to open file: " << temp_file_path << std::endl;
        return;
    }

    // Write the data to the temporary file
    for (const auto &row : checkpoint_tablet_data.row_to_kv)
    {
        for (const auto &kv : row.second)
        {
            temp_file << row.first << " " << kv.first << " " << kv.second << "\n";
        }
    }

    // Close the temporary file
    temp_file.close();

    // Rename the temporary file to the final file name
    if (std::rename(temp_file_path.c_str(), final_file_path.c_str()) != 0)
    {
        std::cerr << "Error renaming file from " << temp_file_path << " to " << final_file_path << std::endl;
    }
}

void checkpoint_tablet(tablet_data &checkpoint_tablet_data, string tablet_name, std::string data_file_location)
{
    string tablet_log_file = get_log_file_name(tablet_name);
    save_tablet(checkpoint_tablet_data, tablet_name, data_file_location);
    clearLogFile(data_file_location, tablet_log_file);
}

string get_file_name(string row_key)
{
    if (row_key.at(0) >= 'a' && row_key.at(0) <= 'c')
    {
        return "a_c.txt";
    }
    else if (row_key.at(0) >= 'd' && row_key.at(0) <= 'f')
    {
        return "d_f.txt";
    }
    else if (row_key.at(0) >= 'g' && row_key.at(0) <= 'i')
    {
        return "g_i.txt";
    }
    else if (row_key.at(0) >= 'j' && row_key.at(0) <= 'l')
    {
        return "j_l.txt";
    }
    else if (row_key.at(0) >= 'm' && row_key.at(0) <= 'o')
    {
        return "m_o.txt";
    }
    else if (row_key.at(0) >= 'p' && row_key.at(0) <= 'r')
    {
        return "p_r.txt";
    }
    else if (row_key.at(0) >= 's' && row_key.at(0) <= 'u')
    {
        return "s_u.txt";
    }
    else if (row_key.at(0) >= 's' && row_key.at(0) <= 'u')
    {
        return "v_x.txt";
    }
    else
    {
        return "y_z.txt";
    }
}

void load_cache(std::unordered_map<std::string, tablet_data> &cache, std::string data_file_location)
{
    for (auto &entry : cache)
    {
        ifstream file(data_file_location + "/" + entry.first);
        string line;

        while (getline(file, line))
        {
            stringstream ss(line);
            string key, inner_key, value;

            ss >> key >> inner_key >> value;
            entry.second.row_to_kv[key][inner_key] = value;
        }

        file.close();
    }
}

void replay_message(std::unordered_map<std::string, tablet_data> &cache, F_2_B_Message f2b_message)
{
    string tablet_name = get_file_name(f2b_message.rowkey);
    // cout << "This row is in file: " << tablet_name << endl;

    switch (f2b_message.type)
    {
    case 1:
        f2b_message = handle_get(f2b_message, tablet_name, cache);
        break;
    case 2:
        f2b_message = handle_put(f2b_message, tablet_name, cache);
        cache[tablet_name].requests_since_checkpoint++;
        break;
    case 3:
        f2b_message = handle_delete(f2b_message, tablet_name, cache);
        cache[tablet_name].requests_since_checkpoint++;
        break;
    case 4:
        f2b_message = handle_cput(f2b_message, tablet_name, cache);
        cache[tablet_name].requests_since_checkpoint++;
        break;
    default:
        cout << "Unknown command type received" << endl;
        break;
    }
}

void recover(std::unordered_map<std::string, tablet_data> &cache, std::string &data_file_location)
{
    // Loop over the cache
    for (auto &entry : cache)
    {
        // Generate log file name for each tablet
        std::string log_file_name = get_log_file_name(entry.first);

        // Construct the full path to the log file
        std::string full_log_file_path = data_file_location + "/" + log_file_name;

        // Open the log file
        std::ifstream log_file(full_log_file_path);
        if (!log_file.is_open())
        {
            std::cerr << "Failed to open log file: " << full_log_file_path << std::endl;
            continue;
        }

        // Read each line from the log file
        std::string line;
        while (std::getline(log_file, line))
        {
            if (!line.empty())
            {
                // Decode the message
                F_2_B_Message message = decode_message(line);
                // Replay the message
                replay_message(cache, message);
            }
        }

        // Close the log file
        log_file.close();
    }
}
