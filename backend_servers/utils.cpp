#include "utils.h"
using namespace std;

// Maximum buffer size for data transmission
const int MAX_BUFFER_SIZE = 1024;

/**
 * Handles the GET operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message to be processed.
 * @return F_2_B_Message The processed F_2_B_Message containing the retrieved
 * value or error message.
 */
F_2_B_Message handle_get(F_2_B_Message message, string data_file_location)
{
    // Construct file path for the specified rowkey
    string file_path = data_file_location + "/" + message.rowkey + ".txt";

    // Open file for reading
    ifstream file(file_path);

    // Check if file exists and can be opened
    if (!file.is_open())
    {
        // If file does not exist, set error status and message
        message.status = 1;
        message.errorMessage = "Rowkey does not exist";
        return message;
    }

    // Iterate through file lines to find the specified colkey
    string line;
    bool keyFound = false;
    while (getline(file, line))
    {
        istringstream iss(line);
        string key, value;
        // Extract key-value pairs from the line
        if (getline(iss, key, ':') && getline(iss, value))
        {
            // Check if key matches the requested colkey
            if (key == message.colkey)
            {
                // If key found, set message value and flag
                message.value = value;
                keyFound = true;
                break;
            }
        }
    }

    // If requested colkey not found, set error status and message
    if (!keyFound)
    {
        message.status = 1;
        message.errorMessage = "Colkey does not exist";
    }
    else
    {
        // Otherwise, clear error message and set success status
        message.status = 0;
        message.errorMessage.clear();
    }

    // Close file and return processed message
    file.close();
    return message;
}

/**
 * Handles the PUT operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message containing data to be written.
 * @return F_2_B_Message The processed F_2_B_Message containing status and error
 * message.
 */
F_2_B_Message handle_put(F_2_B_Message message, string data_file_location)
{
    // Construct file path for the specified rowkey
    string file_path = data_file_location + "/" + message.rowkey + ".txt";

    // Open file for writing in append mode
    ofstream file(file_path, ios::app);

    // Check if file can be opened
    if (!file.is_open())
    {
        // If file cannot be opened, set error status and message
        message.status = 1;
        message.errorMessage = "Error opening file for rowkey";
        return message;
    }

    // Write data to file
    file << message.colkey << ":" << message.value << "\n";

    // Check for write errors
    if (file.fail())
    {
        // If error occurred while writing, set error status and message
        message.status = 1;
        message.errorMessage = "Error writing to file for rowkey";
    }
    else
    {
        // Otherwise, set success status and message
        message.status = 0;
        message.errorMessage = "Data written successfully";
    }

    // Close file and return processed message
    file.close();
    return message;
}

/**
 * Handles the CPUT operation for F_2_B_Messages.
 *
 * @param message The F_2_B_Message containing data to be updated.
 * @return F_2_B_Message The processed F_2_B_Message containing status and error
 * message.
 */
