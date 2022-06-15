#include <bits/stdc++.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <semaphore.h>

#define TIME_OUT -1

using namespace std;

sem_t chatroom_server_boot_lock;
sem_t chatroom_server_connect_lock;
sem_t chatroom_server_end_lock;
string server_owner;
int server_port = -1;

class ChatroomServer{
    private:
    int sock_tcp;
    char *input_buf;
    int length;
    sockaddr_in address;
    vector<pollfd> poll_vec;
    bool listening = false;
    unordered_map<string,string> port_to_user; 
    list<string> mes_list;

    public:
    ChatroomServer(uint16_t port){
        sem_init(&chatroom_server_boot_lock, 0, 0);
        if((sock_tcp = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
        }
    
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);
        //open the reuse option or it will take 30 seconds to reuse the port
        int enable = 1;
        setsockopt(sock_tcp, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

        if((bind(sock_tcp, (sockaddr * ) & address, sizeof(address))) == -1){
            printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
            exit(0);    
        }

        pollfd pfd;
        pfd.fd = sock_tcp;
        pfd.events = POLLIN|POLLPRI;
        pfd.revents = 0;
        poll_vec.push_back(pfd);
    }

    ~ChatroomServer(){
        time_t current = time(nullptr);
        tm *time_struct = localtime(&current);
        string time_info = '[' + to_string(time_struct->tm_hour) + ':' + to_string(time_struct->tm_min) + "] : ";

        string sys = "sys" + time_info + "the chatroom is close.";

        for (int i = 0; i < poll_vec.size(); i++) {
            if (poll_vec[i].fd != sock_tcp) {
                send(poll_vec[i].fd, sys.c_str(), strlen(sys.c_str()), 0);
            }
        }

        for (int i = 0; i < poll_vec.size(); i++) {
            shutdown(poll_vec[i].fd, SHUT_RDWR);
            close(poll_vec[i].fd);
        }
        shutdown(sock_tcp, SHUT_RDWR);
        close(sock_tcp);
        server_port = -1;
        server_owner.clear();
        sem_post(&chatroom_server_end_lock);
    }

    void inputcontrol(int fd){
        time_t current = time(nullptr);
        tm *time_struct = localtime(&current);
        string time_info = '[' + to_string(time_struct->tm_hour) + ':' + to_string(time_struct->tm_min) + "] : ";

        input_buf = new char[1024];
        length = recv(fd, input_buf, 1024, 0);
        input_buf[length]='\0';
        string message = input_buf;

        sockaddr_in src_addr;
        socklen_t src_len = sizeof(src_addr);    
        getpeername(fd,(struct sockaddr*) &src_addr, &src_len);//tcp
        stringstream ss;
        ss << ntohs(src_addr.sin_port);
        string ip_port = ss.str();

        if(length){
            if(port_to_user.find(ip_port) == port_to_user.end()){//first connection of a user, so the message is the name of the user
                port_to_user[ip_port] = message;
                if(server_owner.empty()){
                    server_owner = message;
                }
                if(port_to_user[ip_port] != server_owner){
                    string login_mes;
                    for (list<string>::reverse_iterator rit = mes_list.rbegin();rit != mes_list.rend() ; rit++){
                        if(rit == mes_list.rbegin()) login_mes += *rit;
                        else login_mes += '\n' + *rit;       
                    }
                    
                    send(fd, login_mes.c_str(), strlen(login_mes.c_str()), 0);
    
                    string sys = "sys " + time_info + message + " join us.";
                    for (int i = 0; i < poll_vec.size(); i++) {
                        if (poll_vec[i].fd != sock_tcp && poll_vec[i].fd != fd) {
                            send(poll_vec[i].fd, sys.c_str(), strlen(sys.c_str()), 0);
                        }
                    }
                }
            }
            else{//handle message
                string back = port_to_user[ip_port] + time_info + message;
                if(mes_list.size() ==3){
                    mes_list.pop_back();
                }
                mes_list.push_front(back);
                for (int i = 0; i < poll_vec.size(); i++) {
                    if (poll_vec[i].fd != sock_tcp && poll_vec[i].fd != fd) {        
                        send(poll_vec[i].fd, back.c_str(), strlen(back.c_str()), 0);
                    }
                }
            }
        }
        else{//client input "leave-chatroom"
            if(port_to_user[ip_port] != server_owner){
                string sys = "sys " + time_info + port_to_user[ip_port] + " leave us.";
                for (int i = 0; i < poll_vec.size(); i++) {
                        if (poll_vec[i].fd != sock_tcp && poll_vec[i].fd != fd) {
                            send(poll_vec[i].fd, sys.c_str(), strlen(sys.c_str()), 0);
                        }
                }
            }
            port_to_user.erase(ip_port);
            shutdown(fd, SHUT_RDWR);
            close(fd);
        }
        delete[] input_buf;
    }
    
    void connection(){
        pollfd connect_fd;
        if( (connect_fd.fd = accept(sock_tcp, (struct sockaddr*)NULL, NULL)) == -1){
            printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
        }
        connect_fd.events = POLL_IN;
        connect_fd.revents = 0;
        poll_vec.push_back(connect_fd);
    }

    void stoptolisten(){
        listening = false;
    }

    void listencontrol(){
        listening = true;
        //indicate that classroom server is ready to server

        if(listen(sock_tcp, 10) == -1){
            printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
            exit(0);
        }
        sem_post(&chatroom_server_boot_lock);
        while(listening){
          poll(poll_vec.data(), poll_vec.size(), TIME_OUT);
          for (int i = 0; i < poll_vec.size(); i++)
          {
            if ((poll_vec[i].revents & POLL_IN) == POLL_IN){
                if(poll_vec[i].fd == sock_tcp){ //tcp connection
                  connection();
                  if (server_owner.empty()) {
                    sem_post(&chatroom_server_connect_lock);
                    if(server_port == -1){
                        server_port = ntohs(address.sin_port);
                    }
                  }
                }
                else{ //tcp mes control
                  inputcontrol(poll_vec[i].fd);
                }
            }
            else if(poll_vec[i].revents != 0){ // user leave
                //cout<<"chatroom : client disconnected"<<endl;
                shutdown(poll_vec[i].fd, SHUT_RDWR);
                close(poll_vec[i].fd);
                poll_vec.erase(poll_vec.begin() + i);
            }
          }
        }
        sem_post(&chatroom_server_end_lock); // tell the client that server has completely close
    }
    
};

class Client{
    private:
        int csock_tcp, csock_udp;
        sockaddr_in address;
        socklen_t addr_len = sizeof(address);        
        vector<pollfd> poll_vec;
        char *input_buf;
        int length;
        //for hw3 chatroom
        bool create_chatroom = false;
        bool connect_chatroom = false;
        pthread_t chatroom_server_thread_id;
        pthread_t recv_echo_thread_id;
        bool chatroom_mode = false;
        ChatroomServer *chatroom_server;
        int chatroom_tcp;
        sockaddr_in chatroom_addr;
        string chat_input;

