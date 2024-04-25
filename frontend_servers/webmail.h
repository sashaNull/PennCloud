#include <string>
#include <vector>
#include <sstream>
#include <ctime>
#include <iomanip>

struct DisplayEmail {
    std::string uid;
    std::string to_from;
    std::string subject;
    std::string timestamp;
};

std::vector<DisplayEmail> parse_emails_str(const std::string& emails_str);

std::string generate_inbox_html(const std::string& emails_str);

std::string generate_sentbox_html(const std::string& emails_str);

std::string replace_escaped_newlines(const std::string& input);

std::string generate_compose_html(const std::string& prefill_to, const std::string& prefill_subject, const std::string& prefill_body);

std::vector<std::vector<std::string>> parse_recipients_str_to_vec(const std::string& recipients_str);

std::string format_mail_for_display(const std::string& subject, const std::string& from, const std::string& timestamp, const std::string& body);

std::string get_timestamp();

int deliver_local_email(const std::string& backend_serveraddr_str, int fd, const std::string& recipient, const std::string& uid, const std::string& from, const std::string& subject, const std::string& body);

void put_in_sentbox(const std::string& backend_serveraddr_str, int fd, const std::string& username, const std::string& uid, const std::string& to, const std::string& ts, const std::string& subject, const std::string& body);

std::string newline_to_br(const std::string& text);

std::string construct_view_email_html(const std::string& subject, const std::string& from, const std::string& to, const std::string& timestamp, const std::string& body, const std::string& uid, const std::string &source);

std::string delete_email_from_box_string(const std::string& input, const std::string& uid, const std::string& delimiter);

void delete_email(const std::string& backend_serveraddr_str, int fd, const std::string& username, const std::string& uid, const std::string& source);