F_2_B_Message handle_cput(F_2_B_Message message, string data_file_location)
{
    // Construct file path for the specified rowkey
    string file_path = data_file_location + "/" + message.rowkey + ".txt";

    // Open file for reading
    ifstream file(file_path);

    // Check if file exists and can be opened
    if (!file.is_open())
    {
        // If file does not exist, set error status and message
        message.status = 1;
        message.errorMessage = "Rowkey does not exist";
        return message;
    }

    bool keyFound = false;
    bool valueUpdated = false;
    vector<string> lines;
    string line;

    // Read file line by line
    while (getline(file, line))
    {
        string key, value;
        stringstream lineStream(line);
        getline(lineStream, key, ':');
        getline(lineStream, value);

        // Check if colkey matches
        if (key == message.colkey)
        {
            keyFound = true;
            // Check if current value matches message value
            if (value == message.value)
            {
                // Update value if match found
                lines.push_back(key + ":" + message.value2);
                valueUpdated = true;
            }
            else
            {
                // Otherwise, keep the original line
                lines.push_back(line);
            }
        }
        else
        {
            // Keep the original line
            lines.push_back(line);
        }
    }
    file.close();

    // Check conditions for updating value
    if (keyFound && valueUpdated)
    {
        // If key and value updated successfully, rewrite file
        ofstream outFile(file_path, ios::trunc);
        for (const auto &l : lines)
        {
            outFile << l << endl;
        }
        outFile.close();

        // Set success status and message
        message.status = 0;
        message.errorMessage = "Value updated successfully";
    }
    else if (keyFound && !valueUpdated)
    {
        // If old value does not match, set error status and message
        message.status = 1;
        message.errorMessage = "Old value does not match";
    }
    else
    {
        // If colkey does not exist, set error status and message
        message.status = 1;
        message.errorMessage = "Colkey does not exist";
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
F_2_B_Message handle_delete(F_2_B_Message message, string data_file_location)
{
    // Construct file path for the specified rowkey
    string file_path = data_file_location + "/" + message.rowkey + ".txt";

    // Open file for reading
    ifstream file(file_path);

    // Check if file exists and can be opened
    if (!file.is_open())
    {
        // If file does not exist, set error status and message
        message.status = 1;
        message.errorMessage = "Rowkey does not exist";
        return message;
    }

    vector<string> lines;
    string line;
    bool keyFound = false;

    // Read file line by line
    while (getline(file, line))
    {
        string key;
        stringstream lineStream(line);
        getline(lineStream, key, ':');
        // If colkey doesn't match, keep the line
        if (key != message.colkey)
        {
            lines.push_back(line);
        }
        else
        {
            // If colkey matches, mark as found
            keyFound = true;
        }
    }
    file.close();

    // If colkey not found, set error status and message
    if (!keyFound)
    {
        message.status = 1;
        message.errorMessage = "Colkey does not exist";
        return message;
    }

    // Open file for writing (truncating previous content)
    ofstream outFile(file_path, ios::trunc);
    // Write remaining lines to file
    for (const auto &l : lines)
    {
        outFile << l << endl;
    }
    outFile.close();

    // Set success status and message
    message.status = 0;
    message.errorMessage = "Colkey deleted successfully";

    // Return processed message
    return message;
}

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

void createPrefixToFileMap(const string &directory_path, map<string, fileRange> &prefix_to_file)
{
    DIR *dir;
    struct dirent *ent;
    regex pattern(R"((\w+)_to_(\w+)\.txt)");
    smatch match;

    if ((dir = opendir(directory_path.c_str())) != nullptr)
    {
        while ((ent = readdir(dir)) != nullptr)
        {
            string filename = ent->d_name;
            // Check if filename matches the regex pattern
            if (regex_match(filename, match, pattern))
            {
                // Extract the parts of the filename
                string range_start = match[1];
                string range_end = match[2];

                // Populate the map
                prefix_to_file[range_end] = fileRange{range_start, range_end, filename};
            }
        }
        closedir(dir);
    }
    else
    {
        cerr << "Could not open directory" << endl;
    }
    return;
}

string standardizeRowName(const string &rowname)
{
    if (rowname.length() == 3)
    {
        return rowname;
    }
    else if (rowname.length() > 3)
    {
        return rowname.substr(0, 3);
    }
    else
    {
        return rowname + string(3 - rowname.length(), 'a');
    }
}

string findFileNameInRange(const map<string, fileRange> &prefix_to_file, const string &rowname)
{
    string standardizedRowName = standardizeRowName(rowname);
    for (const auto &entry : prefix_to_file)
    {
        const auto &range = entry.second;
        string standardizedStart = standardizeRowName(range.range_start);
        string standardizedEnd = standardizeRowName(range.range_end);
        if (standardizedStart <= standardizedRowName && standardizedRowName <= standardizedEnd)
        {
            return range.filename;
        }
    }
    return "";
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
    return name_without_extension + "_logs.txt";
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

void checkpoint_tablet(tablet_data &checkpoint_tablet_data, string tablet_name, std::string data_file_location)
{
    string tablet_log_file = get_log_file_name(tablet_name);
    // save_tablet();
    clearLogFile(data_file_location, tablet_log_file);
}