#include "./backend_communication.h"

using namespace std;

int create_socket() {
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        cerr << "Socket creation failed.\n"
            << endl;
        exit(EXIT_FAILURE);
    }
    return fd;
}

void send_message(int fd, const std::string &to_send) {
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

std::string receive_one_message(int fd, std::string& buffer, const unsigned int BUFFER_SIZE) {
    char temp_buffer[BUFFER_SIZE];
    memset(temp_buffer, 0, BUFFER_SIZE);
    ssize_t bytes_received = recv(fd, temp_buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received == -1) {
        std::cerr << "Receiving message failed.\n";
        return "";
    } else if (bytes_received == 0) {
        std::cout << "Server closed the connection.\n";
        return "";
    }

    temp_buffer[bytes_received] = '\0';
    buffer += std::string(temp_buffer);

    size_t pos = buffer.find("\r\n");
    if (pos != std::string::npos) {
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
  connect(fd, (struct sockaddr *)&addr,
          sizeof(addr));
  string to_send = encode_message(msg);
  cout << "to send: " << to_send << endl;

  send_message(fd, to_send);
  // Receive response from the server
  // Receive response from the server
  string buffer;
  while (true)
  {
    const unsigned int BUFFER_SIZE = 1024;
    char temp_buffer[BUFFER_SIZE];
    memset(temp_buffer, 0, BUFFER_SIZE);
    ssize_t bytes_received = recv(fd, temp_buffer, BUFFER_SIZE - 1, 0);

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
        msg_to_return = decode_message(line);
        return msg_to_return;
      }
    }
  }
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
  return msg;
}

std::string ask_coordinator(int fd, sockaddr_in coordinator_addr, const std::string &rowkey, int type) {
  connect(fd, (struct sockaddr *)&coordinator_addr,
          sizeof(coordinator_addr));
  //From Frontend: GET rowname type
  string to_send = "GET " + rowkey + to_string(type);
  send_message(fd, to_send);

  const unsigned int BUFFER_SIZE = 1024;
  string buffer;
  string message = receive_one_message(fd, buffer, BUFFER_SIZE);
  if (message.empty()) {
      cerr << "Failed to receive a complete message or connection was closed." << endl;
      return "";
  }
  if (message.substr(0, 3) == "+OK") {
    vector<string> splitted = split(message);
    return splitted[2];
  } else {
    cerr << message << endl;
    return "";
  }
  // From Coordinator:
  // +OK RESP 127.0.0.1:port
  // -ERR No server for this range
  // -ERR Incorrect Command
}