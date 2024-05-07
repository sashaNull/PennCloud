#include "./backend_communication.h"
#include "../utils/utils.h"
#include "./bulletin.h"
#include <algorithm>

using namespace std;

BulletinMsg retrieve_bulletin_msg(int fd, const string &uid, map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr) {
    string response_value, response_error_msg;
    string rowkey, colkey, type;
    int response_status, response_code;
    F_2_B_Message msg_to_send;
    BulletinMsg msg;

    msg.uid = uid;

    rowkey = "bulletin/" + uid;
    type = "get";
    // get owner
    colkey = "owner";
    msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
    if (response_code == 1)
    {
    cerr << "ERROR in communicating with coordinator" << endl;
    }
    else if (response_code == 2)
    {
    cerr << "ERROR in communicating with backend" << endl;
    }
    msg.owner = base64_decode(response_value);

    // get timestamp
    colkey = "timestamp";
    msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
    if (response_code == 1)
    {
    cerr << "ERROR in communicating with coordinator" << endl;
    }
    else if (response_code == 2)
    {
    cerr << "ERROR in communicating with backend" << endl;
    }
    msg.timestamp = base64_decode(response_value);

    // get title
    colkey = "title";
    msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
    if (response_code == 1)
    {
    cerr << "ERROR in communicating with coordinator" << endl;
    }
    else if (response_code == 2)
    {
    cerr << "ERROR in communicating with backend" << endl;
    }
    msg.title = base64_decode(response_value);

    // get message
    colkey = "message";
    msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send, response_value, response_status,
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
    if (response_code == 1)
    {
    cerr << "ERROR in communicating with coordinator" << endl;
    }
    else if (response_code == 2)
    {
    cerr << "ERROR in communicating with backend" << endl;
    }       
    msg.message = base64_decode(response_value);

    return msg; 
}

vector<BulletinMsg> retrieve_my_bulletin(int fd, const string &uids, map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr)
{
    vector<BulletinMsg> to_return;
    // parse item uids (uid1, uid2, ....)
    vector<string> uids_vector = split(uids, ",");
    // for each uid, fetch (bulletin/uid timestamp, bulletin/uid title, bulletin/uid message)
    string response_value, response_error_msg;
    string rowkey, colkey, type;
    int response_status, response_code;
    F_2_B_Message msg_to_send;
    for (const auto &uid : uids_vector) 
    {
        BulletinMsg msg = retrieve_bulletin_msg(fd, uid, g_map_rowkey_to_server, g_coordinator_addr);
        to_return.push_back(msg); 
    }
    return to_return;
}


vector<BulletinMsg> retrieve_bulletin_board(int fd, const string &uids, map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr) {
    vector<BulletinMsg> to_return;
    // parse item uids (uid1, uid2, ....)
    vector<string> uids_vector = split(uids, ",");
    // for each uid, fetch (bulletin/uid timestamp, bulletin/uid title, bulletin/uid message)
    string response_value, response_error_msg;
    string rowkey, colkey, type;
    int response_status, response_code;
    F_2_B_Message msg_to_send;
    for (const auto &uid : uids_vector) 
    {
        BulletinMsg msg = retrieve_bulletin_msg(fd, uid, g_map_rowkey_to_server, g_coordinator_addr);
        to_return.push_back(msg); 
    }
    return to_return;    
}

string construct_bulletin_board_html(vector<BulletinMsg> msgs) {
    stringstream html;
    html << "<!DOCTYPE html><html><head><title>Bulletin Board</title><style>"
         << "body, html { height: 100%; margin: 0; overflow-y: auto; }"
         << "div.message { width: 30%; padding: 20px; margin: 10px; float: left; box-shadow: 0 4px 8px 0 rgba(0,0,0,0.2); }"
         << "button { margin: 20px; padding: 10px 20px; font-size: 16px; cursor: pointer; }"
         << "</style></head><body><button onclick=\"window.location.href='/my-bulletins'\">My Bulletins</button><div style='clear:both;'></div>";

    for (const auto &msg : msgs) {
        int r = rand() % 156 + 100;
        int g = rand() % 156 + 100;
        int b = rand() % 156 + 100;
        html << "<div class='message' style='background-color: rgb(" << r << "," << g << "," << b << ");'>"
             << "<h2>" << msg.title << "</h2>"
             << "<p><b>Owner:</b> " << msg.owner << "</p>"
             << "<p><b>Time:</b> " << msg.timestamp << "</p>"
             << "<p>" << msg.message << "</p>"
             << "</div>";
    }

    html << "<div style='clear:both;'></div></body></html>";
    return html.str();
}

