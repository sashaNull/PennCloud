#include <arpa/inet.h>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <map>
#include <unordered_map>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <algorithm>

#include "../utils/utils.h"
#include "./client_communication.h"
#include "./backend_communication.h"
#include "./webmail.h"
using namespace std;

bool verbose = false;
string g_coordinator_addr_str = "127.0.0.1:7070";
sockaddr_in g_coordinator_addr = get_socket_address(g_coordinator_addr_str);
string g_serveraddr_str;
int g_listen_fd;
std::unordered_map<std::string, std::string> g_endpoint_html_map;

std::vector<std::string> parse_items_list(const std::string &items_str)
{
  std::vector<std::string> items;
  std::istringstream iss(items_str);
  std::string item;
  while (std::getline(iss, item, ','))
  {
    items.push_back(item);
  }
  return items;
}

void sigint_handler(int signum)
{
  cout << "SIGINT received, shutting down." << endl;
  if (fd_is_open(g_listen_fd))
  {
    close(g_listen_fd);
  }
  exit(EXIT_SUCCESS);
}

void install_sigint_handler()
{
  if (signal(SIGINT, sigint_handler) == SIG_ERR)
  {
    cerr << "SIGINT handling in main thread failed" << endl;
    exit(EXIT_FAILURE);
  }
}

string parse_commands(int argc, char *argv[])
{
  int cmd;
  string ordering_str;
  while ((cmd = getopt(argc, argv, "o:v")) != -1)
  {
    switch (cmd)
    {
    case 'v':
      verbose = true;
      break;
    default: /* if other error, stderr */
      cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
      exit(EXIT_FAILURE);
    }
  }
  // get config file
  string config_file_path;
  if (argv[optind])
  {
    config_file_path = argv[optind];
  }
  else
  {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }
  if (config_file_path.empty())
  {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }
  if (!filepath_is_valid(config_file_path))
  {
    cerr << "Config file " << config_file_path << " is not valid." << endl;
    exit(EXIT_FAILURE);
  }
  // get number of servers
  ifstream file(config_file_path);
  string line;
  vector<string> lines;
  int line_count = 0;
  while (getline(file, line))
  {
    lines.push_back(line);
    line_count++;
  }
  // get server index
  optind++;
  int server_index;
  if (argv[optind])
  {
    server_index = stoi(argv[optind]);
  }
  else
  {
    cerr << "Syntax: " << argv[0] << " -v <config_file_name> <index>" << endl;
    exit(EXIT_FAILURE);
  }
  if (server_index < 0 || server_index > line_count - 1)
  {
    cerr << "Error: Server index " << server_index << " is not in valid range."
         << endl;
    exit(EXIT_FAILURE);
  }
  // return server address
  return lines[server_index];
}

