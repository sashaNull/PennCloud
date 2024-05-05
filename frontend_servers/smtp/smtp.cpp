#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <map>
#include <algorithm>
#include <cctype>
#include <fcntl.h>
#include <dirent.h>
#include <climits>
#include <fstream>
#include <vector>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <sys/file.h>
#include <netdb.h>

#include "../../utils/utils.h"
#include "./../client_communication.h"
#include "./../backend_communication.h"
#include "./../webmail.h"

using namespace std;

struct ThreadArgs {
  int* fd;
  bool debug_mode;
};

map<pthread_t, int> g_thread_args_map;

string g_coordinator_addr_str = "127.0.0.1:7070";
sockaddr_in g_coordinator_addr = get_socket_address(g_coordinator_addr_str);

map<string, string> g_map_rowkey_to_server;

void sigint_handler_smtp(int signum) {
  // Send SIGUSR signal to every user thread
  for (const auto& entry : g_thread_args_map) {
    pthread_t thread_id = entry.first;
    int fd = entry.second;
    if (fd_is_open(fd)) {
      string response = "421 Server shutting down\r\n";
      send(fd, response.c_str(), response.size(), 0);
    }
    if (pthread_kill(thread_id, 0) == 0) {
      if (pthread_kill(thread_id, SIGUSR1) != 0) {
        cerr << "thread with id " << thread_id << " doesn't exist!" << endl;
      }
    }
  }
  exit(EXIT_SUCCESS);
}

void install_sigint_handler_smtp() {
  if (signal(SIGINT, sigint_handler_smtp) == SIG_ERR) {
    cerr << "SIGINT handling in main thread failed" << endl;
    exit(EXIT_FAILURE);
  }
}

void sigusr_handler(int signum, siginfo_t *info, void *context) {
  pthread_t thread_id = pthread_self();
  int fd = g_thread_args_map[thread_id];
  if (fd_is_open(fd)) {
    if (close(fd) == -1) {
      cerr << "Failed to close file descriptor " << fd << endl;
    }
  }
  pthread_exit(NULL);
}

void register_sigint_mask() {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

void install_sigusr_handler() {
  struct sigaction sa;
  sa.sa_sigaction = sigusr_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGUSR1, &sa, NULL);
}

bool receiver_is_valid(const string& receiver) {
  // parse out username
  if (! (receiver[0] == '<' || receiver.back() == '>')) {
    return false;
  }
  string username;
  string domain;
  try {
    size_t delimiter_pos = receiver.find('@');
    size_t end_pos = receiver.find('>');
    if (delimiter_pos != string::npos) {
      username = receiver.substr(1, delimiter_pos - 1);
      domain = receiver.substr(delimiter_pos + 1, end_pos - delimiter_pos - 1);
    } else {
      return false;
    }
  } catch (const exception& e) {
    return false;
  }
  return true;
}

string get_receiver_username(const string& receiver) {
  size_t delimiter_pos = receiver.find('@');
  size_t end_pos = receiver.find('>');
  string username = receiver.substr(1, delimiter_pos-1);
  return username;
}

map<string, string> parse_mail_data(vector<string> maildata) {
  map<string, string> to_return;
  string body;
  bool found_subject = false;
  bool found_from = false;
  string subject_prefix = "Subject:";
  string from_prefix = "From:";
  bool body_started = false;
  for (const auto &line : maildata) {
        // Check if the string starts with "Subject: "
    if (line.find(subject_prefix) == 0 && found_subject == false) {
        // Extract the part of the string after "Subject: "
      to_return["subject"] = strip(line.substr(subject_prefix.length()));
    } 
    else if (line.find(from_prefix) == 0 && found_from == false) {
      to_return["from"] = strip(line.substr(from_prefix.length()));
    }
    else if (line == "\r\n" && body_started == false) {
      body_started = true;
    } else if (body_started) {
      body += strip(line) + "\n";
    }
  }
  to_return["body"] = body;
  return to_return;
}