string construct_my_bulletins_html(vector<BulletinMsg> msgs)
{
    stringstream html;
    html << "<!DOCTYPE html><html><head><title>My Bulletins</title><style>"
         << "div.message { border: 2px solid #ddd; margin: 10px; padding: 10px; background-color: #f9f9f9; }"
         << "button { margin: 5px; padding: 5px 10px; font-size: 14px; cursor: pointer; }"
         << "</style></head><body>"
         << "<h1>My Bulletin Board</h1>"
         << "<button onclick=\"window.location.href='/edit-bulletin'\">Add a New Message</button>"
         << "<button onclick=\"window.location.href='/bulletin'\">Back to Bulletin Board</button>";

    for (const auto &msg : msgs) {
        html << "<div class='message'>"
             << "<h2>" << msg.title << "</h2>"
             << "<p><b>Time:</b> " << msg.timestamp << "</p>"
             << "<p>" << msg.message << "</p>"
             << "<button onclick=\"window.location.href='/edit-bulletin?uid=" << msg.uid  << "'\">Edit</button>"
             << "<button onclick=\"window.location.href='/delete-bulletin?uid=" << msg.uid << "'\">Delete</button>"
             << "</div>";
    }

    html << "</body></html>";
    return html.str();
}


int put_bulletin_item_to_backend(const string &encoded_owner, const string &encoded_ts, const string &encoded_title, const string &encoded_msg, const string &uid, map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr)
{
    int fd = create_socket();
    string response_value, response_error_msg;
    string rowkey, colkey, type;
    int response_status, response_code;
    F_2_B_Message msg_to_send;

    rowkey = "bulletin/" + uid;
    type = "put";
    // put owner
    colkey = "owner";
    msg_to_send = construct_msg(2, rowkey, colkey, encoded_owner, "", "", 0);
    cout << "!!!putting owner to backend: " << encoded_owner << endl;
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

    // put timestamp
    colkey = "timestamp";
    msg_to_send = construct_msg(2, rowkey, colkey, encoded_ts, "", "", 0);
    cout << "!!!putting timestamp to backend: " << encoded_ts << endl;
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

    // put title
    colkey = "title";
    msg_to_send = construct_msg(2, rowkey, colkey, encoded_title, "", "", 0);
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

    // put message
    colkey = "message";
    msg_to_send = construct_msg(2, rowkey, colkey, encoded_msg, "", "", 0);
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
    close(fd);
    return 0;
}


int add_to_bulletin_board(int fd, const string &uid, map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr) {
    string bulletin_board_string, get_response_error_msg;
    F_2_B_Message msg_to_send;
    int get_response_status;
    string rowkey = "bulletin-board";
    string colkey = "items";
    string type = "get";
    msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
    int response_code = send_msg_to_backend(fd, msg_to_send, bulletin_board_string, get_response_status, 
                                            get_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
    if (response_code == 1) {
        cerr << "ERROR in communicating with coordinator" << endl;
        return 1;
    } else if (response_code == 2) {
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
        to_cput = uid + "," + bulletin_board_string;
        type = "cput";
        msg_to_send = construct_msg(4, rowkey, colkey, bulletin_board_string, to_cput, "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, bulletin_board_string, cput_response_status, 
                                            cput_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1) {
            cerr << "ERROR in communicating with coordinator" << endl;
            return 1;
        } else if (response_code == 2) {
            cerr << "ERROR in communicating with backend" << endl;
            return 2;
        }

        if (cput_response_status == 0)
        {
            break;
        }

        type = "get";
        msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, bulletin_board_string, get_response_status, 
                                            get_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1) {
            cerr << "ERROR in communicating with coordinator" << endl;
            return 1;
        } else if (response_code == 2) {
            cerr << "ERROR in communicating with backend" << endl;
            return 2;
        }
    }
    return 0;
}

