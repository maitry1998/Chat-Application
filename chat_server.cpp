#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <vector>
#include <cstring>
#include <map>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <csignal>
#include <netdb.h>
#include <sstream>
#include <thread>
#include <cstdlib>
#include <unistd.h>

const int MAX_CLIENTS = 20;
const int MSG_SIZE = 1024;

std::mutex mu;
// std::vector < std::string > logged_in_users;

int abc;

// this is a signal handler which is called to handle the ctrl C signal
void exit_handle(int signum) {
  std::cout << "Exiting the chat application!" << std::endl;
  close(abc); // abc is the socket which needs to be closed of server 
  exit(signum);
}

// Initialize username to socket mapping
/*this is the main map which maps all the usernmaes of the logged in clients to theier resspective socket numbers*/
std::map < std::string, int > map_user_cl_soc;

void * navigate_taskrunner(void * arg) {
  int cl_soc = * (int * ) arg;
  char read_msg_bfr[MSG_SIZE];

  while (true) {
    memset(read_msg_bfr, 0, MSG_SIZE);
    int mes_recv = read(cl_soc, read_msg_bfr, MSG_SIZE);
    if (mes_recv < 0) {
      std::cerr << "There is some error in recieving the message" << std::endl;
      break;
    } else if (mes_recv == 0) {
      // Client has disconnected
      std::cout << "One of the clients has exited the application" << std::endl;
      break;
    } else {
      std::string whole_msg_orignial(read_msg_bfr, mes_recv);
      if (whole_msg_orignial.substr(0, 5) == "login") {

        // Extract username from message and store in map_user_cl_soc map
        std::string username = whole_msg_orignial.substr(6, whole_msg_orignial.length() - 7);


        username.erase(username.find_last_not_of(" \n\r\t") + 1);
        std::string msg_log_already = "Seems like the user with same username is looged in other session";

        for (const auto & [eachalready, socket]: map_user_cl_soc) {
            if (eachalready == username) // beacuse we dont want to sent the mesage back to same user
            {
              write(socket, msg_log_already.c_str(),msg_log_already.length());
            }
          }

        mu.lock();
        map_user_cl_soc[username] = cl_soc;
        // logged_in_users.push_back(username);
        mu.unlock();
      } else if (whole_msg_orignial.substr(0, 6) == "logout") {

        // Remove socket connection information for corresponding client
        mu.lock();
        for (auto it = map_user_cl_soc.begin(); it != map_user_cl_soc.end(); ++it) {
          if (it -> second == cl_soc) {
            map_user_cl_soc.erase(it);
            break;
          }
        }
        mu.unlock();

        break;
      } else if (whole_msg_orignial.substr(0, 4) == "chat") {

        // Parse chat msg
        std::string usname_client = "";
        std::string text_for_chat = whole_msg_orignial.substr(5);
        size_t pos = text_for_chat.find(" ");

        if (pos != std::string::npos) {
          /*
          here, the message is of type "chat @userb message"
          whole_msg_orignial will have everything after chat , i.e @userb message
          then  split by " " to fetch the message and sender username seperately
         
          
          */

          // we did (1,pos -1) because we want the user name without @ and " "
          usname_client = text_for_chat.substr(1, pos - 1);  
          text_for_chat = text_for_chat.substr(pos + 1);
        }

        // Broadcast or send message to recipient
        /* is @ is present in chat message it means its a personal message if
        @ is not present, its a broadcast message*/
        if (whole_msg_orignial.find('@') == std::string::npos) {
          // this if condition is for brodcast message
          /*the message that we have recieved from the cient is of type below:
          username to whom message is supposed to be sent >> message 
          */

          int index = whole_msg_orignial.find(">>");
          std::string thisuser;

          // this is the user who sent the message
          thisuser = whole_msg_orignial.substr(5, index - 6);

          /*
          In case of brodcast user, the user who sent the message has its username stored in thisuser
          we dont have to send the message to that user
          */
          for (const auto & [username, socket]: map_user_cl_soc) {

            if (username != thisuser) // beacuse we dont want to sent the mesage back to same user
            {
              write(socket, whole_msg_orignial.substr(5, whole_msg_orignial.length()).c_str(), whole_msg_orignial.substr(5, whole_msg_orignial.length()).length());
            }
          }

        } else {
          // Send message to specified recipient and not everyone, This is if the chat message has a @
          mu.lock();
          auto it = map_user_cl_soc.find(usname_client);

          if (it != map_user_cl_soc.end()) {
            write(it -> second, text_for_chat.c_str(), text_for_chat.length());
          } else {
            // Recipient not found, send error message to client
            std::string error_message = "The User " + usname_client + " can not be located.";
            write(cl_soc, error_message.c_str(), error_message.length());
          }
          mu.unlock();
        }
      } else {
        // Invalid msg, send error msg to client
        std::string error_message = "Please type a valid command";
        write(cl_soc, error_message.c_str(), error_message.length());
      }
    }
  }
  return nullptr;
}