    public:
    Client(string ip, uint16_t port){
        csock_tcp = socket(AF_INET, SOCK_STREAM, 0);
        csock_udp = socket(AF_INET, SOCK_DGRAM, 0);
        address.sin_family = AF_INET;
        inet_aton(ip.c_str(), &(address.sin_addr));
        address.sin_port = htons(port);

        connect(csock_tcp, (struct sockaddr * ) & address, addr_len);
        input_buf = new char[1024];
        length = recv(csock_tcp, input_buf, 1024, 0);
        input_buf[length] = '\0';
        sendto(csock_udp, input_buf, strlen(input_buf), 0, (sockaddr * ) & address, addr_len);
        string welcome="********************************\n** Welcome to the BBS server. **\n********************************";
        cout<<welcome<<endl;
        delete [] input_buf;

        pollfd pfd;
        pfd.fd = csock_tcp;
        pfd.events = POLLIN|POLLPRI;
        pfd.revents = 0;
        poll_vec.push_back(pfd);
        pfd.fd = csock_udp;
        poll_vec.push_back(pfd);
    }

    ~Client(){
	shutdown(csock_tcp, SHUT_RDWR);
	shutdown(csock_udp, SHUT_RDWR);
    close(csock_tcp);
    close(csock_udp);
    //cout << "Client shutdown" << endl;
    }

