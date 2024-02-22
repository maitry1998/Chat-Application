#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <csignal>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
using namespace std;

const int MESSAGE_SIZE = 1024;

int abc;
void read_and_print(int my_sock, fd_set read_fds)
{
   // Check if any input is available on socket , notice that we only want to read and display the socket wchich
    // select function has selected , as its is monitoring for the read descriptors , whenever something is ready 
    // to be read in the current client socket, it will be set in read_fsd

    if (FD_ISSET(my_sock, & read_fds)) {
      // if there is something to read from the socket, which is sent by the server , we just display it
      // as it is 
      char buffer[MESSAGE_SIZE];
      memset(buffer, 0, MESSAGE_SIZE);
      int bytes_received = read(my_sock, buffer, MESSAGE_SIZE - 1);
      if (bytes_received < 0) {
        cerr << "Error receiving message_to_send" << endl;
        exit(1);
      } else if (bytes_received == 0) {
        // Server has disconnected
        cout << "Server has disconnected" << endl;
        exit(1);
      } else {
        string message_to_send(buffer, bytes_received);
        cout << message_to_send << endl;
      }
    }
}

// Signal handler function for the ctrl c signal
void handle_exit(int signum) {
  cout << "The Application has exited!" << endl;
  exit(signum);
  close(abc); // closing the client socket so that it acn be reused 
}