string flatten_vector(vector<string> to_flatten) {
  string to_return;
  for (const auto &line : to_flatten) {
    to_return += line;
  }
  return to_return;
}

void read_and_handle_data(int* fd, bool debug_mode) {
  int state_num = -1;
  char buffer[100];
  size_t buffer_length = 0;
  bool quit = false;
  string mail_from; // external user
  vector<string> mail_to; // local users
  vector<string> mail_data;
  int backend_fd = create_socket();
  // # keep repeating:
  while (true) {
    if (quit) {
      break;
    }
    // ## read from client
    ssize_t bytes_read = read(*fd, buffer + buffer_length, 100 - buffer_length);
    if (bytes_read <= 0) {
      cerr << "Error when reading from client " << *fd << endl;
      break;
    }
    buffer_length += bytes_read;
    // ## check if there is <CRLF> in the buffer
    string data(buffer, buffer_length);
    size_t pos = data.find("\r\n");
    while (pos != string::npos) {
      // ## if yes, extract command until <CLRF> (including)
      string command = data.substr(0, pos + 2);
      string command_to_check = lower_case(command);
      if (debug_mode) {
        cerr << "[" << *fd << "] C: " << command << endl;
      }
      // 3 -> 0: reading email data
      if (state_num == 3) {
        if (command_to_check == ".\r\n") {
          // for each recipient...
          string ts = get_timestamp();
          string uid = compute_md5_hash(flatten_vector(mail_data));
          map<string, string> maildata_map = parse_mail_data(mail_data);
          string from = maildata_map["from"];
          string subject = maildata_map["subject"];
          string encoded_subject = base_64_encode(reinterpret_cast<const unsigned char*>(subject.c_str()), subject.length());
          string body = maildata_map["body"];
          string encoded_body = base_64_encode(reinterpret_cast<const unsigned char*>(body.c_str()), body.length());
          string for_display = format_mail_for_display(subject, from, ts, body);
          string encoded_display = base_64_encode(reinterpret_cast<const unsigned char*>(for_display.c_str()), for_display.length());
          bool atleast_one_sent = false;
          for (const auto& recipient : mail_to) {
            string username = get_receiver_username(recipient);
            cout << "SMTP | recipient: " << recipient << " | username: " << username;
            if (deliver_local_email(username, uid, from, encoded_subject, encoded_body, encoded_display, 
                                    g_map_rowkey_to_server, g_coordinator_addr) == 0) {
              atleast_one_sent = true;
            }
          }
          string to = flatten_vector(mail_to);
          put_email_to_backend(uid, from, to, ts, encoded_subject, encoded_body, 
                              encoded_display, g_map_rowkey_to_server, g_coordinator_addr);
          // send response
          string response;
          if (atleast_one_sent) {
            response = "250 OK\r\n";
          } else {
            response = "450 Requested mail action not taken: mailbox unavailable\r\n";
          }
          send(*fd, response.c_str(), response.size(), 0);
          if (debug_mode) {
            cerr << "[" << *fd << "] S: " << response << endl;
          }
          // clear mail data
          mail_data.clear();
          state_num = 0;
        } else {
          mail_data.push_back(command);
        }
      }
      // 0: HELO <domain>
      else if (command_to_check.substr(0, 4) == "helo") {
        string client_domain = strip(command_to_check.substr(4));
        if (client_domain.empty()) {
          string error_msg = "500 Client domain for HELO cannot be null\r\n";
          send(*fd, error_msg.c_str(), error_msg.size(), 0);
        } else {
          string server_domain = "localhost";
          string response = "250 " + server_domain + "\r\n";
          send(*fd, response.c_str(), response.size(), 0);
          if (state_num < 0) {
            state_num = 0;
          }
        }
      // 1: MAIL FROM:
      } else if (command_to_check.substr(0, 10) == "mail from:") {
        if (state_num > 1 || 1 - state_num > 1) {
          string error_msg = "503 Bad sequence of commands\r\n";
          send(*fd, error_msg.c_str(), error_msg.size(), 0);
        } else {
          mail_from = strip(command_to_check.substr(10));
          if (mail_from.empty()) {
            string error_msg = "553 Mail cannot be from empty domain\r\n";
            send(*fd, error_msg.c_str(), error_msg.size(), 0);
          } else {
            string response = "250 OK\r\n";
            send(*fd, response.c_str(), response.size(), 0);
            if (debug_mode) {
              cerr << "[" << *fd << "] S: " << response << endl;
            }
            state_num = 1;
          }
        }
      // 2: RCPT TO:
      } else if (command_to_check.substr(0, 8) == "rcpt to:") {
        if (state_num > 2 || 2 - state_num > 1) {
          string error_msg = "503 Bad sequence of commands\r\n";
          send(*fd, error_msg.c_str(), error_msg.size(), 0);
        } else {
          string rcpt_to = strip(command_to_check.substr(8));
          if (rcpt_to.empty()) {
            string error_msg = "553 Receiver cannot be an empty domain\r\n";
            send(*fd, error_msg.c_str(), error_msg.size(), 0);
          } else if (!receiver_is_valid(rcpt_to)) {
            string error_msg = "550 Requested action not taken: mailbox unavailable\r\n";
            send(*fd, error_msg.c_str(), error_msg.size(), 0);
          } else {
            mail_to.push_back(rcpt_to);
            string response = "250 OK\r\n";
            send(*fd, response.c_str(), response.size(), 0);
            if (debug_mode) {
              cerr << "[" << *fd << "] S: " << response << endl;
            }
            state_num = 2;
          }
        }
      // 3: DATA
      } else if (strip(command_to_check) == "data") {
        if (state_num > 3 || 3 - state_num > 1) {
          string error_msg = "503 Bad sequence of commands\r\n";
          send(*fd, error_msg.c_str(), error_msg.size(), 0);
        } else {
          mail_data.clear();
          string response = "354 Start mail input; end with <CRLF>.<CRLF>\r\n";
          send(*fd, response.c_str(), response.size(), 0);
          if (debug_mode) {
            cerr << "[" << *fd << "] S: " << response << endl;
          }
          state_num = 3;
        }
      // RSET
      } else if (command_to_check.substr(0, 4) == "rset") {
        if (state_num < 0) {
          string error_msg = "503 Bad sequence of commands\r\n";
          send(*fd, error_msg.c_str(), error_msg.size(), 0);
        } else {
          // send success response
          string response = "250 OK\r\n";
          send(*fd, response.c_str(), response.size(), 0);
          if (debug_mode) {
            cerr << "[" << *fd << "] S: " << response << endl;
          }
          // set state to write after successful HELO
          state_num = 0;
        }
      // NOOP
      } else if (command_to_check.substr(0, 4) == "noop") {
        if (state_num < 0) {
          string error_msg = "503 Bad sequence of commands\r\n";
          send(*fd, error_msg.c_str(), error_msg.size(), 0);
        } else {
          // send success response
          string response = "250 OK\r\n";
          send(*fd, response.c_str(), response.size(), 0);
          if (debug_mode) {
            cerr << "[" << *fd << "] S: " << response << endl;
          }
        }
      // QUIT
      } else if (command_to_check.substr(0, 4) == "quit") {
        if (state_num < 0) {
          string error_msg = "503 Bad sequence of commands\r\n";
          send(*fd, error_msg.c_str(), error_msg.size(), 0);
        } else {
          string server_domain = "localhost";
          string response = "221 " + server_domain + " Service closing transmission channel\r\n";
          send(*fd, response.c_str(), response.size(), 0);
          if (debug_mode) {
            cerr << "[" << *fd << "] S: " << response << endl;
          }
          quit = true;
          break;
        }
      } else {
        string error_msg = "500 Syntax error, command unrecognized\r\n";
        send(*fd, error_msg.c_str(), error_msg.size(), 0);
      }
      // ## remove command from buffer
      copy(buffer + pos + 2, buffer + 100, buffer);
      buffer_length -= pos + 2;
      data = string(buffer, buffer_length);
      pos = data.find("\r\n");
    }
  }
}