int add_to_my_bulletins(int fd, const string &username, const string &uid, map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr) {
    // cput uid into (username_bulletin items)
    string my_bulletins_str, get_response_error_msg;
    F_2_B_Message msg_to_send;
    int get_response_status;
    string rowkey = username + "_bulletin";
    string colkey = "items";
    string type = "get";
    msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
    int response_code = send_msg_to_backend(fd, msg_to_send, my_bulletins_str, get_response_status, 
                                            get_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
    if (response_code == 1) {
        cerr << "ERROR in communicating with coordinator" << endl;
        return 1;
    } else if (response_code == 2) {
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
        to_cput = uid + "," + my_bulletins_str;
        type = "cput";
        msg_to_send = construct_msg(4, rowkey, colkey, my_bulletins_str, to_cput, "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, my_bulletins_str, cput_response_status, 
                                            cput_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1) {
            cerr << "ERROR in communicating with coordinator" << endl;
            return 1;
        } else if (response_code == 2) {
            cerr << "ERROR in communicating with backend" << endl;
            return 2;
        }

        if (cput_response_status == 0)
        {
            break;
        }

        type = "get";
        msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, my_bulletins_str, get_response_status, 
                                            get_response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1) {
            cerr << "ERROR in communicating with coordinator" << endl;
            return 1;
        } else if (response_code == 2) {
            cerr << "ERROR in communicating with backend" << endl;
            return 2;
        }
    }
    return 0;
}

string delete_uid_from_string(const string& original, const string& to_remove, const string& delimiter) {
    string result;
    size_t start = 0;
    size_t end = original.find(delimiter);
    
    while (end != string::npos) {
        string token = original.substr(start, end - start);
        if (token != to_remove) {
            if (!result.empty()) {
                result += delimiter;
            }
            result += token;
        }
        start = end + delimiter.length();
        end = original.find(delimiter, start);
    }

    string last_token = original.substr(start);
    if (last_token != to_remove) {
        if (!result.empty()) {
            result += delimiter;
        }
        result += last_token;
    }

    return result;
}


int delete_in_my_bulletin(int fd, const string &username, const string &uid, map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr) {
    string my_bulletin_str, response_error_msg;
    F_2_B_Message msg_to_send;
    int response_status, response_code;
    string type = "get";
    string rowkey = username + "_bulletin";
    string colkey = "items";
    msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send, my_bulletin_str, response_status, 
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
    if (response_code == 1) {
        cerr << "ERROR in communicating with coordinator" << endl;
        return 1;
    } else if (response_code == 2) {
        cerr << "ERROR in communicating with backend" << endl;
        return 2;
    }

    // try cput until success
    string received_ts, to_cput;
    while (true)
    {
        to_cput = delete_uid_from_string(my_bulletin_str, uid, ",");

        type = "cput";
        msg_to_send = construct_msg(4, rowkey, colkey, my_bulletin_str, to_cput, "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, my_bulletin_str, response_status, 
                                                response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);
        if (response_code == 1) {
            cerr << "ERROR in communicating with coordinator" << endl;
            return 1;
        } else if (response_code == 2) {
            cerr << "ERROR in communicating with backend" << endl;
            return 2;
        }

        if (response_status == 0)
        {
            break;
        }

        type = "get";
        msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, my_bulletin_str, response_status, 
                                                response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                                g_coordinator_addr, type);
        if (response_code == 1) {
            cerr << "ERROR in communicating with coordinator" << endl;
            return 1;
        } else if (response_code == 2) {
            cerr << "ERROR in communicating with backend" << endl;
            return 2;
        }
    }
    return 0;

}