    static void *recv_echo_thread(void *arg) {
        Client *client = (Client *) arg;
        char thread_input_buffer[1024];
        int length;
        while ((length = recv(client->chatroom_tcp, thread_input_buffer, 1024, 0))) {
            thread_input_buffer[length] = '\0';
            if (client->chatroom_mode) {
                cout << thread_input_buffer; 
                cout << endl;
            } else {
                break;
            }
        }
    
        if (client->chatroom_mode) { // chatroom server closed (passive close)
            client->chatroom_mode = false;
            shutdown(client->chatroom_tcp, SHUT_RDWR);
            close(client->chatroom_tcp);
            client->sendmessage("detach");
        }
        
    }

    static void *chatroom_server_thread(void *arg) {
        ChatroomServer *server = (ChatroomServer *) arg;
        server->listencontrol();
    }

    void createChatroom(string ip, uint16_t port, string owner){
        chatroom_mode = true;
        //create thread for maintain chatroom
        chatroom_server = new ChatroomServer(port);
        pthread_create(&chatroom_server_thread_id, nullptr, Client::chatroom_server_thread, chatroom_server);
        sem_wait(&chatroom_server_boot_lock);

        chatroom_tcp = socket(AF_INET, SOCK_STREAM, 0);
        chatroom_addr.sin_family = AF_INET;
        inet_aton(ip.c_str(), &(chatroom_addr.sin_addr));
        chatroom_addr.sin_port = htons(port);

        sem_init(&chatroom_server_connect_lock, 0, 0);
        connect(chatroom_tcp, (struct sockaddr * ) & chatroom_addr, sizeof(chatroom_addr));
        sem_wait(&chatroom_server_connect_lock);
        //create thread to receive other user's messages
        pthread_create(&recv_echo_thread_id, nullptr, Client::recv_echo_thread, this);
        send(chatroom_tcp, owner.c_str(), strlen(owner.c_str()), 0); // send the owner's name
        cout << "start to create chatroom..." << endl;
        string welcome="********************************\n** Welcome to the chatroom. **\n********************************";
        cout<<welcome<<endl;

        string chat_input;
        while (chatroom_mode) {
            getline(cin, chat_input);
            if (chat_input == "detach") {
                chatroom_mode = false;
                shutdown(chatroom_tcp, SHUT_RDWR);
                close(chatroom_tcp);
                sendmessage("detach");
                continue;
            }
            
            if (chat_input == "leave-chatroom") {
                chatroom_mode = false;
                sem_init(&chatroom_server_end_lock, 0, 0);
                chatroom_server->stoptolisten();
                shutdown(chatroom_tcp, SHUT_RDWR);
                close(chatroom_tcp);
                sem_wait(&chatroom_server_end_lock);
                sem_destroy(&chatroom_server_end_lock);
                delete chatroom_server;
                sendmessage("leave-chatroom");
                continue;
            }
            
            if (chatroom_mode) {
                send(chatroom_tcp, chat_input.c_str(), strlen(chat_input.c_str()), 0);
            }
        }
        sem_destroy(&chatroom_server_connect_lock);
        sem_destroy(&chatroom_server_boot_lock);
    }

    void connectChatroom(string ip, uint16_t port, string user){
        chatroom_mode = true;
        chatroom_tcp = socket(AF_INET, SOCK_STREAM, 0);
        chatroom_addr.sin_family = AF_INET;
        inet_aton(ip.c_str(), &(chatroom_addr.sin_addr));
        chatroom_addr.sin_port = htons(port);

        connect(chatroom_tcp, (struct sockaddr * ) & chatroom_addr, sizeof(chatroom_addr));
        //create thread to receive other user's messages
        pthread_create(&recv_echo_thread_id, nullptr, Client::recv_echo_thread, this);
        send(chatroom_tcp, user.c_str(), strlen(user.c_str()), 0); // send the owner's name
        string welcome="********************************\n** Welcome to the chatroom. **\n********************************";
        cout<<welcome<<endl;

        while (chatroom_mode) {
            getline(cin, chat_input);
            
            if(chatroom_mode){
            if (chat_input == "detach" && port == server_port) {
                chatroom_mode = false;
                shutdown(chatroom_tcp, SHUT_RDWR);
                close(chatroom_tcp);
                sendmessage("detach");
                continue;
            }
            
            if (chat_input == "leave-chatroom") {
                if(port == server_port){
                    chatroom_mode = false;
                    sem_init(&chatroom_server_end_lock, 0, 0);
                    chatroom_server->stoptolisten();
                    shutdown(chatroom_tcp, SHUT_RDWR);
                    close(chatroom_tcp);
                    sem_wait(&chatroom_server_end_lock);
                    sem_destroy(&chatroom_server_end_lock);
                    delete chatroom_server;
                    sendmessage("leave-chatroom");
                    continue;
                }
                else{
                    chatroom_mode = false;
                    shutdown(chatroom_tcp, SHUT_RDWR);
                    close(chatroom_tcp);
                    sendmessage("detach");
                }
            }
            
            if (chatroom_mode) {
                send(chatroom_tcp, chat_input.c_str(), strlen(chat_input.c_str()), 0);
            }
            }
        }
    }
    