void* thread_function(void* arg) {
  ThreadArgs* args = static_cast<ThreadArgs*>(arg);
  int *fd = args->fd;
  bool debug_mode = args->debug_mode;
  // sigmask for SIGINT
  register_sigint_mask();
  // Install signal handler for the thread
  install_sigusr_handler();
  // send greeting message
  string server_domain = "localhost";
  string greeting = "220 " + server_domain + " Server is ready.\r\n";
  send(*fd, greeting.c_str(), greeting.size(), 0);
  // read message from client
  read_and_handle_data(fd, debug_mode);
  close(*fd);
  if (debug_mode) {
    cerr << "[" << *fd << "] Connection closed" << endl;
  }
  delete fd;
  pthread_exit(NULL);
}

int create_listening_socket(int port_number) {
  int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (listen_fd == -1) {
      cerr << "socket creation failed" << endl;
      exit(1);
  }
  int opt = 1;
  int ret = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
  // define server address
  struct sockaddr_in servaddr = {};
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htons(INADDR_ANY);
  servaddr.sin_port = htons(port_number);
  // bind server address to socket
  if (bind(listen_fd, reinterpret_cast<struct sockaddr*>(&servaddr), sizeof(servaddr)) == -1) {
    cerr << "bind failed" << endl;
    close(listen_fd);
    exit(1);
  }
  // set socket to listening mode
  if (listen(listen_fd, 1000) == -1) {
    cerr << "listen failed" << endl;
    close(listen_fd);
    exit(1);
  }
  return listen_fd;
}

