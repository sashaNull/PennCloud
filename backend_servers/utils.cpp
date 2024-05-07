#include "utils.h"
using namespace std;

// Maximum buffer size for data transmission
const int MAX_BUFFER_SIZE = 1024 * 100;
const int WELCOME_BUFFER_SIZE = 1024;

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

    // Increment the version number for the new save
    int current_version = checkpoint_tablet_data.tablet_version;
    int new_version = current_version + 1;

    // Construct file paths with version numbers
    std::string base_filename = tablet_name.substr(0, tablet_name.find_last_of('.'));
    std::string old_file_path = data_file_location + "/" + base_filename + "_" + std::to_string(current_version) + ".txt";
    std::string new_file_path = data_file_location + "/" + base_filename + "_" + std::to_string(new_version) + ".txt";

    // Create and open the new version of the file
    std::ofstream new_file(new_file_path);
    if (!new_file.is_open())
    {
        std::cerr << "Failed to open file: " << new_file_path << std::endl;
        return;
    }

    // Write the data to the new version of the file
    for (const auto &row : checkpoint_tablet_data.row_to_kv)
    {
        for (const auto &kv : row.second)
        {
            new_file << row.first << " " << kv.first << " " << kv.second << "\n";
        }
    }

    // Close the new file
    new_file.close();

    // Update the current version number
    checkpoint_tablet_data.tablet_version = new_version;

    // Remove the old version of the file if it exists
    if (std::remove(old_file_path.c_str()) != 0)
    {
        std::cerr << "Error deleting old file: " << old_file_path << std::endl;
    }
}

void checkpoint_tablet(tablet_data &checkpoint_tablet_data, string tablet_name, std::string data_file_location)
{
    string tablet_log_file = get_log_file_name(tablet_name);
    save_tablet(checkpoint_tablet_data, tablet_name, data_file_location);
    clearLogFile(data_file_location, tablet_log_file);
}

std::string get_new_file_name(const std::string &row_key, const std::vector<std::string> &server_tablet_list)
{
    if (row_key.length() < 2)
    {
        throw std::invalid_argument("Row key must be at least two characters long.");
    }

    // Extract first two characters of the row key
    std::string row_key_prefix = row_key.substr(0, 2);

    // Iterate over the list of tablet ranges
    for (const std::string &range : server_tablet_list)
    {
        if (range.length() < 5)
        {             // Must be at least "xx_yy"
            continue; // Skip invalid ranges
        }

        // Extract the range start and end substrings
        std::string range_start = range.substr(0, 2);
        std::string range_end = range.substr(3, 2);

        // Check if the row key prefix is within the range
        if (row_key_prefix >= range_start && row_key_prefix <= range_end)
        {
            return range; // Assuming you want to return the range as part of the file name
        }
    }

    // Default case if no match is found
    return server_tablet_list[0];
}

