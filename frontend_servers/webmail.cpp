#include "./webmail.h"
#include "./backend_communication.h"
#include "../utils/utils.h"

using namespace std;

vector<DisplayEmail> parse_emails_str(const string &emails_str)
{
    vector<DisplayEmail> to_return;
    vector<string> email_strs_vector = split(emails_str, "&&&");
    for (const auto &email_str : email_strs_vector)
    {
        vector<string> email_fields = split(email_str, "##");
        DisplayEmail email_item;
        email_item.uid = email_fields[0];
        email_item.to_from = email_fields[1];
        email_item.subject = email_fields[2];
        email_item.timestamp = email_fields[3];
        to_return.push_back(email_item);
    }
    return to_return;
}

string generate_inbox_html(const string &emails_str)
{
    // "uid##sender##subject##timestamp&&&uid##sender##subject##timestamp"
    vector<DisplayEmail> emails = parse_emails_str(emails_str);
    stringstream html;
    html << "<!DOCTYPE html><html><head><title>Inbox</title><style>"
         << "table { width: 100%; }"
         << "td { padding: 5px; border-bottom: 1px solid #ddd; }"
         << "button { margin: 5px; padding: 10px 20px; font-size: 16px; cursor: pointer; }"
         << "</style></head><body>";

    // Navigation buttons
    html << "<div><button onclick=\"window.location.href='/sentbox'\">Sent</button>"
         << "<button onclick=\"window.location.href='/compose'\">New Email</button></div>";

    html << "<h1>Inbox</h1><table>";
    for (const auto &email : emails)
    {
        html << "<tr onclick=\"location.href='/view_email/?source=inbox&id=" << email.uid << "'\" style='cursor:pointer;'>";
        html << "<td>" << email.to_from << "</td>";
        html << "<td>" << email.subject << "</td>";
        html << "<td>" << email.timestamp << "</td>";
        html << "</tr>";
    }
    html << "</table></body></html>";
    return html.str();
}

string generate_sentbox_html(const string &emails_str)
{
    // "uid##recipient##subject##timestamp&&&uid##recipient##subject##timestamp"
    vector<DisplayEmail> emails = parse_emails_str(emails_str);
    stringstream html;
    html << "<!DOCTYPE html><html><head><title>Sent</title><style>"
         << "table { width: 100%; }"
         << "td { padding: 5px; border-bottom: 1px solid #ddd; }"
         << "button { margin: 5px; padding: 10px 20px; font-size: 16px; cursor: pointer; }"
         << "</style></head><body>";

    // Navigation buttons
    html << "<div><button onclick=\"window.location.href='/inbox'\">Inbox</button>"
         << "<button onclick=\"window.location.href='/compose'\">New Email</button></div>";

    html << "<h1>Sent</h1><table>";
    for (const auto &email : emails)
    {
        html << "<tr onclick=\"location.href='/view_email/?source=sentbox&id=" << email.uid << "'\" style='cursor:pointer;'>";
        html << "<td>" << email.to_from << "</td>";
        html << "<td>" << email.subject << "</td>";
        html << "<td>" << email.timestamp << "</td>";
        html << "</tr>";
    }
    html << "</table></body></html>";
    return html.str();
}

string replace_escaped_newlines(const string &input)
{
    string result;
    result.reserve(input.size());
    for (size_t i = 0; i < input.length(); ++i)
    {
        if (input[i] == '\\' && i + 1 < input.length() && input[i + 1] == 'n')
        {
            result += '\n';
            ++i;
        }
        else
        {
            result += input[i];
        }
    }
    return result;
}

