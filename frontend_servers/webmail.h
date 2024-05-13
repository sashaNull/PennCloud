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

struct DisplayEmail {
    std::string uid;
    std::string to_from;
    std::string subject;
    std::string timestamp;
};

/**
 * Parses a string containing email details and converts it into a vector of DisplayEmail objects.
 * Each email detail is separated by ",", and within each email, fields are delimited by "##".
 * The email fields include UID, sender/recipient, subject, and timestamp, all in base64 encoded format.
 * 
 * @param emails_str A constant reference to a string containing the concatenated email data.
 * @return A vector of DisplayEmail objects, each representing an individual email.
 */
std::vector<DisplayEmail> parse_emails_str(const std::string& emails_str);


/**
 * Generates an HTML string that represents the inbox view of emails. The function first parses
 * the provided string of emails into a vector of DisplayEmail objects using the parse_emails_str function.
 * It then constructs HTML content which displays these emails in a table format with clickable rows
 * that redirect to individual email view pages. It includes CSS styles for layout and formatting,
 * and navigation buttons like Home and Logout.
 * 
 * @param emails_str A constant reference to a string containing the concatenated email data.
 * @return A string containing the complete HTML document for the inbox view.
 */
std::string generate_inbox_html(const std::string& emails_str);


/**
 * Generates an HTML string that represents the sent box view of emails. Similar to the inbox,
 * this function parses the email data, and creates an HTML representation suitable for the sent box.
 * The HTML includes styles and interaction elements like clickable rows, and navigation buttons
 * for user actions like composing new emails or returning to the inbox.
 * 
 * @param emails_str A constant reference to a string containing the concatenated email data.
 * @return A string containing the complete HTML document for the sent box view.
 */
std::string generate_sentbox_html(const std::string& emails_str);


/**
 * Replaces escaped newline characters (\\n) in a given input string with actual newline characters (\n).
 * This function iterates through the string and checks for the sequence '\\n'. If found, it replaces
 * this sequence with a newline character in the resultant string.
 * 
 * @param input A constant reference to a string containing the original text with escaped newlines.
 * @return A string with all escaped newlines replaced by actual newline characters.
 */
std::string replace_escaped_newlines(const std::string& input);


/**
 * Generates an HTML string for composing an email. This function constructs an HTML form with prefilled
 * values for the recipient (To), subject, and body of the email. It includes styles for the layout and 
 * a script to handle form submission asynchronously. Upon successful submission, the user is redirected 
 * to the inbox, or an error message is displayed.
 * 
 * @param prefill_to A string containing initial recipient(s) information.
 * @param prefill_subject A string containing initial subject information.
 * @param prefill_body A string containing initial body text of the email.
 * @return A string containing the complete HTML document for composing an email.
 */
std::string generate_compose_html(const std::string& prefill_to, const std::string& prefill_subject, const std::string& prefill_body);


/**
 * Parses a semicolon-separated string of email recipients into two separate vectors: one for local recipients
 * (those with a domain of 'localhost') and another for external recipients. It splits the input string by semicolons,
 * then further splits each recipient by '@' to separate the username and domain, and sorts them accordingly.
 * 
 * @param recipients_str A constant reference to a string containing semicolon-separated email addresses.
 * @return A vector of two vectors of strings; the first vector contains local recipients, and the second contains external recipients.
 */
std::vector<std::vector<std::string>> parse_recipients_str_to_vec(const std::string& recipients_str);

/**
 * Formats email content for display by organizing the subject, sender, timestamp, and body into a readable format.
 * It constructs a string with these components separated by newline characters for clarity.
 *
 * @param subject A reference to a string containing the subject of the email.
 * @param from A reference to a string indicating the sender of the email.
 * @param timestamp A reference to a string representing the time the email was sent.
 * @param body A reference to a string containing the body of the email.
 * @return A formatted string suitable for display, which includes the subject, sender, date, and body of the email.
 */
std::string format_mail_for_display(const std::string& subject, const std::string& from, const std::string& timestamp, const std::string& body);

/**
 * Retrieves the current local time formatted as a string. The format used is a common readable format
 * (e.g., "Mon Jan 01 23:59:59 2022"), suitable for timestamps in emails or logs.
 *
 * @return A string representing the current local time formatted according to the specified layout.
 */
std::string get_timestamp();

/**
 * Handles the delivery of an email to a local recipient's inbox. This function performs a series of network operations:
 * retrieving the current state of the inbox, attempting to append the new email, and managing potential conflicts 
 * through conditional put operations. It interacts with a backend server, handling both retrieval and update of 
 * mailbox data. The function includes detailed error handling and network communication statuses.
 *
 * @param recipient A string indicating the recipient's email address.
 * @param uid A unique identifier string for the email.
 * @param encoded_from A base64 encoded string representing the sender's email.
 * @param encoded_subject A base64 encoded string of the email's subject.
 * @param encoded_body A base64 encoded string of the email's body content.
 * @param encoded_display Unused parameter for future display options.
 * @param g_map_rowkey_to_server A reference to a map associating row keys with server information.
 * @param g_coordinator_addr A structure containing network address information of the coordinator.
 * @return An integer indicating the result of the delivery attempt: 0 for success, 1 for coordinator communication errors, 2 for backend communication errors.
 */
