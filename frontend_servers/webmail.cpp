#include "./webmail.h"
#include "./backend_communication.h"
#include "../utils/utils.h"

using namespace std;

vector<DisplayEmail> parse_emails_str(const string &emails_str)
{
    vector<DisplayEmail> to_return;
    vector<string> email_strs_vector = split(emails_str, ",");
    for (const auto &email_str : email_strs_vector)
    {
        vector<string> email_fields = split(email_str, "##");
        DisplayEmail email_item;
        email_item.uid = email_fields[0];
        email_item.to_from = base64_decode(email_fields[1]);
        email_item.subject = base64_decode(email_fields[2]);
        email_item.timestamp = base64_decode(email_fields[3]);
        to_return.push_back(email_item);
    }
    return to_return;
}

string generate_inbox_html(const string &emails_str)
{
    vector<DisplayEmail> emails = parse_emails_str(emails_str);
    stringstream html;
    html << "<!DOCTYPE html><html><head><title>Inbox</title><style>"
         << "body { font-family: Verdana, Geneva, Tahoma, sans-serif; margin: 20px; background-color: #e7cccb; color: #282525; }"
         << "h2 { text-align: center; background-color: #3737516b; padding: 10px; border-radius: 10px; margin: 20px 0; }"
         << "table { width: 100%; border-collapse: collapse; }"
         << "td { padding: 8px; border-bottom: 1.5px solid #ddd; }"
         << "tr:nth-child(odd) { background-color: #e9e9e9; }"
         << "tr:nth-child(even) { background-color: #d1d1d1; }"
         << "tr:hover { background-color: #ffffff; }"
         << "td:nth-child(1) { text-align: left; }"
         << "td:nth-child(2) { text-align: left; }"
         << "td:nth-child(3) { text-align: right; }"
         << "button { background-color: #161637; width: 120px ;color: white; border: none; padding: 10px 20px; border-radius: 20px; cursor: pointer; margin-right: 10px; margin-bottom: 10px;}"
         << "button:hover { background-color: #27274a; }"
         << "</style></head><body>";

    // Navigation buttons
    html << "<header style='width: 100%; display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px;'>"
         << "<div>"
         << "<button onclick=\"window.location.href='/home'\">Home</button>"
         << "</div>"
         << "<button onclick=\"window.location.href='/logout'\">Logout</button>"
         << "</header>";

    html << "<h2>Inbox</h2><table>"
         << "<div>"
         << "<button onclick=\"window.location.href='/sentbox'\">Sent</button>"
         << "<button onclick=\"window.location.href='/compose'\">New Email</button>"
         << "</div>";

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
         << "body { font-family: Verdana, Geneva, Tahoma, sans-serif; margin: 20px; background-color: #e7cccb; color: #282525; }"
         << "h2 { text-align: center; background-color: #3737516b; padding: 10px; border-radius: 10px; margin: 20px 0; }"
         << "table { width: 100%; border-collapse: collapse; }"
         << "td { padding: 8px; border-bottom: 1.5px solid #ddd; text-align: left; }"
         << "tr:nth-child(odd) { background-color: #e9e9e9; }" 
         << "tr:nth-child(even) { background-color: #d1d1d1; }"
         << "tr:hover { background-color: #ffffff; }"
         << "td:nth-child(1) { text-align: left; }"
         << "td:nth-child(2) { text-align: left; }"
         << "td:nth-child(3) { text-align: right; }"
         << "button { background-color: #161637; width: 120px; color: white; border: none; padding: 10px 20px; border-radius: 20px; cursor: pointer; margin-right: 10px; margin-bottom: 10px; }"
         << "button:hover { background-color: #27274a; }"
         << "</style></head><body>";

    // Navigation buttons
    html << "<header style='width: 100%; display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px;'>"
         << "<div>"
         << "<button onclick=\"window.location.href='/home'\">Home</button>"
         << "</div>"
         << "<button onclick=\"window.location.href='/logout'\">Logout</button>"
         << "</header>";

    html << "<h2>Sent</h2><table>"
         << "<div>"
         << "<button onclick=\"window.location.href='/inbox'\">Inbox</button>"
         << "<button onclick=\"window.location.href='/compose'\">New Email</button>"
         << "</div>";

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
         << "body { font-family: Verdana, Geneva, Tahoma, sans-serif; margin: 20px; background-color: #e7cccb; color: #282525; }"
         << "input, textarea { width: 95%; margin: 5px auto; display: block; box-sizing: border-box; padding: 8px; border: 1px solid #ccc; border-radius: 4px; }"
         << "textarea { height: 180px; }"
         << "button { background-color: #161637; width: 120px; color: white; border: none; padding: 10px 20px; border-radius: 20px; cursor: pointer; display: block; margin-bottom: 5px; }"
         << "button:hover { background-color: #27274a; }"
         << "header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }"
         << "h2 { text-align: center; background-color: #3737516b; padding: 10px; border-radius: 10px; }"
         << "</style>"
         << "<script>"
         << "document.addEventListener('DOMContentLoaded', function () {"
         << "    document.getElementById('emailForm').addEventListener('submit', function (event) {"
         << "        event.preventDefault();"
         << "        var formData = {to: document.getElementById('to').value,"
         << "            subject: document.getElementById('subject').value,"
         << "            body: document.getElementById('body').value"
         << "        };"
         << "        fetch('/compose', {"
         << "            method: 'POST',"
         << "            headers: { 'Content-Type': 'application/json' },"
         << "            body: JSON.stringify(formData)"
         << "        })"
         << "        .then(response => {"
         << "            if (response.ok) {"
         << "               window.location.href = '/inbox';"
         << "            } else {"
         << "               return response.json().then(data => {"
         << "                   if (data.error) {"
         << "                       throw new Error(data.error);"
         << "                   }"
         << "               });"
         << "           }"
         << "        })"
         << "        .catch((error) => {"
         << "            console.error('Error:', error);"
         << "            alert(error.message);"
         << "        });"
         << "    });"
         << "});"
         << "</script>"
         << "</head><body>"
         << "<header>"
         << "<button onclick=\"location.href='/home'\">Home</button>"
         << "<button onclick=\"location.href='/logout'\">Logout</button>"
         << "</header>"
         << "<h2>Compose Email</h2>"
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
            to_return[0].push_back(split(r, "@")[0]);
        }
        else
        {
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

// TODO: make helper function for deliver_local_mail and put_in_sentbox
int deliver_local_email(const string &recipient, const string &uid, const string &encoded_from,
                        const string &encoded_subject, const string &encoded_body, const string &encoded_display,
                        map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr)
{
    int fd = create_socket();
    string usr_inbox_str, get_response_error_msg;
    F_2_B_Message msg_to_send;
    int get_response_status;
    string rowkey = recipient + "_email";
    string colkey = "inbox_items";
    string type = "get";
    msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
    int response_code = send_msg_to_backend(fd, msg_to_send, usr_inbox_str, get_response_status,
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

    // try cput until success
    string received_ts, encoded_ts, to_cput;
    string get_response_value;
    string cput_response_value, cput_response_error_msg;
    int cput_response_status;
    while (true)
    {
        // uid##sender##subject##timestamp,
        received_ts = get_timestamp();
        encoded_ts = base64_encode(received_ts);
        to_cput = uid + "##" + encoded_from + "##" + encoded_subject + "##" + encoded_ts + "," + usr_inbox_str;

        type = "cput";
        msg_to_send = construct_msg(4, rowkey, colkey, usr_inbox_str, to_cput, "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, usr_inbox_str, cput_response_status,
                                            cput_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
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

        if (cput_response_status == 0)
        {
            break;
        }

        type = "get";
        msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, usr_inbox_str, get_response_status,
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
    }

    return 0;
}

int put_in_sentbox(const string &username, const string &uid, const string &encoded_to,
                   const string &encoded_ts, const string &encoded_subject, const string &encoded_body,
                   map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr)
{
    int fd = create_socket();

    string usr_sentbox_str, get_response_error_msg;
    F_2_B_Message msg_to_send;
    int get_response_status;
    string rowkey = username + "_email";
    string colkey = "sentbox_items";
    string type = "get";
    msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
    int response_code = send_msg_to_backend(fd, msg_to_send, usr_sentbox_str, get_response_status,
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

    // try cput until success
    string to_cput;
    string get_response_value;
    string cput_response_value, cput_response_error_msg;
    int cput_response_status;
    while (true)
    {
        // uid##to##subject##timestamp,
        to_cput = uid + "##" + encoded_to + "##" + encoded_subject + "##" + encoded_ts + "," + usr_sentbox_str;
        type = "cput";
        msg_to_send = construct_msg(4, rowkey, colkey, usr_sentbox_str, to_cput, "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, usr_sentbox_str, cput_response_status,
                                            cput_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
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

        if (cput_response_status == 0)
        {
            break;
        }

        type = "get";
        msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, usr_sentbox_str, get_response_status,
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
    }
    return 0;
}

string newline_to_br(const string &input)
{
    string output;
    for (char ch : input)
    {
        if (ch == '\n')
        {
            output += "<br>";
        }
        else
        {
            output += ch;
        }
    }
    return output;
}

string construct_view_email_html(const string &subject, const string &from, const string &to, const string &timestamp, const string &body, const string &uid, const string &source)
{
    string formatted_to = strip(to, "<>");
    string formatted_from = strip(from, "<>");
    string formatted_body = newline_to_br(body);
    stringstream html;
    html << "<!DOCTYPE html><html><head><title>" << subject << "</title><style>"
         << "body { font-family: Verdana, Geneva, Tahoma, sans-serif; margin: 20px; background-color: #e7cccb; color: #282525; }"
         << "header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }"
         << "h1 { text-align: left; font-size: 20px; margin-bottom: 5px; margin-left: 3px;}"
         << "p, label { font-family: Verdana, Geneva, Tahoma, sans-serif; margin: 5px 0; }"
         << "label { font-weight: bold; }"
         << ".subject { font-weight: bold; }"
         << ".message { background-color: #3737516b; border-radius: 10px; padding: 20px; margin-bottom: 20px; }"
         << ".message-header { padding-bottom: 10px; margin-bottom: 10px; }"
         << ".button-bar { display: flex; justify-content: center; gap: 20px; margin-bottom: 20px; }"
         << "button { background-color: #161637; color: white; border: none; padding: 10px 20px; border-radius: 20px; cursor: pointer; }"
         << "button:hover { background-color: #27274a; }"
         << "</style></head><body>"
         << "<header>"
         << "<button onclick=\"location.href='/home';\">Home</button>"
         << "<button onclick=\"location.href='/logout';\">Logout</button>"
         << "</header>"
         << "<h1>Subject: <span class='subject' style='font-weight: normal;'>" << subject << "</span></h1>"
         << "<div class='message'>"
         << "<div class='message-header'>"
         << "<p><label>From:</label> " << formatted_from << "</p>"
         << "<p><label>To:</label> " << formatted_to << "</p>"
         << "<p><label>Date:</label> " << timestamp << "</p>"
         << "</div>"
         << "<div class='message-line' style='border-bottom: 1px solid #282525; margin-bottom: 10px;'></div>"
         << "<p>" << formatted_body << "</p>"
         << "</div>"
         << "<div class='button-bar'>"
         << "<button onclick=\"window.location.href='/compose?mode=reply&email_id=" << uid << "'\">Reply</button>"
         << "<button onclick=\"window.location.href='/compose?mode=forward&email_id=" << uid << "'\">Forward</button>"
         << "<button onclick=\"if(confirm('Are you sure you want to delete this email from " << source << "?')) window.location.href='/delete_email?source=" << source << "&id=" << uid << "';\">Delete</button>"
         << "</div>";

    return html.str();
}

int put_email_to_backend(const string &uid, const string &encoded_from, const string &encoded_to, const string &encoded_ts,
                         const string &encoded_subject, const string &encoded_body, const string &encoded_display,
                         map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr)
{
    int fd = create_socket();
    string response_value, response_error_msg;
    F_2_B_Message msg_to_send;
    int response_status, response_code;
    string type = "put";
    string rowkey = "email/" + uid;

    // put from
    string colkey = "from";
    msg_to_send = construct_msg(2, rowkey, colkey, encoded_from, "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                        response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
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

    // put to
    colkey = "to";
    msg_to_send = construct_msg(2, rowkey, colkey, encoded_to, "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                        response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
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

    // put ts
    colkey = "timestamp";
    msg_to_send = construct_msg(2, rowkey, colkey, encoded_ts, "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                        response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
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

    // put subject
    colkey = "subject";
    msg_to_send = construct_msg(2, rowkey, colkey, encoded_subject, "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                        response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
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

    // put body
    colkey = "body";
    msg_to_send = construct_msg(2, rowkey, colkey, encoded_body, "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                        response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
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

    // put display
    colkey = "display";
    msg_to_send = construct_msg(2, rowkey, colkey, encoded_display, "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                        response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
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

    return 0;
}

std::string erase_to_comma(const std::string &original, const std::string &substring)
{
    std::string str = original; // Make a copy of the original string
    size_t start_pos = str.find(substring);
    if (start_pos != std::string::npos)
    {
        size_t end_pos = str.find(',', start_pos);
        if (end_pos != std::string::npos)
        {
            // Erase from start_pos to end_pos + 1 to include the comma
            str.erase(start_pos, end_pos - start_pos + 1);
        }
        else
        {
            // If no comma is found, erase until the end of the string
            str.erase(start_pos);
        }
    }
    return str;
}

int delete_email(const string &username, const string &uid,
                 const string &source, map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr)
{

    int fd = create_socket();
    string usr_box_str, response_error_msg;
    F_2_B_Message msg_to_send;
    int response_status, response_code;
    string type = "get";
    string rowkey = username + "_email";
    string colkey = source + "_items";
    msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send, usr_box_str, response_status,
                                        response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
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

    // try cput until success
    string received_ts, to_cput;
    while (true)
    {
        // to_cput = delete_email_from_box_string(usr_box_str, uid, ",");
        to_cput = erase_to_comma(usr_box_str, uid);

        type = "cput";
        msg_to_send = construct_msg(4, rowkey, colkey, usr_box_str, to_cput, "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, usr_box_str, response_status,
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
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

        if (response_status == 0)
        {
            break;
        }

        type = "get";
        msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, usr_box_str, response_status,
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
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
    }
    return 0;
}

void send_smtp_command(int fd, const char *cmd)
{
    std::cout << "Sending SMTP command: " << cmd;
    if (send(fd, cmd, strlen(cmd), 0) <= 0)
    {
        perror("Failed to send command");
        throw std::runtime_error("Failed to send command");
    }
    char buffer[1024] = {0}; // Clear buffer
    if (recv(fd, buffer, sizeof(buffer), 0) <= 0)
    {
        perror("Failed to read response");
        throw std::runtime_error("Failed to read response");
    }
    std::cout << "Received form SMTP server: " << buffer << std::endl;
}

void cleanup(int fd)
{
    if (fd != -1)
    {
        close(fd);
        std::cout << "Socket closed" << std::endl;
    }
}

void *smtp_client(void *arg)
{
    auto data = static_cast<std::map<std::string, std::string> *>(arg);
    std::string to = (*data)["to"];
    std::string from = (*data)["from"];
    std::string subject = (*data)["subject"];
    std::string content = (*data)["content"];
    std::string domain = to.substr(to.find('@') + 1);
    delete data;

    unsigned char query_buffer[NS_PACKETSZ];
    int response = res_query(domain.c_str(), ns_c_in, ns_t_mx, query_buffer, sizeof(query_buffer));
    if (response < 0)
    {
        std::cerr << "DNS query failed" << std::endl;
        return nullptr;
    }

    ns_msg handle;
    ns_initparse(query_buffer, response, &handle);
    int count = ns_msg_count(handle, ns_s_an);

    for (int i = 0; i < count; i++)
    {
        ns_rr record;
        if (ns_parserr(&handle, ns_s_an, i, &record) != 0)
        {
            std::cerr << "Failed to parse MX record" << std::endl;
            continue;
        }

        char mx_host[NS_MAXDNAME];
        dn_expand(ns_msg_base(handle), ns_msg_end(handle), ns_rr_rdata(record) + NS_INT16SZ, mx_host, sizeof(mx_host));

        struct addrinfo hints, *res, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET; // Use AF_INET6 to force IPv6
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(mx_host, NULL, &hints, &result) != 0)
        {
            std::cerr << "Failed to resolve MX host: " << mx_host << std::endl;
            continue;
        }
        else
        {
            cout << "Resolved MX host: " << mx_host << std::endl;
        }

        // Try each address until we successfully connect
        for (res = result; res != NULL; res = res->ai_next)
        {
            int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (fd == -1)
                continue;

            ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(25);

            if (connect(fd, res->ai_addr, res->ai_addrlen) == 0)
            {
                std::cout << "Connected to " << mx_host << std::endl;

                send_smtp_command(fd, ("HELO " + domain + "\r\n").c_str());
                send_smtp_command(fd, ("MAIL FROM: <" + from + ">\r\n").c_str());
                // send_smtp_command(fd, ("MAIL FROM: <mqjin@seas.upenn.edu>\r\n"));
                send_smtp_command(fd, ("RCPT TO: <" + to + ">\r\n").c_str());
                send_smtp_command(fd, "DATA\r\n");
                send_smtp_command(fd, ("Subject: " + subject + "\r\n" + content + "\r\n.\r\n").c_str());
                send_smtp_command(fd, "QUIT\r\n");

                cleanup(fd);
                freeaddrinfo(result);
                return nullptr;
            }
            else
            {
                cout << "Can't connect to " << mx_host << endl;
            }
            cleanup(fd);
        }
        freeaddrinfo(result);
    }

    std::cerr << "Failed to connect to any MX hosts" << std::endl;
    return nullptr;
}

bool is_valid_email(const string &email)
{
    const regex pattern("(\\w+)(\\.|_)?(\\w*)@(\\w+)(\\.(\\w+))+");
    return regex_match(email, pattern);
}