string generate_compose_html(const string &prefill_to, const string &prefill_subject, const string &prefill_body)
{
    stringstream html;
    html << "<!DOCTYPE html><html><head><title>Compose Email</title><style>"
         << "input, textarea { width: 95%; margin: 10px; padding: 8px; }"
         << "textarea { height: 200px; }"
         << "</style>"
         << "<script>"
         << "document.addEventListener('DOMContentLoaded', function () {"
         << "    document.getElementById('emailForm').addEventListener('submit', function (event) {"
         << "        event.preventDefault();"
         << "        var formData = {to: document.getElementById('to').value,"
         << "            subject: document.getElementById('subject').value,"
         << "            body: document.getElementById('body').value"
         << "        };"
         << "        fetch('http://127.0.0.1:8000/compose', {"
         << "            method: 'POST',"
         << "            headers: { 'Content-Type': 'application/json' },"
         << "            body: JSON.stringify(formData)"
         << "        })"
         << "        .then(response => {"
         << "            if (response.ok) {"
         << "               window.location.href = 'http://127.0.0.1:8000/inbox';"
         << "               return;"
         << "            }"
         << "        })"
         << "        .catch((error) => {"
         << "            console.error('Error:', error);"
         << "        });"
         << "    });"
         << "});"
         << "</script>"
         << "</head><body>"
         << "<h1>Compose Email</h1>"
         << "<form id='emailForm'>"
         << "<label for='to'>To:</label><input type='text' id='to' name='to' placeholder='Enter recipients, separated by semicolons' value='" << prefill_to << "'><br>"
         << "<label for='subject'>Subject:</label><input type='text' id='subject' name='subject' placeholder='Subject' value='" << prefill_subject << "'><br>"
         << "<label for='body'>Body:</label><textarea id='body' name='body' placeholder='Write your email here...'>" << prefill_body << "</textarea><br>"
         << "<button type='submit'>Send Email</button>"
         << "</form></body></html>";

    return html.str();
}

vector<vector<string>> parse_recipients_str_to_vec(const string &recipients_str)
{
    cout << "recipients str: " << recipients_str << endl;
    vector<vector<string>> to_return;
    vector<string> local, external;
    to_return.push_back(local);
    to_return.push_back(external);
    vector<string> recipients = split(recipients_str, ";");
    for (const auto &r : recipients)
    {   
        cout << "recipient: " << r << endl;
        if (split(r, "@")[1] == "localhost")
        {
            // just push back the username
            cout << "in local if: " << split(r, "@")[0] << endl;
            to_return[0].push_back(split(r, "@")[0]);
        }
        else
        {
            cout << "in external if" << endl;
            to_return[1].push_back(r);
        }
    }
    return to_return;
}

string format_mail_for_display(const string &subject, const string &from, const string &timestamp, const string &body)
{
    return "Subject: " + subject + "\n" + "From: " + from + "\n" + "Date: " + timestamp + "\n\n" + body;
}

string get_timestamp()
{
    time_t current_time = time(nullptr);
    struct tm *local_time = localtime(&current_time);
    stringstream formatted_time;
    formatted_time << put_time(local_time, "%a %b %d %H:%M:%S %Y");
    return formatted_time.str();
}