int deliver_local_email(const std::string& recipient, const std::string& uid, const std::string& encoded_from, const std::string& encoded_subject, const std::string& encoded_body, const std::string& encoded_display, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Attempts to add an email to the user's sent box by interacting with a backend server using network messages.
 * It first retrieves the current state of the sent box, then tries to append the new email data in a transaction-like
 * manner, using conditional put operations to handle concurrent modifications. Extensive error handling is included
 * to manage communication issues with both the coordinator and the backend.
 *
 * @param username A string representing the username whose sent box is being updated.
 * @param uid A string representing the unique identifier of the email.
 * @param encoded_to A base64 encoded string representing the recipient of the email.
 * @param encoded_ts A base64 encoded string representing the timestamp of the email.
 * @param encoded_subject A base64 encoded string representing the subject of the email.
 * @param encoded_body A base64 encoded string representing the body of the email.
 * @param g_map_rowkey_to_server A reference to a map associating row keys with server information.
 * @param g_coordinator_addr A structure containing network address information of the coordinator.
 * @return An integer status code: 0 for success, 1 for errors with the coordinator, 2 for errors with the backend.
 */
int put_in_sentbox(const std::string& username, const std::string& uid, const std::string& encoded_to, const std::string& encoded_ts, const std::string& encoded_subject, const std::string& encoded_body, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Converts newline characters in a given string to HTML <br> tags for web display. Each newline character
 * is replaced by "<br>", making the text suitable for HTML rendering.
 *
 * @param input A constant reference to a string that may contain newline characters.
 * @return A new string where each newline character has been replaced with "<br>".
 */
std::string newline_to_br(const std::string& input);

/**
 * Constructs an HTML document for displaying a single email in a web interface. It formats the subject,
 * sender, recipient, timestamp, and body for web viewing and adds interactive buttons for user actions
 * like replying, forwarding, and deleting the email. It ensures that email metadata and content are
 * appropriately escaped and formatted for safe and intuitive display.
 *
 * @param subject The subject of the email.
 * @param from The sender of the email.
 * @param to The recipient(s) of the email.
 * @param timestamp The timestamp when the email was sent.
 * @param body The body content of the email.
 * @param uid The unique identifier for the email.
 * @param source The source mailbox of the email (e.g., 'inbox' or 'sent').
 * @return A string containing the complete HTML document for viewing the email.
 */
std::string construct_view_email_html(const std::string& subject, const std::string& from, const std::string& to, const std::string& timestamp, const std::string& body, const std::string& uid, const std::string &source);

/**
 * Performs a comprehensive operation to store all parts of an email (from, to, timestamp, subject, body,
 * and display) in the backend storage using separate network requests for each part. Handles all communication
 * errors and retries as necessary, ensuring that all parts of the email are correctly stored.
 *
 * @param uid A unique identifier for the email.
 * @param encoded_from, encoded_to, encoded_ts, encoded_subject, encoded_body, encoded_display Base64 encoded strings representing various parts of the email.
 * @param g_map_rowkey_to_server A map that links row keys to server details for network communication.
 * @param g_coordinator_addr Network address details of the coordinator server.
 * @return An integer status code indicating the outcome (0 for success, 1 or 2 for different types of errors).
 */
int put_email_to_backend(const std::string &uid, const std::string &encoded_from, const std::string &encoded_to, const std::string &encoded_ts, const std::string &encoded_subject, const std::string &encoded_body, const std::string &encoded_display, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Removes a substring from a given string up to the next comma, inclusive. This function is useful for
 * modifying comma-separated lists in strings by removing specific elements.
 *
 * @param original The original string from which a substring will be removed.
 * @param substring The substring to locate within the original string. Removal starts from the beginning
 *                  of this substring and continues to the next comma or to the end of the string if no comma is found.
 * @return The modified string with the specified substring removed up to the next comma.
 */
std::string erase_to_comma(const std::string& original, const std::string& substring);

/**
 * Deletes an email from a user's mailbox by updating the relevant mailbox storage on the backend server.
 * This involves retrieving the current list of emails, removing the specified email, and then
 * conditionally putting the updated list back to ensure data consistency in the presence of concurrent modifications.
 * Error handling is robust, with specific responses for communication issues with the coordinator or backend.
 *
 * @param username A string representing the user whose mailbox is being modified.
 * @param uid A string representing the unique identifier of the email to be deleted.
 * @param source A string indicating the mailbox type (e.g., 'inbox' or 'sentbox').
 * @param g_map_rowkey_to_server A map linking row keys to server information for network communication.
 * @param g_coordinator_addr The network address configuration of the coordinator.
 * @return An integer status code: 0 for success, 1 or 2 for different types of errors.
 */
int delete_email(const std::string& username, const std::string& uid, const std::string& source, std::map<std::string, std::string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr);

/**
 * Closes a socket connection and logs the closure to standard output. This function is typically called to
 * clean up resources after network communications are completed.
 *
 * @param fd The file descriptor of the socket to be closed.
 */
void cleanup(int sock);

/**
 * Acts as an SMTP client that handles sending an email by resolving the recipient's mail server through DNS,
 * connecting via SMTP, and executing the necessary SMTP protocol commands. This function handles all aspects of
 * the email sending process, including DNS resolution, SMTP handshaking, and message transmission.
 *
 * @param arg A pointer to a map containing email fields such as recipient, sender, subject, and content.
 * @return A pointer to `nullptr` after completion, indicating the thread's operation is done.
 */
void* smtp_client(void* arg);

/**
 * Validates an email address against a standard email format using a regular expression. This function checks if
 * the email address conforms to common email patterns (e.g., user@example.com).
 *
 * @param email A string representing the email address to validate.
 * @return A boolean indicating whether the email address is valid according to the regex pattern.
 */
bool is_valid_email(const std::string& email);