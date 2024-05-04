#include "./backend_communication.h"

using namespace std;

int create_socket()
{
  int fd = socket(PF_INET, SOCK_STREAM, 0);
  if (fd == -1)
  {
    cerr << "Socket creation failed.\n"
         << endl;
    exit(EXIT_FAILURE);
  }
  return fd;
}

void send_message(int fd, const std::string &to_send)
{
  ssize_t bytes_sent = send(fd, to_send.c_str(), to_send.length(), 0);
  if (bytes_sent == -1)
  {
    cerr << "Sending message failed.\n";
  }
  else
  {
    cout << "Message sent successfully: " << bytes_sent << " bytes.\n";
  }
}

std::string receive_one_message(int fd, std::string &buffer, const unsigned int BUFFER_SIZE)
{
  char temp_buffer[BUFFER_SIZE];
  memset(temp_buffer, 0, BUFFER_SIZE);
  ssize_t bytes_received = recv(fd, temp_buffer, BUFFER_SIZE - 1, 0);

  if (bytes_received == -1)
  {
    std::cerr << "Receiving message failed.\n";
    return "";
  }
  else if (bytes_received == 0)
  {
    std::cout << "Server closed the connection.\n";
    return "";
  }

  temp_buffer[bytes_received] = '\0';
  buffer += std::string(temp_buffer);

  size_t pos = buffer.find("\r\n");
  if (pos != std::string::npos)
  {
    std::string complete_message = buffer.substr(0, pos);
    buffer.erase(0, pos + 2);
    return complete_message;
  }
  return "";
}

F_2_B_Message send_and_receive_msg(int fd, const string &addr_str, F_2_B_Message msg)
{
  F_2_B_Message msg_to_return;
  sockaddr_in addr = get_socket_address(addr_str);
  int new_fd = create_socket();
  connect(new_fd, (struct sockaddr *)&addr,
          sizeof(addr));
  string to_send = encode_message(msg);
  cout << "to send: " << to_send << endl;

  send_message(new_fd, to_send);
  // Receive response from the server
  string buffer;
  while (true)
  {
    const unsigned int BUFFER_SIZE = 1024;
    char temp_buffer[BUFFER_SIZE];
    memset(temp_buffer, 0, BUFFER_SIZE);
    ssize_t bytes_received = recv(new_fd, temp_buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received == -1)
    {
      cerr << "Receiving message failed.\n";
      continue;
    }
    else if (bytes_received == 0)
    {
      cout << "Server closed the connection.\n";
      break;
    }
    else
    {
      temp_buffer[bytes_received] = '\0';
      buffer += string(temp_buffer);
    }

    size_t pos;
    while ((pos = buffer.find("\r\n")) != string::npos)
    {
      string line = buffer.substr(0, pos);
      buffer.erase(0, pos + 2);

      if (!line.empty() && line != "WELCOME TO THE SERVER")
      {
        cout << "going to decode message: " << line << endl;
        msg_to_return = decode_message(line);
        cout << "decoded message" << endl;
        return msg_to_return;
        cout << "after return" << endl;
      }
    }
  }
  close(new_fd);
  cout << "before return" << endl;
  return msg_to_return;
}

F_2_B_Message construct_msg(int type, const std::string &rowkey, const std::string &colkey, const std::string &value, const std::string &value2, const std::string &errmsg, int status)
{
  F_2_B_Message msg;
  msg.type = type;
  msg.rowkey = rowkey;
  msg.colkey = colkey;
  msg.value = value;
  msg.value2 = value2;
  msg.errorMessage = errmsg;
  msg.status = status;
  msg.isFromPrimary = 0;
  return msg;
}

std::string ask_coordinator(sockaddr_in coordinator_addr, const std::string &rowkey, const std::string &type)
{
  int fd = create_socket();
  connect(fd, (struct sockaddr *)&coordinator_addr,
          sizeof(coordinator_addr));
  cout << "connected to coordinator" << endl;
  // From Frontend: GET rowname type
  string to_send = "GET " + rowkey + " " + type + "\r\n";
  send_message(fd, to_send);
  cout << "sent message to coordinator" << endl;
  const unsigned int BUFFER_SIZE = 1024;
  string buffer;
  string message = receive_one_message(fd, buffer, BUFFER_SIZE);
  cout << "received message from coordinator" << endl;
  close(fd);
  if (message.empty())
  {
    cerr << "Failed to receive a complete message or connection was closed." << endl;
    return "";
  }
  if (message.substr(0, 3) == "+OK")
  {
    cout << "Message: " << message << endl;
    vector<string> splitted = split(message);
    return splitted[2];
  }
  else
  {
    cerr << message << endl;
    return "";
  }
  // From Coordinator:
  // +OK RESP 127.0.0.1:port
  // -ERR No server for this range
  // -ERR Incorrect Command
}

bool check_backend_connection(int fd, const string &backend_serveraddr_str, const string &rowkey, const string &colkey)
{
  F_2_B_Message msg_to_send = construct_msg(1, rowkey, colkey, "", "", "", 0);
  F_2_B_Message get_response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
  cout << "got response" << endl;
  if (get_response_msg.status == 2)
  {
    return false;
  }
  return true;
}

string get_backend_server_addr(int fd, const string &rowkey, const string &colkey, map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr, const string &type)
{
  string backend_serveraddr_str;

  if (g_map_rowkey_to_server.find(rowkey) != g_map_rowkey_to_server.end())
  {
    string serveraddr_str_to_check = g_map_rowkey_to_server[rowkey];

    if (check_backend_connection(fd, serveraddr_str_to_check, rowkey, colkey))
    {
      backend_serveraddr_str = serveraddr_str_to_check;
    }
    else
    {
      backend_serveraddr_str = ask_coordinator(g_coordinator_addr, rowkey, type);

      if (backend_serveraddr_str.empty())
      {
        cerr << "ERROR in getting backend server address from coordinator" << endl;
        return "";
      }
    }
  }
  else
  {
    backend_serveraddr_str = ask_coordinator(g_coordinator_addr, rowkey, type);
    cout << "asked coordinator: " << backend_serveraddr_str << endl;

    if (backend_serveraddr_str.empty())
    {

      cerr << "ERROR in getting backend server address from coordinator" << endl;
      return "";
    }
  }
  return backend_serveraddr_str;
}

// 1: coordinator error; 2: backend error
int send_msg_to_backend(int fd, F_2_B_Message msg_to_send, string &value, int &status, string &err_msg,
                        const string &rowkey, const string &colkey,
                        map<string, string> &g_map_rowkey_to_server, sockaddr_in g_coordinator_addr,
                        const string &type)
{

  string backend_serveraddr_str = get_backend_server_addr(fd, rowkey, colkey, g_map_rowkey_to_server,
                                                          g_coordinator_addr, type);
  if (backend_serveraddr_str.empty())
  {
    return 1;
  }
  g_map_rowkey_to_server[rowkey] = backend_serveraddr_str;

  try
  {
    F_2_B_Message response_msg = send_and_receive_msg(fd, backend_serveraddr_str, msg_to_send);
    value = response_msg.value;
    status = response_msg.status;
    err_msg = response_msg.errorMessage;
  }
  catch (const std::exception &e)
  {
    cerr << "ERROR: " << e.what() << endl;
    return 2;
  }

  return 0;
}