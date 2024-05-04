#include "../../utils/utils.h"
#include <arpa/inet.h> // for inet_pton
#include <cstring>     // for strlen and memset
#include <iostream>
#include <netinet/in.h> // for sockaddr_in
#include <sstream>
#include <sys/select.h> // for select()
#include <sys/socket.h> // for socket functions
#include <unistd.h>     // for close

using namespace std;

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    cerr << "Usage: " << argv[0] << " <Server IP> <Port>" << endl;
    return 1;
  }

  const char *server_ip = argv[1];
  int server_port = stoi(argv[2]);

  // Create a socket
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    cerr << "Error in socket creation" << endl;
    return 1;
  }

  sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port);

  if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
  {
    cerr << "Invalid address/ Address not supported" << endl;
    return 1;
  }

  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    cerr << "Connection Failed" << endl;
    return 1;
  }

  cout << "Connected to the server. Type 'quit' to exit." << endl;
  bool readFromConnection = true;

  while (true)
  {
    if (readFromConnection)
    {
      char buffer[1024] = {0};
      int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
      if (bytes_received <= 0)
      {
        cout << "Server closed the connection or error occurred." << endl;
        break;
      }
      string buffer_str(buffer);
      if (buffer_str.find('|') != string::npos)
      {
        cout << "\nServer: " << endl;
        F_2_B_Message received_message = decode_message(buffer_str);
        print_message(received_message);
      }
      else
      {
        cout << "\nServer: " << buffer << endl;
      }
    }
    else
    {
      string input;
      cout << "> ";
      getline(cin, input);
      if (input == "quit")
      {
        input = input + "\r\n";
        cout << "Closing the connection" << endl;
        send(sock, input.c_str(), input.length(), 0);
        break;
      }
      if (input == "list")
      {
        F_2_B_Message message;
        message.type = 10;
        message.rowkey = "adw";
        message.colkey = "adw";
        message.isFromPrimary = 0;

        string serialized = encode_message(message);
        send(sock, serialized.c_str(), serialized.length(), 0);
        while (true)
        {
          char buffer[1024] = {0};
          int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
          if (bytes_received <= 0)
          {
            cout << "Server closed the connection or error occurred." << endl;
            break;
          }
          string response(buffer);
          F_2_B_Message received_message = decode_message(response);
          if (received_message.rowkey == "terminate")
          {
            cout << "End of list." << endl;
            print_message(received_message);
            break;
          }
          cout << "Server: " << endl;
          print_message(received_message);
        }
        continue;
      }
      istringstream iss(input);
      string command, rowkey, colkey, value, value2;
      iss >> command >> rowkey >> colkey;

      F_2_B_Message message;
      message.rowkey = rowkey;
      message.colkey = colkey;
      message.type = 0;
      message.status = 0;
      message.isFromPrimary = 0;

      if (command == "get")
      {
        message.type = 1;
      }
      else if (command == "put")
      {
        iss >> value;
        message.value = value;
        message.type = 2;
      }
      else if (command == "delete")
      {
        message.type = 3;
      }
      else if (command == "cput")
      {
        iss >> value >> value2;
        message.value = value;
        message.value2 = value2;
        message.type = 4;
      }
      else if (command == "sus")
      {
        message.type = 5;
      }
      else if (command == "rev")
      {
        message.type = 6;
      }
      else
      {
        cerr << "Unknown command" << endl;
        continue;
      }

      // Simple serialization (you may need a more complex method for actual
      // use)
      string serialized = encode_message(message);

      send(sock, serialized.c_str(), serialized.length(), 0);
    }

    readFromConnection = !readFromConnection;
  }

  close(sock);
  return 0;
}