int main(int argc, char * argv[]) {
  // Check command-line arguments
  if (argc != 2) {
    std::cerr << "You need to give config file" << std::endl;
    exit(1);
  }

  // Read configuration file
  std::ifstream config_file(argv[1]);
  if (!config_file.is_open()) {
    std::cerr << "Cant open configuration file" << std::endl;
    exit(1);
  }
  int port = 0;
  std::string line;
  bool zeroportflag = false;
  std::getline(config_file, line);
  std::istringstream iss(line);
  std::string key, realport;
    if (!(iss >> key >> realport)) {
      std::cerr << "The file format is wrong, the file should be formatted like port: portnumber" << std::endl;
      return 1;
    }
    if (key == "port:") { // dont remove the : 
    // if for some reason , you are not able to retrieve port number ,
    // replace the below line with your desiresd port number, only if the port is not able to read
    port = std::stoi(realport);
    if (port == 0)
    {
      std::cout << "Server config file has read 0 as port number, so server will be assigned an unused port!\n";
      zeroportflag = true;
    }
    } else {
      std::cerr << "The key: " << key << "is not present in configuration file" << std::endl;
      return 1;
    }
  

  // Create server socket
  // TCP BSD
  int ser_soc = socket(AF_INET, SOCK_STREAM, 0);
  if (ser_soc < 0) {
    std::cerr << "Error in creating socket" << std::endl;
    exit(1);
  }

  // Bind server socket
  struct sockaddr_in ser_addr_struct;
  ser_addr_struct.sin_family = AF_INET;
  ser_addr_struct.sin_addr.s_addr = INADDR_ANY;
  ser_addr_struct.sin_port = htons(port);

  if (bind(ser_soc, (struct sockaddr * ) & ser_addr_struct, sizeof(ser_addr_struct)) < 0) {
    std::cerr << "Socket bind error" << std::endl;
    exit(1);
  }

  // Get the assigned port number
    socklen_t addrlen = sizeof(ser_addr_struct);
    if (getsockname(ser_soc, (struct sockaddr*) &ser_addr_struct, &addrlen) == -1) {
        std::cerr << "Failed to get socket name\n";
        return 1;
    }
    std::cout << "Server started on port " << ntohs(ser_addr_struct.sin_port) << "\n";


    // if the port is 0, we need to update the new unused port with whatever is selected to the client file also
    if (zeroportflag)
    {
         std::fstream file("client_config.txt", std::ios::in | std::ios::out); // assuming that all are in one folder
         std::string line;
        std::string linehost;
        if (file.is_open()) {
        getline(file, linehost);  // Read the existing value of servhost
        file.seekp(0, std::ios::beg); // Move the write position to the beginning of the file
        // file.truncate(); // Trun
        file << linehost + '\n';
        file << "servport: " + std::to_string(ntohs(ser_addr_struct.sin_port));
        file.close();
        std::cout << "The client configuration file has been updated to use the newly assigned unused port!\n";
    } else {
        std::cout << "Failed to open file.\n";
    }
    }

  // Listining for connections
  if (listen(ser_soc, MAX_CLIENTS) < 0) {
    std::cerr << "Error in listening for connections" << std::endl;
    exit(1);
  }

  // While its listining , displaying the data at which server is open so that the clients can change that information 
  // accordingly to the config file
  // std::cout << "Server is running on the port:" << port << std::endl;
  char hostname[MSG_SIZE];
  gethostname(hostname, MSG_SIZE);
  std::cout << "Hostname for the server is: " << hostname << std::endl;

  abc = ser_soc;

  // this is the part where we are registering the signal handleer to handle the exit event
  struct sigaction sa;
  std::memset( & sa, 0, sizeof(sa));
  sa.sa_handler = & exit_handle;
  sigemptyset( & sa.sa_mask);
  sigaddset( & sa.sa_mask, SIGINT);
  sigaction(SIGINT, & sa, nullptr);

  while (true) {
    fd_set readfds;
    FD_ZERO( & readfds);
    FD_SET(ser_soc, & readfds);
    int max_fd = ser_soc;
    // Here, we get the messages recieved from clients , we call the thread for individual client
    if (FD_ISSET(ser_soc, & readfds)) {
      // Accept incoming connection from client
      int cl_soc = accept(ser_soc, NULL, NULL);
      if (cl_soc < 0) {
        std::cerr << "Connection cannot be accepted" << std::endl;
        exit(1);
      }
      pthread_t thread;
      // we are calling thread for individual threads. the fourth argument we are passing is client socket id 
      // because we want individual thread for individual clients 
      if (pthread_create( & thread, NULL, navigate_taskrunner, (void * ) & cl_soc) != 0) {
        std::cerr << "Thread cannot be created" << std::endl;
        exit(1);
      }

    }

  }

}