void *handle_connection(void *arg)
{
  int client_fd = *static_cast<int *>(arg);
  delete static_cast<int *>(arg);
  // Receive the request
  const unsigned int BUFFER_SIZE = 4096;
  char buffer[BUFFER_SIZE];

  int fd = create_socket();

  // Keep listening for requests
  while (true)
  {
    cout << "Listening..." << endl;
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read == -1)
    {
      cerr << "Failed to read from socket." << endl;
      continue;
    }
    else if (bytes_read == 0)
    {
      cout << "Client closed the connection." << endl;
      break;
    }
    buffer[bytes_read] = '\0';
    string request(buffer);
    unordered_map<string, string> html_request_map = parse_http_request(request);

    // GET: rendering signup page
    if (html_request_map["uri"] == "/signup" && html_request_map["method"] == "GET")
    {
      // Retrieve HTML content from the map
      std::string html_content = g_endpoint_html_map["signup"];

      // Construct and send the HTTP response
      send_response(client_fd, 200, "OK", "text/html", html_content);
    }
    // POST: new user signup
    else if (html_request_map["uri"] == "/signup" && html_request_map["method"] == "POST")
    {
      // Parse out formData
      map<string, string> form_data = parse_json_string_to_map(html_request_map["body"]);
      string username = form_data["username"];

      // Convert username to lowercase
      transform(username.begin(), username.end(), username.begin(), ::tolower);

      // TODO: coordinator
      //  string backend_serveraddr_str = ask_coordinator(fd, g_coordinator_addr, username + "_info", 1);
      // check if user exists with get
      string backend_serveraddr_str = "127.0.0.1:6000";

      F_2_B_Message msg_to_send = construct_msg(1, username + "_info", "password", "", "", "", 0);
      F_2_B_Message get_response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);

      if (get_response_msg.status == 1 && strip(get_response_msg.errorMessage) == "Rowkey does not exist")
      {
        // Send new user data to backend and receive response
        string firstname = form_data["firstName"];
        string lastname = form_data["lastName"];
        string email = form_data["email"];
        string password = form_data["password"];

        msg_to_send = construct_msg(2, username + "_info", "firstName", firstname, "", "", 0);
        F_2_B_Message response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        if (response_msg.status != 0)
        {
          cerr << "Error in PUT to backend" << endl;
        }

        msg_to_send = construct_msg(2, username + "_info", "lastName", lastname, "", "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        if (response_msg.status != 0)
        {
          cerr << "Error in PUT to backend" << endl;
        }

        msg_to_send = construct_msg(2, username + "_info", "email", email, "", "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        if (response_msg.status != 0)
        {
          cerr << "Error in PUT to backend" << endl;
        }

        msg_to_send = construct_msg(2, username + "_info", "password", password, "", "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        if (response_msg.status != 0)
        {
          cerr << "Error in PUT to backend" << endl;
        }
        // if successful, ask browser to redirect to /login
        std::string redirect_to = "http://" + g_serveraddr_str + "/login";
        redirect(client_fd, redirect_to);
        // TODO: put user's fields for mail and drive
        msg_to_send = construct_msg(2, username + "_email", "inbox_items", "", "", "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        msg_to_send = construct_msg(2, username + "_email", "sentbox_items", "", "", "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
      }
      else if (get_response_msg.status == 0)
      {
        // error: user already exists - Construct and send the HTTP response
        std::string content = "{\"error\":\"User already exists\"}";
        send_response(client_fd, 409, "Conflict", "application/json", content);
      }
      else
      {
        // error: signup failed - Construct and send the HTTP response
        std::string content = "{\"error\":\"Signup Failed\"}";
        send_response(client_fd, 400, "Bad Request", "application/json", content);
      }
    }
    // GET: rendering login page
    else if (html_request_map["uri"] == "/login" && html_request_map["method"] == "GET")
    {
      // Retrieve HTML content from the map
      std::string html_content = g_endpoint_html_map["login"];

      // Construct and send the HTTP response
      send_response(client_fd, 200, "OK", "text/html", html_content);
    }
    // POST: user login
    else if (html_request_map["uri"] == "/login" && html_request_map["method"] == "POST")
    {
      // Parse out formData
      map<string, string> form_data = parse_json_string_to_map(html_request_map["body"]);
      string username = form_data["username"];
      string password = form_data["password"];

      // Convert username to lowercase
      transform(username.begin(), username.end(), username.begin(), ::tolower);

      // TODO: coordinator
      //  string backend_serveraddr_str = ask_coordinator(fd, g_coordinator_addr, username + "_info", 1);
      // Check if user exists
      string backend_serveraddr_str = "127.0.0.1:6000";

      F_2_B_Message msg_to_send = construct_msg(1, username + "_info", "password", "", "", "", 0);
      F_2_B_Message user_existence_response = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);

      if (user_existence_response.status == 1 && strip(user_existence_response.errorMessage) == "Rowkey does not exist")
      {
        // User does not exist - Construct and send the HTTP response
        string content = "{\"error\":\"User does not exist\"}";
        send_response(client_fd, 404, "Not Found", "application/json", content);
      }
      else if (user_existence_response.status == 0)
      {
        // User exists, check password
        string actual_password = user_existence_response.value;

        if (password == actual_password)
        {
          // Password matches, redirect to home page
          string redirect_to = "http://" + g_serveraddr_str + "/home";
          redirect(client_fd, redirect_to);
        }
        else
        {
          // Password is incorrect- Construct and send the HTTP response
          string content = "{\"error\":\"Incorrect password\"}";
          send_response(client_fd, 401, "Unauthorized", "application/json", content);
        }
      }
      else
      {
        // Error in checking user existence - Construct and send the HTTP response
        string content = "{\"error\":\"Error checking user existence\"}";
        send_response(client_fd, 500, "Internal Server Error", "application/json", content);
      }
    }
    // GET: rendering home page
    else if (html_request_map["uri"] == "/home" && html_request_map["method"] == "GET")
    {
      // Retrieve HTML content from the map
      std::string html_content = g_endpoint_html_map["home"];

      // Construct and send the HTTP response
      send_response(client_fd, 200, "OK", "text/html", html_content);
    }
    // GET: rendering reset-password page
    else if (html_request_map["uri"] == "/reset-password" && html_request_map["method"] == "GET")
    {
      // Retrieve HTML content from the map
      std::string html_content = g_endpoint_html_map["reset_password"];

      // Construct and send the HTTP response
      send_response(client_fd, 200, "OK", "text/html", html_content);
    }
    // POST: reset password
    else if (html_request_map["uri"] == "/reset-password" && html_request_map["method"] == "POST")
    {
      // Parse out formData
      map<string, string> form_data = parse_json_string_to_map(html_request_map["body"]);
      string username = form_data["username"];
      string oldPassword = form_data["oldPassword"];
      string newPassword = form_data["newPassword"];

      // Convert username to lowercase
      transform(username.begin(), username.end(), username.begin(), ::tolower);

      // TODO: coordinator
      //  string backend_serveraddr_str = ask_coordinator(fd, g_coordinator_addr, username + "_info", 1);
      // Check the response and parse accordingly
      string backend_serveraddr_str = "127.0.0.1:6000";

      F_2_B_Message msg_to_send = construct_msg(4, username + "_info", "password", oldPassword, newPassword, "", 0);
      F_2_B_Message reset_request_response = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);

      if (reset_request_response.status == 1 && strip(reset_request_response.errorMessage) == "Rowkey does not exist")
      {
        // User does not exist - Construct and send the HTTP response
        string content = "{\"error\":\"User does not exist\"}";
        send_response(client_fd, 404, "Not Found", "application/json", content);
      }
      else if (reset_request_response.status == 1 && strip(reset_request_response.errorMessage) == "Current value is not v1")
      {
        // Old password is wrong - Construct and send the HTTP response
        string content = "{\"error\":\"Current password is wrong!\"}";
        send_response(client_fd, 401, "Unauthorized", "application/json", content);
      }
      else if (reset_request_response.status == 0)
      {
        // Password reset successful, redirect to login page
        string redirect_to = "http://" + g_serveraddr_str + "/login";
        redirect(client_fd, redirect_to);
      }
      else
      {
        // Error in fetching user - Construct and send the HTTP response
        string content = "{\"error\":\"Error fetching user\"}";
        send_response(client_fd, 500, "Internal Server Error", "application/json", content);
      }
    }
    else if (html_request_map["uri"] == "/" && html_request_map["method"] == "GET")
    {
      string redirect_to = "http://" + g_serveraddr_str + "/login";
      redirect(client_fd, redirect_to);
    }
    // STARTMAIL
    else if (html_request_map["uri"] == "/inbox" && html_request_map["method"] == "GET")
    {
      F_2_B_Message msg_to_send, response_msg;
      string backend_serveraddr_str = "127.0.0.1:6000";
      // TODO: get username from cookie
      string username = "emmajin";
      msg_to_send = construct_msg(1, username + "_email", "inbox_items", "", "", "", 0);
      response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
      string inbox_emails_str = response_msg.value;
      string html_content = generate_inbox_html(inbox_emails_str);
      send_response(client_fd, 200, "OK", "text/html", html_content);
    }
    else if (html_request_map["uri"] == "/sentbox" && html_request_map["method"] == "GET")
    {
      F_2_B_Message msg_to_send, response_msg;
      string backend_serveraddr_str = "127.0.0.1:6000";
      // TODO: get username from cookie
      string username = "emmajin";
      msg_to_send = construct_msg(1, username + "_email", "sentbox_items", "", "", "", 0);
      response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
      string sentbox_emails_str = response_msg.value;
      string html_content = generate_sentbox_html(sentbox_emails_str);
      send_response(client_fd, 200, "OK", "text/html", html_content);
    }
    else if (html_request_map["uri"].substr(0, 8) == "/compose" && html_request_map["method"] == "GET")
    {
      // /compose?mode=reply&email_id=123
      F_2_B_Message msg_to_send, response_msg;
      // TODO:
      string username = "emmajin";
      string backend_serveraddr_str = "127.0.0.1:6000";
      string prefill_to = "";
      string prefill_subject = "";
      string prefill_body = "";
      if (html_request_map["uri"] != "/compose")
      {
        string mode, uid;
        vector<string> params = split(html_request_map["uri"].substr(9), "&");
        mode = split(params[0], "=")[1];
        uid = split(params[1], "=")[1];
        // look up email with uid in backend
        msg_to_send = construct_msg(1, username + "_email/" + uid, "subject", "", "", "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        string subject = response_msg.value;
        msg_to_send = construct_msg(1, username + "_email/" + uid, "body", "", "", "", 0);
        response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
        string body = response_msg.value;
        // if reply, prefill_to = from, prefill_subject = RE:subject, prefill_body = content
        if (mode == "reply")
        {
          msg_to_send = construct_msg(1, username + "_email/" + uid, "from", "", "", "", 0);
          response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
          prefill_to = response_msg.value;
          prefill_subject = "RE: " + subject;
          prefill_body = body;
        }
        else
        {
          // if forward, prefill_subject = FWD:subject, prefill body = content (with headers)
          prefill_subject = "FWD: " + subject;
          prefill_body = body;
        }
      }
      string html_content = generate_compose_html(prefill_to, prefill_subject, prefill_body);
      send_response(client_fd, 200, "OK", "text/html", html_content);
    }
    else if (html_request_map["uri"].substr(0, 8) == "/compose" && html_request_map["method"] == "POST")
    {
      cout << "uri: " << html_request_map["uri"] << endl;
      string ts_sentbox = get_timestamp();
      map<string, string> form_data = parse_json_string_to_map(html_request_map["body"]);
      string subject = form_data["subject"];
      string body = form_data["body"];
      cout << "subject: " << subject << endl;
      cout << "to: " << form_data["to"] << endl;
      cout << "body: " << body << endl;
      vector<vector<string>> recipients = parse_recipients_str_to_vec(form_data["to"]);
      // TODO: get from field from cookies
      string from = "emmajin@localhost";
      string from_username = "emmajin";
      // format to_display
      string for_display = format_mail_for_display(subject, from, form_data["to"], ts_sentbox, body);
      // compute uid of email
      string uid = compute_md5_hash(for_display);
      cout << "uid: " << uid << endl;
      string backend_serveraddr_str = "127.0.0.1:6000";
      // TODO: forward to SMTP client for each external recipient
      F_2_B_Message msg_to_send, response_msg;
      for (const auto &r : recipients[1])
      {
        // header: from, to, date, subject; + content
        // from || to || date || subject || content
      }
      // local recipients
      for (const auto &usrname : recipients[0])
      {
        // put in inbox
        cout << "local username: " << usrname << endl;
        deliver_local_email(backend_serveraddr_str, fd, usrname, uid, from, subject, body);
      }

      put_in_sentbox(backend_serveraddr_str, fd, from_username, uid, form_data["to"], ts_sentbox, subject, body);
      std::string redirect_to = "http://" + g_serveraddr_str + "/inbox";
      redirect(client_fd, redirect_to);
    }
    else if (html_request_map["uri"].substr(0, 11) == "/view_email" && html_request_map["method"] == "GET")
    {
      F_2_B_Message msg_to_send, response_msg;
      // TODO: get username from cookies
      string username = "emmajin";
      string backend_serveraddr_str = "127.0.0.1:6000";
      // view_email?id=xxx
      string uid = split(split(html_request_map["uri"], "?")[1], "=")[1];
      cout << "uid: " << uid << endl;
      // fetch email with corresponding uid (subject, from, to, timestamp, body) from backend
      msg_to_send = construct_msg(1, username + "_email/" + uid, "subject", "", "", "", 0);
      response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
      string subject = response_msg.value;
      msg_to_send = construct_msg(1, username + "_email/" + uid, "from", "", "", "", 0);
      response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
      string from = response_msg.value;
      msg_to_send = construct_msg(1, username + "_email/" + uid, "to", "", "", "", 0);
      response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
      string to = response_msg.value;
      msg_to_send = construct_msg(1, username + "_email/" + uid, "timestamp", "", "", "", 0);
      response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
      string timestamp = response_msg.value;
      msg_to_send = construct_msg(1, username + "_email/" + uid, "body", "", "", "", 0);
      response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
      string body = response_msg.value;
      // display the email content
      string html_content = construct_view_email_html(subject, from, to, timestamp, body, uid);
      send_response(client_fd, 200, "OK", "text/html", html_content);
      // reply / forward button should take us to pre-filled compose??
    }
    // ENDMAIL

    // GET: display drive page
    else if (html_request_map["uri"] == "/drive" && html_request_map["method"] == "GET")
    {
      std::string uri_path = html_request_map["uri"];

      // Extract path from the URI path
      std::string path = uri_path.substr(1 + uri_path.find_first_of("/"));

      // Construct the row key by appending "user_" to the foldername
      std::string row_key = "user_" + path;

      // Fetch items for the foldername from the backend
      string backend_serveraddr_str = "127.0.0.1:6000";
      F_2_B_Message msg_to_send = construct_msg(1, row_key, "items", "", "", "", 0);
      F_2_B_Message response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);

      if (response_msg.status == 0)
      {
        // Parse the response to get the list of items
        std::vector<std::string> items = parse_items_list(response_msg.value);

        // Check if the response contains a list of items (folders)
        if (!items.empty())
        {
          // Generate HTML for displaying folders and files
          std::stringstream html_content;
          html_content << "<ul>";

          for (const auto &item : items)
          {
            std::string item_type = item.substr(0, 2);
            std::string item_name = item.substr(2);

            if (item_type == "D@")
            {
              html_content << "<li><img src=\"folder.png\" alt=\"Folder\" width=\"20\" height=\"20\"> <a href=\"/" << path << "/" << item_name << "\">" << item_name << "</a></li>";
            }
            else if (item_type == "F@")
            {
              html_content << "<li><img src=\"file.png\" alt=\"File\" width=\"20\" height=\"20\"> <a href=\"/" << path << "/" << item_name << "\">" << item_name << "</a></li>";
            }
          }

          html_content << "</ul>";

          // Append upload file and create folder buttons
          html_content << "<button onclick=\"uploadFile()\">Upload File</button>";
          html_content << "<button onclick=\"createFolder()\">Create Folder</button>";

          // Generate JavaScript functions for upload and create folder actions
          html_content << "<script>";
          html_content << "function uploadFile() { /* Implement upload file logic */ }";
          html_content << "function createFolder() { /* Implement create folder logic */ }";
          html_content << "</script>";

          // Send the HTTP response with the generated HTML content
          send_response(client_fd, 200, "OK", "text/html", html_content.str());
        }
        else
        {
          // If the response is not a list, it is a file, so read the content and display it
          // You can implement this part based on your file content retrieval logic
        }
      }
      else
      {
        // Error handling: send appropriate error response to the client
        std::string error_message = "Failed to fetch items for folder ";
        send_response(client_fd, 500, "Internal Server Error", "text/plain", error_message);
      }
    }
    else
    {
      send_response(client_fd, 405, "Method Not Allowed", "text/html", "");
    }
  }
  close(client_fd);
  return nullptr;
}