void parse_commands(int &port_number, bool &debug_mode, bool &relay_mode, int argc, char *argv[]) {
  int cmd;
  while ((cmd = getopt (argc, argv, "p:av")) != -1) {
    switch (cmd) {
      case 'p':
        try {
          port_number = stoi(optarg);
          if (port_number < 0) {
            cerr << "port number cannot be negative!" << endl;
            exit(1);
          }
          break;
        } catch (invalid_argument&) { /* if n can't be converted to int */
          cerr << "port number needs to be an integer" << endl;
          exit(1);
        }
      case 'a': /* if -t is provided, use threads instead of processes*/
        cerr << "Emma Jin (mqjin)" << endl;
        exit(1);
      case 'v':
        debug_mode = true;
        break;
      default: /* if other error, stderr */
        cerr << "parsing failed - please check you arguments!" << endl;
        exit(1);
    }
  }

}

int main(int argc, char *argv[])
{
  // Register the SIGINT signal handler
  install_sigint_handler_smtp();
  // Parse commands
  int port_number = 2500;
  bool debug_mode = false;
  bool relay_mode = false;
  parse_commands(port_number, debug_mode, relay_mode, argc, argv);
  // create socket
  int listen_fd = create_listening_socket(port_number);
  // keep listening for client connection requests
  while (true) {
    // accept the connection from client
    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);
    int* fd = new int;
    *fd = accept(listen_fd, reinterpret_cast<struct sockaddr*>(&clientaddr), &clientaddrlen);
    if (*fd == -1) {
      cerr << "accept failed" << endl;
      continue;
    }
    if (debug_mode) {
      cerr << "[" << *fd << "] New connection" << endl;
    }
    // create worker thread
    ThreadArgs* args = new ThreadArgs;
    args->fd = fd;
    args->debug_mode = debug_mode;
    pthread_t thread_id;
    if (pthread_create(&thread_id, nullptr, thread_function, args) != 0) {
      cerr << "pthread_create failed" << endl;
      close(*fd);
      delete fd;
      continue;
    }
    g_thread_args_map[thread_id] = *fd;
    //pthread_detach(thread_id);
  }
  for (const auto& entry : g_thread_args_map) {
    pthread_join(entry.first, NULL);
  }
  close(listen_fd);
  return 0;
}