void load_cache(std::unordered_map<std::string, tablet_data> &cache, std::string data_file_location)
{
    // Iterate through each entry in the cache map
    for (auto &entry : cache)
    {
        entry.second.tablet_version = 0;
        // Create the base filename from entry.first by removing the extension
        std::string base_filename = entry.first.substr(0, entry.first.find_last_of('.'));

        // Directory reading objects
        DIR *dir;
        struct dirent *ent;
        std::string dir_path = data_file_location;
        bool file_found = false;

        // Open the directory
        if ((dir = opendir(dir_path.c_str())) != NULL)
        {
            // Read all the files and directories within the directory
            while ((ent = readdir(dir)) != NULL)
            {
                std::string file_name = std::string(ent->d_name);

                // Check if the file starts with the base_filename and ends with ".txt"
                if (file_name.find(base_filename + "_") == 0 && file_name.rfind(".txt") == file_name.length() - 4)
                {
                    file_found = true;

                    // Extract the version number from the filename
                    std::string version_str = file_name.substr(base_filename.length() + 1, file_name.rfind(".txt") - base_filename.length() - 1);
                    int version_number = std::stoi(version_str);

                    // Update the tablet version to the maximum version found
                    if (version_number > entry.second.tablet_version)
                    {
                        entry.second.tablet_version = version_number;
                    }

                    // Construct the full path to the file
                    std::string file_path = dir_path + "/" + file_name;

                    // Open the file
                    std::ifstream file(file_path);
                    std::string line;

                    // Read each line of the file
                    if (file.is_open())
                    {
                        while (getline(file, line))
                        {
                            std::stringstream ss(line);
                            std::string key, inner_key, value;

                            ss >> key >> inner_key;
                            std::getline(ss, value);

                            value = std::regex_replace(value, std::regex("^ +| +$|( ) +"), "$1");
                            entry.second.row_to_kv[key][inner_key] = value;
                        }
                        file.close();
                    }
                }
            }
            // Close the directory
            closedir(dir);
        }
        else
        {
            // Could not open directory
            std::cerr << "Could not open directory: " << dir_path << std::endl;
        }

        // If no files were found, create an initial file with version 0
        if (!file_found)
        {
            std::string initial_file_path = dir_path + "/" + base_filename + "_0.txt";
            std::ofstream new_file(initial_file_path);
            if (!new_file.is_open())
            {
                std::cerr << "Failed to create initial file: " << initial_file_path << std::endl;
            }
            else
            {
                // Optionally write some initial data or just close the file
                new_file.close();
                entry.second.tablet_version = 0; // Initialize the version to 0
            }
        }
    }
}

void replay_message(std::unordered_map<std::string, tablet_data> &cache, F_2_B_Message f2b_message, vector<string> &server_tablet_ranges)
{
    string tablet_name = get_new_file_name(f2b_message.rowkey, server_tablet_ranges);
    cout << "This row is in file: " << tablet_name << endl;

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

void recover(std::unordered_map<std::string, tablet_data> &cache, std::string &data_file_location, vector<string> &server_tablet_ranges)
{
    // Loop over the cache
    for (auto &entry : cache)
    {
        std::string base_filename = entry.first.substr(0, entry.first.find_last_of('.'));
        std::string versioned_filename = base_filename + "_" + std::to_string(entry.second.tablet_version);
        std::string log_file_name = get_log_file_name(base_filename);
        std::string full_log_file_path = data_file_location + "/" + log_file_name;
        std::ifstream log_file(full_log_file_path);
        entry.second.requests_since_checkpoint = 0;
        if (!log_file.is_open())
        {
            std::cerr << "Failed to open log file: " << full_log_file_path << std::endl;
            continue;
        }
        std::string line;
        while (std::getline(log_file, line))
        {
            if (!line.empty())
            {
                F_2_B_Message message = decode_message(line);
                replay_message(cache, message, server_tablet_ranges);
            }
        }
        log_file.close();
    }
}

char get_next_char(char start, int offset)
{
    char nextChar = char(start + offset);
    if (nextChar > 'z')
    {
        return 'z';
    }
    return nextChar;
}

vector<string> split_range(const string &range, int splits_per_char)
{
    char start = range[0];
    char end = range[2];
    int total_chars = end - start + 1;
    int offset_per_range = 26 / splits_per_char;

    vector<string> sub_ranges;
    for (char i = start; i <= end; i++)
    {
        char sub_start = 'a';
        while (sub_start <= 'z')
        {
            char sub_end = get_next_char(sub_start, offset_per_range - 1);

            string newRange = string(1, i) + sub_start + "_" + string(1, i) + sub_end;
            sub_ranges.push_back(newRange);
            sub_start = char(sub_end + 1);
        }
    }
    return sub_ranges;
}

void update_server_tablet_ranges(vector<string> &server_tablet_ranges)
{
    vector<string> new_ranges;
    for (const auto &range : server_tablet_ranges)
    {
        vector<string> subranges = split_range(range, NUM_SPLITS);
        new_ranges.insert(new_ranges.end(), subranges.begin(), subranges.end());
    }
    server_tablet_ranges = new_ranges; // Update the global variable with the new ranges
}