int deliver_local_email(const string &backend_serveraddr_str, int fd, const string &recipient, const string &uid, const string &from, const string &subject, const string &encoded_body, const string &encoded_display)
{
    cout << "delivering to " << recipient << endl;
    F_2_B_Message msg_to_send = construct_msg(1, recipient + "_email", "inbox_items", "", "", "", 0);
    F_2_B_Message response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
    if (response_msg.status != 0) {
        cerr << response_msg.errorMessage << endl;
        return 1;
    }
    string usr_inbox_str = response_msg.value;
    // try cput until success
    string received_ts, to_cput;
    while (true)
    {
        // uid##sender##subject##timestamp&&&
        received_ts = get_timestamp();
        to_cput = uid + "##" + from + "##" + subject + "##" + received_ts + "&&&" + usr_inbox_str;
        msg_to_send = construct_msg(4, recipient + "_email", "inbox_items", usr_inbox_str, to_cput, "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        if (response_msg.status == 0)
        {
            break;
        }
        msg_to_send = construct_msg(1, recipient + "_email", "inbox_items", "", "", "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        usr_inbox_str = response_msg.value;
    }

    return response_msg.status;
}

void put_in_sentbox(const string &backend_serveraddr_str, int fd, const string &username, const string &uid, const string &to, const string &ts, const string &subject, const string &body)
{
    F_2_B_Message msg_to_send = construct_msg(1, username + "_email", "sentbox_items", "", "", "", 0);
    F_2_B_Message response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
    string usr_sentbox_str = response_msg.value;

    // try cput until success
    string received_ts, to_cput;
    while (true)
    {
        // uid##to##subject##timestamp&&&
        to_cput = uid + "##" + to + "##" + subject + "##" + ts + "&&&" + usr_sentbox_str;
        msg_to_send = construct_msg(4, username + "_email", "sentbox_items", usr_sentbox_str, to_cput, "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        if (response_msg.status == 0)
        {
            break;
        }
        msg_to_send = construct_msg(1, username + "_email", "sentbox_items", "", "", "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        usr_sentbox_str = response_msg.value;
    }
}

string newline_to_br(const string &input)
{
    string output;
    for (char ch : input) {
        if (ch == '\n') {
            output += "<br>";
        } else {
            output += ch;
        }
    }
    return output;
}

string construct_view_email_html(const string &subject, const string &from, const string &to, const string &timestamp, const string &body, const string &uid, const string &source)
{
    string formatted_body = newline_to_br(body);
    stringstream html;
    html << "<!DOCTYPE html><html><head><title>" << subject << "</title><style>"
         << "body { font-family: Arial, sans-serif; margin: 20px; }"
         << "h1 { color: #333; }"
         << "p { margin: 5px 0; }"
         << "label { font-weight: bold; }"
         << "button { margin: 10px; padding: 5px 10px; font-size: 16px; cursor: pointer; }"
         << "</style></head><body>"
         << "<h1>" << subject << "</h1>"
         << "<p><label>From:</label> " << from << "</p>"
         << "<p><label>To:</label> " << to << "</p>"
         << "<p><label>Date:</label> " << timestamp << "</p>"
         << "<h2>Message</h2>"
         << "<p>" << formatted_body << "</p>"
         << "<button onclick=\"window.location.href='/compose?mode=reply&email_id=" << uid << "'\">Reply</button>"
         << "<button onclick=\"window.location.href='/compose?mode=forward&email_id=" << uid << "'\">Forward</button>"
         << "<button onclick=\"if(confirm('Are you sure you want to delete this email from " << source << "?')) window.location.href='/delete_email?source=" << source << "&id=" << uid << "';\">Delete</button>";

    return html.str();
}

string delete_email_from_box_string(const string& input, const string& uid, const string& delimiter) {
    size_t uid_pos = input.find(uid);
    
    if (uid_pos == string::npos) {
        cout << "UID not found." << endl;
        return input;
    }

    size_t start_pos = input.rfind(delimiter, uid_pos);
    if (start_pos == string::npos) {
        start_pos = 0;
    }

    size_t end_pos = input.find(delimiter, uid_pos + uid.length());
    if (end_pos != string::npos) {
        end_pos += delimiter.length();
    } else {
        end_pos = input.length();
    }

    string modified_string = input;
    modified_string.erase(start_pos, end_pos - start_pos);

    return modified_string;
}


void delete_email(const string& backend_serveraddr_str, int fd, const string& username, const string& uid, const string& source) {
    F_2_B_Message msg_to_send = construct_msg(1, username + "_email", source + "_items", "", "", "", 0);
    F_2_B_Message response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
    string usr_box_str = response_msg.value;

    // try cput until success
    string received_ts, to_cput;
    while (true)
    {
        // uid##to##subject##timestamp&&&...
        to_cput = delete_email_from_box_string(usr_box_str, uid, "&&&");
        msg_to_send = construct_msg(4, username + "_email", source + "_items", usr_box_str, to_cput, "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        if (response_msg.status == 0)
        {
            break;
        }
        msg_to_send = construct_msg(1, username + "_email", source + "_items", "", "", "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        usr_box_str = response_msg.value;
    }
}

SSL_CTX* create_ssl_context() {
    const SSL_METHOD* method = TLS_client_method();
    if (!method) {
        std::cerr << "Unable to get SSL method" << std::endl;
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        std::cerr << "Unable to create SSL context" << std::endl;
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1); // Only allow TLSv1.2 and above
    return ctx;
}

void ssl_cleanup(SSL_CTX* ctx, int sock, SSL* ssl) {
    if (ssl) {
        SSL_free(ssl);
        std::cout << "SSL freed" << std::endl;
    }
    if (sock != -1) {
        close(sock);
        std::cout << "Socket closed" << std::endl;
    }
    if (ctx) {
        SSL_CTX_free(ctx);
        std::cout << "SSL context freed" << std::endl;
    }
    ERR_free_strings();
}

void send_smtp_command(SSL* ssl, const char* cmd) {
    std::cout << "Sending command: " << cmd;
    if (SSL_write(ssl, cmd, strlen(cmd)) <= 0) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to send command");
    }
    char buffer[1024];
    if (SSL_read(ssl, buffer, sizeof(buffer)) <= 0) {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to read response");
    }
    std::cout << "Received: " << buffer << std::endl;
}

void* smtp_client(void* arg) {
    auto data = static_cast<std::map<std::string, std::string>*>(arg);
    std::string to = (*data)["to"];
    std::string from = (*data)["from"];
    std::string subject = (*data)["subject"];
    std::string content = (*data)["content"];
    std::string domain = to.substr(to.find('@') + 1);
    delete data;  // Clean up data as it's no longer needed


    // Lookup MX record
    unsigned char query_buffer[NS_PACKETSZ];
    int response = res_query(domain.c_str(), ns_c_in, ns_t_mx, query_buffer, sizeof(query_buffer));
    if (response < 0) {
        std::cerr << "DNS query failed" << std::endl;
        return nullptr;
    }

    ns_msg handle;
    ns_initparse(query_buffer, response, &handle);
    ns_rr record;
    char mx_host[NS_MAXDNAME];
    if (ns_parserr(&handle, ns_s_an, 0, &record) == 0) {
        dn_expand(ns_msg_base(handle), ns_msg_end(handle), ns_rr_rdata(record) + NS_INT16SZ, mx_host, sizeof(mx_host));
        std::cout << "MX Record found: " << mx_host << std::endl;
    } else {
        std::cerr << "Failed to parse MX record" << std::endl;
        return nullptr;
    }

    // Resolve IP address of MX host
    std::cout << "Resolving IP address for: " << mx_host << std::endl;
    struct hostent* host = gethostbyname(mx_host);
    if (!host) {
        std::cerr << "Failed to resolve MX host: " << mx_host << std::endl;
        return nullptr;
    }
    struct in_addr* address = (struct in_addr*)host->h_addr;
    std::cout << "IP Address: " << inet_ntoa(*address) << std::endl;

    // Create socket and establish a connection
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(576);
    memcpy(&dest.sin_addr.s_addr, host->h_addr, host->h_length);

    std::cout << "Connecting to server: " << mx_host << " on port 576" << std::endl;
    if (connect(sock, (struct sockaddr*)&dest, sizeof(dest)) != 0) {
        std::cerr << "Failed to connect to the server: " << strerror(errno) << std::endl;
        ssl_cleanup(nullptr, sock, nullptr);
        return nullptr;
    }

    // Initialize SSL
    SSL_CTX* ctx = create_ssl_context();
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    if (SSL_connect(ssl) != 1) {
        std::cerr << "SSL connection failed" << std::endl;
        ssl_cleanup(ctx, sock, ssl);
        return nullptr;
    }

    std::cout << "SSL connection established" << std::endl;

    try {
        send_smtp_command(ssl, "HELO seas.upenn.edu\r\n");
        // send_smtp_command(ssl, ("MAIL FROM: <" + from + ">\r\n").c_str());
        send_smtp_command(ssl, "MAIL FROM: <mqjin@seas.upenn.edu>\r\n");
        send_smtp_command(ssl, ("RCPT TO: <" + to + ">\r\n").c_str());
        send_smtp_command(ssl, "DATA\r\n");
        send_smtp_command(ssl, ("Subject: " + subject + "\r\n" + content + "\r\n.\r\n").c_str());
        send_smtp_command(ssl, "QUIT\r\n");
    } catch (const std::exception& e) {
        std::cerr << "Error during SMTP communication: " << e.what() << std::endl;
        ssl_cleanup(ctx, sock, ssl);
        return nullptr;
    }

    ssl_cleanup(ctx, sock, ssl);

    return nullptr;
}
