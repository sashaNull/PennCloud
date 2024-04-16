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