    void sendmessage(string mes){
        if (regex_match(mes, regex("^register.*")) || mes == "whoami" || mes == "list-chatroom") {
            sendto(csock_udp, mes.c_str(), strlen(mes.c_str()), 0, (sockaddr * ) &address, addr_len);
        } 
        else {
            sendto(csock_tcp, mes.c_str(), strlen(mes.c_str()), 0, (sockaddr * ) &address, addr_len);
        }
    }

    void listenmessage(){
        poll(poll_vec.data(), poll_vec.size(), TIME_OUT);
        for (int i = 0; i < poll_vec.size(); i++) {
            if ((poll_vec[i].revents & POLL_IN) == POLL_IN) {
                input_buf = new char[1024];
                length = recv(poll_vec[i].fd, input_buf, 1024, 0);
                input_buf[length] = '\0';

                if(create_chatroom){
                    stringstream parser(input_buf);
                    string user_name;
                    getline(parser, user_name, ' ');
                    string port;
                    getline(parser, port, ' ');

                    for (char c : port) {
                        if (!isdigit(c)) {
                        create_chatroom = false;
                        break;
                        }
                    }

                    if(create_chatroom){
                        create_chatroom = false;
                        createChatroom("127.0.0.1", stoul(port), user_name);
                        length = recv(poll_vec[i].fd, input_buf, 1024, 0);
                        input_buf[length] = '\0';
                    }
                }


                
                if(connect_chatroom){
                    stringstream parser(input_buf);
                    string user_name;
                    getline(parser, user_name, ' ');
                    string port;
                    getline(parser, port, ' ');

                    for (char c : port) {
                        if (!isdigit(c)) {
                        connect_chatroom = false;
                        break;
                        }
                    }

                    if(connect_chatroom){
                        connect_chatroom = false;
                        connectChatroom("127.0.0.1", stoul(port), user_name);
                        length = recv(poll_vec[i].fd, input_buf, 1024, 0);
                        input_buf[length] = '\0';
                    }
                }

                cout << input_buf ;
                delete[] input_buf;
            } else if (poll_vec[i].revents != 0) { 
                cout << "{Caution} : Disconnected from server" << endl;
                close(poll_vec[i].fd);
                poll_vec.erase(poll_vec.begin() + i);
            }
        }
    }

    void clientstart(){
        string input;
        while(1){
            cout << "% ";
            if (!chat_input.empty()) {
                if (chat_input.size() == 0) {
                    continue;
                }
                if (chat_input == "exit") {
                    shutdown(csock_tcp, SHUT_RDWR);
                    shutdown(csock_udp, SHUT_RDWR);
                    close(csock_tcp);
                    close(csock_udp);
                    break;
                }
        
                if (regex_match(chat_input, regex("^create-chatroom.*")) || chat_input == "restart-chatroom") {
                    create_chatroom = true;
                }
                
                if (regex_match(chat_input, regex("^join-chatroom.*")) || chat_input == "attach") {
                    connect_chatroom = true;
                }
                
                sendmessage(chat_input);
                chat_input.clear();
                listenmessage();
                continue;
            }

            getline(cin, input);
            if (input.size() == 0) {
                continue;
            }
            if (input == "exit") {
                shutdown(csock_tcp, SHUT_RDWR);
	            shutdown(csock_udp, SHUT_RDWR);
                close(csock_tcp);
                close(csock_udp);
                break;
            }
    
            if (regex_match(input, regex("^create-chatroom.*")) || input == "restart-chatroom") {
                create_chatroom = true;
            }
            
            if (regex_match(input, regex("^join-chatroom.*")) || input == "attach") {
                connect_chatroom = true;
            }
            
            sendmessage(input);
            listenmessage();
        }
    }
};

int main(int argc, char *argv[]) {
  string ip(argv[1]);
  Client client(ip, atoi(argv[2]));
  client.clientstart();
}