int main(int argc, char *argv[])
{
  if (argc == 1)
  {
    cerr << "*** PennCloud: T15" << endl;
    exit(EXIT_FAILURE);
  }

  // install sigint handler
  install_sigint_handler();

  // read and save html files
  g_endpoint_html_map = load_html_files();

  // parse commands
  // <ip>:<port> string for future use
  g_serveraddr_str = parse_commands(argc, argv);
  cout << "IP: " << g_serveraddr_str << endl;
  sockaddr_in server_sockaddr = get_socket_address(g_serveraddr_str);

  // create listening socket
  g_listen_fd = socket(PF_INET, SOCK_STREAM, 0);
  cout << "Listening fd: " << g_listen_fd << endl;
  if (g_listen_fd == -1)
  {
    cerr << "Socket creation failed.\n"
         << endl;
    exit(EXIT_FAILURE);
  }
  int opt = 1;
  if (setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt)) < 0)
  {
    cerr << "Setting socket option failed.\n";
    close(g_listen_fd);
    exit(EXIT_FAILURE);
  }

  // bind listening socket
  if (bind(g_listen_fd, (struct sockaddr *)&server_sockaddr,
           sizeof(server_sockaddr)) != 0)
  {
    cerr << "Socket binding failed.\n"
         << endl;
    close(g_listen_fd);
    exit(EXIT_FAILURE);
  }

  // check listening socket
  if (listen(g_listen_fd, SOMAXCONN) != 0)
  {
    cerr << "Socket listening failed.\n"
         << endl;
    close(g_listen_fd);
    exit(EXIT_FAILURE);
  }

  // listen to messages from client (user)
  while (true)
  {
    sockaddr_in client_sockaddr;
    socklen_t client_socklen = sizeof(client_sockaddr);
    int client_fd = accept(g_listen_fd, (struct sockaddr *)&client_sockaddr,
                           &client_socklen);

    if (client_fd < 0)
    {
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
      {
        continue;
      }
      cerr << "Failed to accept new connection: " << strerror(errno) << endl;
      break;
    }

    pthread_t thd;
    pthread_create(&thd, nullptr, handle_connection, new int(client_fd));
    pthread_detach(thd);
  }

  close(g_listen_fd);

  return EXIT_SUCCESS;
}