int main(int argc, char * argv[]) {
  // the second argument must have config file
  if (argc != 2) {
    cerr << "Second argument must be config file name" << endl;
    return 1;
  }

  // Read configuration file
  ifstream confi_name(argv[1]);
  if (!confi_name) {
    cerr << "Cannot open the configuration file" << endl;
    return 1;
  }

  /*
  this part of the code is reading data from config file
  */
  string servhost, servport;
  string each;
  int cnt_lines=0;
  while ((cnt_lines < 2) && (getline(confi_name, each))) {
    istringstream iss(each);
    string kwar, value;
    if (!(iss >> kwar >> value)) {
      cerr << "The format of file has been compromised" << endl;
      return 1;
    }
    if (kwar == "servhost:") {
    servhost = value;
    } else if (kwar == "servport:") {
    servport = value;
    // htons(std::stoi(servport))
    } else {
      cerr << "Error: unrecognized keyword " << kwar << " in configuration file" << endl;
      return 1;
    }
    cnt_lines++;
  }

  // here is where we are attaching the signal handler for our interrupt signal
  struct addrinfo hints, * serverdata;
  memset( & hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  int recievedinfo;
  if ((recievedinfo = getaddrinfo(servhost.c_str(), servport.c_str(), & hints, & serverdata)) != 0) {
    cerr << "Error: " << gai_strerror(recievedinfo) << endl;
    return 1;
  }

  // Here is where we are creating socket 
  int my_sock = socket(serverdata -> ai_family, serverdata -> ai_socktype, serverdata -> ai_protocol);
  abc = my_sock;
  if (my_sock < 0) {
    cerr << "Error: cannot create socket" << endl;
    return 1;
  }

  // here we are connecting to server
  if (connect(my_sock, serverdata -> ai_addr, serverdata -> ai_addrlen) < 0) {
    cerr << "Error: cannot connect to server" << endl;
    close(my_sock);
    return 1;
  }
  freeaddrinfo(serverdata);

  string username;
  fd_set master_fds;
  FD_ZERO( & master_fds);
  FD_SET(my_sock, & master_fds);
  FD_SET(STDIN_FILENO, & master_fds);

//   abc = my_sock;
  struct sigaction sa;
  memset( & sa, 0, sizeof(sa));
  sa.sa_handler = & handle_exit;
  sigemptyset( & sa.sa_mask);
  sigaddset( & sa.sa_mask, SIGINT);
  sigaction(SIGINT, & sa, nullptr);
  bool Loggedout = false;

  while (true) {
    // Wait for input on either standard input or socket
    fd_set read_fds = master_fds;
    // here FD_SETSIZE is a constant defined in the C++ library sys/select.h
    // we are writing select here to monitor for the read operations, 
    // there can be two types of read operation
    // from STDIN i.e from keyboard
    // from socket i.e from server
    int num_fds = select(FD_SETSIZE, & read_fds, NULL, NULL, NULL);
    if (num_fds < 0) {
      cerr << "Error in select" << endl;
      break;
    }

    // Check if input is available on standard input for read and sending to server
    if (FD_ISSET(STDIN_FILENO, & read_fds)) {
      string whole_msg;
      getline(cin, whole_msg);

      // here i am fetching whatever the user has typed on terminal
      istringstream iss(whole_msg);
      string data;
      iss >> data;

      // Execute command
      if (data == "exit") {
        // Close connection and exit program
        if (Loggedout)
        {
          std::cout << "Exiting the chat application!" << std::endl;
          // this is imp, we have to close the socfd when we type exit, if the socket is remained open it cannot be reused
        close(my_sock);
        return 0;
        }
        else
        {
           cerr << "You cannot exit if you have not logged out." << endl;
           continue;
        }
        
      } else if (data == "login") {
        // Getting username  this is very importnat as the username will be used later on
        Loggedout = false;
        iss >> username;

        // to note that we are parsing the message here before sending 
        string message_to_send = "login " + username;
        if (write(my_sock, message_to_send.c_str(), message_to_send.size() + 1) < 0) {
          cerr << "Cannot send message to server some error occured" << endl;
          close(my_sock);
          return 1;
        }
      } else if (data == "logout") {
        // Send logout message to server
        string message_to_send = "logout";
        Loggedout = true;


        // here we are checking that before logging out, the user must be logged in
        if (username.empty()) {
          cerr << "Error: you must log in first to logout" << endl;
          continue; // if user is not logged in , we wait for next input and dont let it prcoeed untill login
        } 

        // if user is not empty the we tell the server aabout this user logging out
        if (write(my_sock, message_to_send.c_str(), message_to_send.size() + 1) < 0) {
          cerr << "Cannot send message to server" << endl;

          close(my_sock);
          exit(1) ;
        }

        // Remove the logged out socket from master set
        FD_CLR(my_sock, & master_fds);
        // shutting down the socket which needs to be logged out
        shutdown(my_sock, SHUT_RDWR);
        close(my_sock);


        /*
        Notice that once the user has logged out, we have closed the socket but we need to create a new connection for the
        next user to login , so i have added the part of reconnection below
        */
        int recievedinfo;
        if ((recievedinfo = getaddrinfo(servhost.c_str(), servport.c_str(), & hints, & serverdata)) != 0) {
          cerr << "Error: " << gai_strerror(recievedinfo) << endl;
          return 1;
        }

        // Create socket and connect to server
        my_sock = socket(serverdata -> ai_family, serverdata -> ai_socktype, serverdata -> ai_protocol);
        if (my_sock < 0) {
          cerr << "Error: cannot create socket" << endl;
          return 1;
        }
        if (connect(my_sock, serverdata -> ai_addr, serverdata -> ai_addrlen) < 0) {
          cerr << "Error: cannot connect to server" << endl;
          close(my_sock);
          return 1;
        }
        freeaddrinfo(serverdata);
        // setting the new socet information to the fd set
        FD_SET(my_sock, & master_fds);

        // clearing the username 
        username.clear();
      } else if (data == "chat") {
        // Check if user is logged in before it can chat
        if (username.empty()) {
          cerr << "User should login first to chat" << endl;
        } else {
          // Get recipient and message to server
          string reciever;
          string message_to_send;
          size_t findpos_for_at = whole_msg.find("@");
          if (findpos_for_at != string::npos) {
            // Extract reciever

            /* the message that user types has two parts, 1 is the username to whom message is to be sent and the other 
            is the actual message
            for example for chat @user1 message text,
            we find the index for @ here , then we find the position for first " " after chat
            whartever is between is recieptient username and whatever is after the next " " is thebmessage
            
             */
            size_t space_pos = whole_msg.find(" ", findpos_for_at);
            reciever = whole_msg.substr(findpos_for_at + 1, space_pos - findpos_for_at - 1);
            // Extract message
            message_to_send = whole_msg.substr(space_pos + 1);
          } else {
            // If @ is not present, it means its a broadcast message for brodcast message we send the data as it is
            // the server will extract "chat" part 
            message_to_send = whole_msg.substr(data.length() + 1);
          }
          // Constructing the  message to be sent
          string chat_msg = "chat "; // initalizing with chat because we have extracted "chat" from message

          // when we send the message to the we send it in a specific format so that the 
          // server we are using can extract the reciever and sender part from it fro perosnal messages
          if (!reciever.empty()) {
            chat_msg += "@" + reciever + " ";
          }

          // here we are appening the send information to the above parsed message
          chat_msg += username + " >> " + message_to_send;
          // Send message to server after processing the input from stdin
          if (write(my_sock, chat_msg.c_str(), chat_msg.length()) == -1) {
            cerr << "Error sending message_to_send" << endl;
            exit(1);
          }
        }

      } else {
        // Invalid command
        cerr << "The command is invalid, Valid commands are login, logout, chat and exit" << endl;
      }
    }
    read_and_print(my_sock,read_fds);
  }

  close(my_sock);
  return 0;
}