int delete_in_bulletin_board(int fd, const string &uid, map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr) {
    string bulletin_board_str, response_error_msg;
    F_2_B_Message msg_to_send;
    int response_status, response_code;
    string type = "get";
    string rowkey = "bulletin-board";
    string colkey = "items";
    msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
    response_code = send_msg_to_backend(fd, msg_to_send, bulletin_board_str, response_status, 
                                        response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                        g_coordinator_addr, type);
    if (response_code == 1) {
        cerr << "ERROR in communicating with coordinator" << endl;
        return 1;
    } else if (response_code == 2) {
        cerr << "ERROR in communicating with backend" << endl;
        return 2;
    }

    // try cput until success
    string received_ts, to_cput;
    while (true)
    {
        to_cput = delete_uid_from_string(bulletin_board_str, uid, ",");

        type = "cput";
        msg_to_send = construct_msg(4, rowkey, colkey, bulletin_board_str, to_cput, "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, bulletin_board_str, response_status, 
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1) {
            cerr << "ERROR in communicating with coordinator" << endl;
            return 1;
        } else if (response_code == 2) {
            cerr << "ERROR in communicating with backend" << endl;
            return 2;
        }

        if (response_status == 0)
        {
            break;
        }

        type = "get";
        msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
        response_code = send_msg_to_backend(fd, msg_to_send, bulletin_board_str, response_status, 
                                            response_error_msg, rowkey, colkey, g_map_rowkey_to_server,
                                            g_coordinator_addr, type);
        if (response_code == 1) {
            cerr << "ERROR in communicating with coordinator" << endl;
            return 1;
        } else if (response_code == 2) {
            cerr << "ERROR in communicating with backend" << endl;
            return 2;
        }
    }
    return 0;
}


int delete_bulletin_item_from_backend(int fd, const string &uid, map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr)
{

    // delete bulletin/uid owner, timestamp, title, message
    string response_value, response_error_msg;
    string rowkey, colkey, type;
    int response_status, response_code;
    F_2_B_Message msg_to_send;

    rowkey = "bulletin/" + uid;
    type = "delete";
    // delete owner
    colkey = "owner";
    msg_to_send = construct_msg(3, rowkey, colkey, "", "", "", 0);
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

    // delete timestamp
    colkey = "timestamp";
    msg_to_send = construct_msg(3, rowkey, colkey, "", "", "", 0);
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

    // delete title
    colkey = "title";
    msg_to_send = construct_msg(3, rowkey, colkey, "", "", "", 0);
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

    // delete message
    colkey = "message";
    msg_to_send = construct_msg(3, rowkey, colkey, "", "", "", 0);
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

string construct_edit_bulletin_html(const string &prefill_title, const string &prefill_message)
{
    stringstream html;
    html << "<!DOCTYPE html><html><head><title>Compose Bulletin</title><style>"
         << "input, textarea { width: 95%; margin: 10px; padding: 8px; }"
         << "textarea { height: 200px; }"
         << "</style>"
         << "<script>"
         << "document.addEventListener('DOMContentLoaded', function () {"
         << "    document.getElementById('bulletinForm').addEventListener('submit', function (event) {"
         << "        event.preventDefault();"
         << "        var formData = {title: document.getElementById('title').value,"
         << "            message: document.getElementById('message').value};"
         << "        fetch('/edit-bulletin', {"
         << "            method: 'POST',"
         << "            headers: { 'Content-Type': 'application/json' },"
         << "            body: JSON.stringify(formData)"
         << "        })"
         << "        .then(response => {"
         << "            if (response.ok) {"
         << "               window.location.href = '/my-bulletins';"
         << "            }"
         << "        });"
         << "    });"
         << "});"
         << "</script>"
         << "</head><body>"
         << "<h1>Compose Bulletin</h1>"
         << "<form id='bulletinForm'>"
         << "<label for='title'>Title:</label><input type='text' id='title' name='title' placeholder='Enter title' value='" << prefill_title << "'><br>"
         << "<label for='message'>Message:</label><textarea id='message' name='message' placeholder='Write your message here...'>" << prefill_message << "</textarea><br>"
         << "<button type='submit'>Submit Message</button>"
         << "</form></body></html>";

    return html.str();
}

