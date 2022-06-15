#include <bits/stdc++.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define TIME_OUT -1

using namespace std;

int post_id=1;
unordered_map<string,pair<string, string> > loginfile;
unordered_map<string,string> useronline;
unordered_map<string,string> tcpudp_map;
unordered_map<string,string> tcptoudp;
map<string,pair<int,string> >chatroom_own;
map<string,string> board_own;
map<int,string> post_to_board;
map<int,pair<int, char *> > post_content;

string mescontrol(char *mes, string ss){
    string command(mes);
    stringstream result;
    mes=strtok(mes, " ");
    vector<string> mes_vec;
    while(mes){
        mes_vec.push_back(mes);
        mes=strtok(NULL," ");
    }
        if(mes_vec[0]=="register"){//udp
            if(mes_vec.size()==4){
                if(loginfile.find(mes_vec[1])==loginfile.end()){
                    loginfile[mes_vec[1]]=make_pair(mes_vec[2], mes_vec[3]);
                    result << "register successfully." << endl;
                }
                else result << "Username is already used." << endl;
            }
            else{
                result << "Usage: register <username> <email> <password>" << endl;
            }
        }
        else if(mes_vec[0]=="login"){//tcp
            if(mes_vec.size()==3){
                if(loginfile[mes_vec[1]].second!=mes_vec[2]){
                    result << "Login failed." << endl;
                }
                else if(useronline.find(ss)!=useronline.end()){
                   result << "Please logout first." << endl;
                }
                else{
                  string res="Welcome, "+mes_vec[1]+".";
                  useronline[ss]=mes_vec[1];
                  result << res << endl;
                }
            }
            else{
                result << "Usage: login <username> <password>>" << endl;
            }
        }
        else if(mes_vec[0]=="logout"){//tcp
            if(mes_vec.size()>1) result << "no need for parameter" << endl;
            else{
                if(useronline.find(ss)==useronline.end()) result << "Please login first." << endl;
                else if(chatroom_own.find(useronline[ss]) != chatroom_own.end() && chatroom_own[useronline[ss]].second == "open"){
                    result << "Please do “attach” and “leave-chatroom” first." << endl;
                }
                else {
                    string res="Bye, "+useronline[ss]+".";
                    useronline.erase(ss);
                    result << res << endl;
                }
            }
        }
        else if(mes_vec[0]=="whoami"){//udp
            if(mes_vec.size()>1) result << "no need for parameter" << endl;
            else{
                if(useronline.find(ss)==useronline.end()) result << "Please login first." <<endl;
                else result << useronline[ss] << endl;
            }
        }
        else if(mes_vec[0]=="list-user"){//tcp
            if(mes_vec.size()>1) result << "no need for parameter" << endl;
            else{
                string res="Name    Email\n";
                int i=0;
                for(auto it=loginfile.begin();it!=loginfile.end();it++,i++){
                    auto& p=it->second;
                    res+=it->first+"   "+p.first;
                    if(i<loginfile.size()-1) res+="\n";
                }
                result << res << endl;
            }
        }
        else if(mes_vec[0]=="exit"){//tcp
            if(mes_vec.size()>1) result <<  "no need for parameter" << endl;
        }
        else if(mes_vec[0]=="create-board"){
            if(mes_vec.size() == 2){
                if(useronline.find(ss)==useronline.end()){
                    result << "Please login first." << endl;
                }
                else{
                    if(board_own.find(mes_vec[1])!=board_own.end()){
                        result << "Board already exists." << endl;
                    }
                    else{
                        board_own[mes_vec[1]]=useronline[ss];
                        result << "Create board successfully." << endl;
                    }
                }
            }
            else result << "Usage : create-board <name>" << endl;
        }
        else if(mes_vec[0]=="create-post"){
            string title, content;
            if(regex_match(command,regex("(create-post )(.+)( --title )(.*)( --content)(.*)")) 
              || regex_match(command,regex("(create-post )(.+)( --content )(.*)( --title)(.*)"))){
                if(useronline.find(ss)==useronline.end()){
                    result << "Please login first." << endl;
                }
                else{
                    if(board_own.find(mes_vec[1])==board_own.end()){
                        result << "Board does not exist." << endl;
                    }
                    else{
                        smatch match;
                        if(regex_search(command, match, regex("(--title )(.*)( --content )(.*)"))){
                            title = match.str(2);
                            content = match.str(4);
                        }
                        else if(regex_search(command, match, regex("(--content )(.*)( --title )(.*)"))){
                            title = match.str(4);
                            content = match.str(2);
                        }

                        time_t t1 = time(nullptr);
                        tm *nPtr = localtime(&t1);
                        int month = nPtr->tm_mon + 1;
                        int mday = nPtr->tm_mday;
                        string date = to_string(month)+"/"+to_string(mday);
                        string post = useronline[ss] + "\n" + title + "\n" + date + "\n" + content + "\n";
                        int shmid = shmget(0, strlen(post.c_str()), IPC_CREAT | 0600);
                        char *shmchar = (char *) shmat(shmid, NULL, 0);
                        strcpy(shmchar, post.c_str());
                        post_to_board[post_id]=mes_vec[1];
                        post_content[post_id++]=make_pair(shmid, shmchar);
                        result << "Create post successfully." << endl;
                    }
                }
            }
            else{
                result << "Usage : create-post <board-name> --title <title> --content <content>" << endl;
            }
        }
        else if(mes_vec[0]=="list-board"){
            if(mes_vec.size()>1) result << "no need for parameter" << endl;
            else{
                result << "Index    Name          Moderator" << endl;
                int index = 1;
                for (auto it = board_own.begin(); it != board_own.end(); it++) {
                result << left << setw(9) << index++ << left << setw(14) << it->first << it->second << endl;
                }
            }
        }
        else if(mes_vec[0]=="list-post"){
            if(mes_vec.size() == 2){
                if(board_own.find(mes_vec[1]) != board_own.end()){
                    result << "S/N    Title            Author    Date" << endl;
                    for (auto it = post_to_board.begin(); it != post_to_board.end(); it++)
                    {
                        if(mes_vec[1] != it->second) continue;
                        stringstream post;
                        post << post_content[it->first].second;
                        string title, author, date;
                        getline(post,author);
                        getline(post,title);
                        getline(post,date);
                        result << left << setw(7) << it->first << left << setw(17) <<title << left << setw(10) << author << date << endl;
                    }
                }
                else result << "Board does not exist." << endl;
            }
            else result << "Usage : list-post <board-name>" << endl;
        }
        else if(mes_vec[0]=="read"){
            if(mes_vec.size() == 2){
                if(post_content.find(stoi(mes_vec[1])) != post_content.end()){
                    stringstream post;
                    string title, author, date, content, comment;
                    post << post_content[stoi(mes_vec[1])].second <<endl;
                    getline(post,author);
                    getline(post,title);
                    getline(post,date);
                    getline(post,content);
                    getline(post,comment);
                    content = regex_replace(content, regex("<br>"), "\n");
                    result << "Author: " << author << "\nTitle: " << title << "\nDate: " << date << "\n--\n" << content << "\n--\n";
                    if (!comment.empty()) {
                        result << regex_replace(comment, regex(" <br> "), "\n") << endl;
                    }
                }
                else result << "Post does not exist." << endl;
            }
            else result << "Usage : read <post-S/N>" << endl;
        }
        else if(mes_vec[0]=="delete-post"){
            if(mes_vec.size() == 2){
                if(useronline.find(ss)==useronline.end()){
                    result << "Please login first." << endl;
                }
                else{
                if(post_content.find(stoi(mes_vec[1])) == post_content.end()){
                    result << "Post does not exist." << endl;
                }
                else{
                stringstream post;
                post << post_content[stoi(mes_vec[1])].second;
                string author;
                getline(post, author, '\n');
                if(author != useronline[ss]){
                    result << "Not the post owner." << endl;
                }
                else{
                    shmdt(post_content[stoi(mes_vec[1])].second);
                    shmctl(post_content[stoi(mes_vec[1])].first, IPC_RMID, nullptr);
                    post_content.erase(stoi(mes_vec[1]));
                    post_to_board.erase(stoi(mes_vec[1]));
                    result << "Delete successfully." << endl;
                }
                }
                }
            }
            else result << "Usage : delete-post <post-S/N>" << endl;
        }
        else if(mes_vec[0]=="update-post"){
            if(regex_match(command,regex("(update-post )(.+)( --title)(.*)( --content)(.*)")) 
              || regex_match(command,regex("(update-post )(.+)( --content)(.*)( --title)(.*)"))
              ||regex_match(command,regex("(update-post )(.+)( --content)(.*)"))
              ||regex_match(command,regex("(update-post )(.+)( --title)(.*)"))){
                if(useronline.find(ss)==useronline.end()){
                    result << "Please login first." << endl;
                }
                else{ 
                    if(post_content.find(stoi(mes_vec[1])) == post_content.end()){
                        result << "Post does not exist." << endl;
                    }
                    else{ 
                    stringstream post;
                    post << post_content[stoi(mes_vec[1])].second;
                    string author;
                    getline(post, author, '\n');
                    if(author != useronline[ss]){
                        result << "Not the post owner." << endl;
                    }
                    else{
                        smatch match;
                        string title, date, content, comment;
                        getline(post,title);
                        getline(post,date);
                        getline(post,content);
                        getline(post,comment);
                        if(regex_search(command, match, regex("(--title )(.*)"))
                        || regex_search(command, match, regex("(--title )(.*)( --content)"))){
                            title = match.str(2);
                        }
                        if(regex_search(command, match, regex("(--content )(.*)"))
                        || regex_search(command, match, regex("(--content )(.*)( --title)"))){
                            content = match.str(2);
                        }
                        //delete the old share memory
                        shmdt(post_content[stoi(mes_vec[1])].second);
                        shmctl(post_content[stoi(mes_vec[1])].first, IPC_RMID, nullptr);
                        post_content.erase(stoi(mes_vec[1]));
                        //create updated share memory
                        string up_post = author + "\n" + title + "\n" + date + "\n" + content + "\n" + comment;
                        int shmid = shmget(0, strlen(up_post.c_str()), IPC_CREAT | 0600);
                        char *shmchar = (char *) shmat(shmid, NULL, 0);
                        strcpy(shmchar, up_post.c_str());
                        post_content[stoi(mes_vec[1])]=make_pair(shmid, shmchar);
                        result << "Update successfully." << endl;
                    }
                    }
                }
            }
            else result << "Usage : update-post <post-S/N> --title/content <new>" << endl;
        }
        else if(mes_vec[0]=="comment"){
            if(regex_match(command, regex("(comment )(\\d+)(.*)"))){
            if(useronline.find(ss)==useronline.end()){
                result << "Please login first." << endl;
            }
            else{
                if(post_content.find(stoi(mes_vec[1])) == post_content.end()){
                    result << "Post does not exist." << endl;
                }
                else{
                    stringstream post;
                    post << post_content[stoi(mes_vec[1])].second;
                    string author, title, date, content, comment;
                    getline(post, author);
                    getline(post,title);
                    getline(post,date);
                    getline(post,content);
                    getline(post,comment);
                    smatch match;
                    if(!comment.empty()) comment += " <br> ";
                    if(regex_search(command, match, regex("(comment \\d+ )(.*)"))){
                        comment += useronline[ss] + ": " + match.str(2);
                    }
                    //delete the old share memory
                    shmdt(post_content[stoi(mes_vec[1])].second);
                    shmctl(post_content[stoi(mes_vec[1])].first, IPC_RMID, nullptr);
                    post_content.erase(stoi(mes_vec[1]));
                    //create updated share memory
                    string up_post = author + "\n" + title + "\n" + date + "\n" + content + "\n" + comment;
                    int shmid = shmget(0, strlen(up_post.c_str()), IPC_CREAT | 0600);
                    char *shmchar = (char *) shmat(shmid, NULL, 0);
                    strcpy(shmchar, up_post.c_str());
                    post_content[stoi(mes_vec[1])]=make_pair(shmid, shmchar);
                    result << "Comment successfully." << endl;
                }
            }
            }
            else result << "Usage : comment <post-S/N> <comment> " << endl;
        }
        else if(mes_vec[0]=="create-chatroom"){
            if(useronline.find(ss) == useronline.end()){
                result << "Please login first." << endl;
            }
            else if(chatroom_own.find(useronline[ss]) != chatroom_own.end()){
                result << "User has already created the chatroom." << endl;
            }
            else{
                chatroom_own[useronline[ss]] = make_pair(stoi(mes_vec[1]), "open");
                result << useronline[ss] << " " << mes_vec[1];
            }
        }
        else if(mes_vec[0] == "list-chatroom"){
            if(useronline.find(ss) == useronline.end()){
                result << "Please login first." << endl;
            }
            else{
                result << "Chatroom_name  Status" << endl;
                for (auto it = chatroom_own.begin(); it != chatroom_own.end(); it++) {
                result << left << setw(15) << it->first << it->second.second << endl;
                }
            }
        }
        else if(mes_vec[0] == "join-chatroom"){
            if(useronline.find(ss) == useronline.end()){
                result << "Please login first." << endl;
            }
            else if(chatroom_own.find(mes_vec[1]) == chatroom_own.end() || chatroom_own[mes_vec[1]].second == "close"){
                result << "The chatroom does not exist or the chatroom is close." << endl;
            }
            else{
                result << useronline[ss] << " " << chatroom_own[mes_vec[1]].first;
            }
        }
        else if(mes_vec[0] == "attach"){
            if(useronline.find(ss) == useronline.end()){
                result << "Please login first." << endl;
            }
            else if(chatroom_own.find(useronline[ss]) == chatroom_own.end()){
                result << "Please create-chatroom first." << endl;
            }
            else if(chatroom_own[useronline[ss]].second == "close"){
                result << "Please restart-chatroom first." << endl;
            }
            else{
                result << useronline[ss] << " " << chatroom_own[useronline[ss]].first;
            }
        }
        else if(mes_vec[0] == "restart-chatroom"){
            if(useronline.find(ss) == useronline.end()){
                result << "Please login first." << endl;
            }
            else if(chatroom_own.find(useronline[ss]) == chatroom_own.end()){
                result << "Please create-chatroom first." << endl;
            }
            else if(chatroom_own[useronline[ss]].second == "open"){
                result << "Your chatroom is still running." << endl;
            }
            else{
                chatroom_own[useronline[ss]].second = "open";
                result << useronline[ss] << " " << chatroom_own[useronline[ss]].first;
            }
        }
        else if(mes_vec[0] == "leave-chatroom") {
            chatroom_own[useronline[ss]].second = "close";
            result << "Welcome back to BBS." << endl;
        }

        else if(mes_vec[0] == "detach") {
            result << "Welcome back to BBS." << endl;
        }
        else {
            result << "Command not found" << endl;
        }
        return result.str();
    }

