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

// backend
F_2_B_Message send_and_receive_msg(int fd, const string &addr_str, F_2_B_Message msg)
{
  F_2_B_Message msg_to_return;
  sockaddr_in addr = get_socket_address(addr_str);
  connect(fd, (struct sockaddr *)&addr,
          sizeof(addr));
  string to_send = encode_message(msg);
  cout << "to send: " << to_send << endl;
  ssize_t bytes_sent = send(fd, to_send.c_str(), to_send.length(), 0);
  if (bytes_sent == -1)
  {
    cerr << "Sending message failed.\n";
  }
  else
  {
    cout << "Message sent successfully: " << bytes_sent << " bytes.\n";
  }

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

F_2_B_Message construct_msg(int type, string rowkey, string colkey, string value, string value2, string errmsg, int status)
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