class BBSserver{
    private:
    int sock_tcp, sock_udp, connect_fd;
    char *input_buf;
    int length;
    sockaddr_in address;
    vector<pollfd> poll_vec;
    vector<pthread_t> worker;
    pthread_mutex_t mutex;  
    vector<int *> pack_pool;
    public:
    BBSserver(uint16_t port){
        if((sock_tcp = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
        }
        if((sock_udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
        }
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);
        
        int enable = 1;
        setsockopt(sock_tcp, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

        if((bind(sock_tcp, (sockaddr * ) & address, sizeof(address))) == -1){
            printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
            exit(0);    
        }
        cout<<"TCP server host on "<<inet_ntoa(address.sin_addr)<<" port: "<<ntohs(address.sin_port) << endl;
        if((bind(sock_udp, (sockaddr * ) & address, sizeof(address))) == -1){
            printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
            exit(0);    
        }
        cout<<"UDP server host on "<<inet_ntoa(address.sin_addr)<<" port: "<<ntohs(address.sin_port) << endl;
        pollfd pfd;
        pfd.fd = sock_tcp;
        pfd.events = POLLIN|POLLPRI;
        pfd.revents = 0;
        poll_vec.push_back(pfd);
        pfd.fd = sock_udp;
        poll_vec.push_back(pfd);

        pthread_mutex_init(&mutex, NULL);
    }

    ~BBSserver(){
        for (int *&element : pack_pool) {
            delete element;
        }   
        shutdown(sock_tcp, SHUT_RDWR);
        shutdown(sock_udp, SHUT_RDWR);
        close(sock_tcp);
        close(sock_udp);
        cout << "BBSserver shutdown" << endl;
    }

    void inputcontrol(int fd){
        input_buf = new char[1024];
        string tmp;
        sockaddr_in src_addr;
        socklen_t src_len = sizeof(src_addr);    
        getpeername(fd,(struct sockaddr*) &src_addr, &src_len);//tcp
        length = recvfrom(fd, input_buf, 1024, 0, (struct sockaddr*) &src_addr, &src_len);//udp
        input_buf[length]='\0';
        cout<<input_buf<<endl;
        stringstream ss;
        ss<<inet_ntoa(src_addr.sin_addr)<< ntohs(src_addr.sin_port);
        string inputstr=input_buf;
        if(tcpudp_map[inputstr] == "undefined"){//first connection request
            stringstream key;
            key<<inputstr<<inet_ntoa(src_addr.sin_addr)<< ntohs(src_addr.sin_port);
            tcpudp_map[inputstr]=key.str();
            tcpudp_map[ss.str()]=key.str();
            tcptoudp[inputstr]=ss.str();
        }
        else{//normal input message
            if(length){
            string back = mescontrol(input_buf,tcpudp_map[ss.str()]);
            if((sendto(fd, back.c_str(), strlen(back.c_str()), 0, (struct sockaddr*) &src_addr, src_len)) == -1){
                printf("sendto message error: %s(errno: %d)\n",strerror(errno),errno);
                return;
            }
            }
            else{//client input "exit"
            cout<<"bye";
                tcpudp_map[ss.str()].erase();
                tcpudp_map[tcptoudp[ss.str()]].erase();
                tcptoudp[ss.str()].erase();
                shutdown(fd, SHUT_RDWR);
                close(fd);
            }
        }
        delete[] input_buf;
    }
    
    static void *threadinput(void *arg){
        char thread_buffer[1024];
        int thread_length;
        int fd= *((int *)arg);
        stringstream ss;
        sockaddr_in src_addr;
        socklen_t src_len = sizeof(src_addr);   
        /**************************************key transmitting********************************************/  
        getpeername(fd, (struct sockaddr*) &src_addr, &src_len);
        cout<<"New connection."<<endl;
        ss<<inet_ntoa(src_addr.sin_addr)<< ntohs(src_addr.sin_port);
        string tcp=ss.str();
        tcpudp_map[tcp]="undefined";
        sendto(fd, tcp.c_str(), strlen(tcp.c_str()), 0, (struct sockaddr*) &src_addr, src_len);
        /**************************************************************************************************/
        //Start serving the client
        while((thread_length = recv(fd, thread_buffer, 1024, 0))){
            thread_buffer[thread_length]='\0';
            cout<<thread_buffer<<endl;
            string back = mescontrol(thread_buffer,tcpudp_map[ss.str()]);
            send(fd, back.c_str(), strlen(back.c_str()), 0);
        }
        cout<<"Bye: "<<inet_ntoa(src_addr.sin_addr)<<":"<< ntohs(src_addr.sin_port)<<endl;
        tcpudp_map[ss.str()].erase();
        tcpudp_map[tcptoudp[ss.str()]].erase();
        tcptoudp[ss.str()].erase();
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    void listencontrol(){
        if(listen(sock_tcp, 10) == -1){
            printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
            exit(0);
        }
        cout<<"======waiting for client's request======\n";
        int *connfd=new int;
        while(1){
          poll(poll_vec.data(), poll_vec.size(), TIME_OUT);
          for (int i = 0; i < poll_vec.size(); i++)
          {
            if ((poll_vec[i].revents & POLL_IN) == POLL_IN){
                if(poll_vec[i].fd == sock_tcp){ //tcp connection
                  pthread_t p;
                  *connfd = accept(sock_tcp, (struct sockaddr*)NULL, NULL);
                  pthread_create(&p, nullptr, BBSserver::threadinput, connfd);
                  worker.emplace_back(p);
                  pack_pool.emplace_back(connfd);
                }
                else{ //udp mes control
                  inputcontrol(poll_vec[i].fd);
                }
            }
            else if(poll_vec[i].revents != 0){
                cout<<"Server : client disconnected"<<endl;
                shutdown(poll_vec[i].fd, SHUT_RDWR);
                close(poll_vec[i].fd);
                poll_vec.erase(poll_vec.begin() + i);
            }
          }
        }
    }
    
};

int main(int argc, char *argv[]){
    BBSserver server(atoi(argv[1]));
    server.listencontrol();
    return 0;
}