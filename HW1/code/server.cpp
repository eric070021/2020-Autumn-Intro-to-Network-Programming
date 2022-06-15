#include <bits/stdc++.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TIME_OUT -1

using namespace std;

class BBSserver{
    private:
    int sock_tcp, sock_udp, connect_fd;
    int length;
    sockaddr_in address;
    char *input_buf;
    vector<pollfd> poll_vec;
    unordered_map<string,pair<string, string> > loginfile;
    unordered_map<string,string> useronline;
    unordered_map<string,string> tcpudp_map;
    unordered_map<string,string> tcptoudp;
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
    }

    ~BBSserver(){
        shutdown(sock_tcp, SHUT_RDWR);
        shutdown(sock_udp, SHUT_RDWR);
        close(sock_tcp);
        close(sock_udp);
        cout << "BBSserver shutdown" << endl;
    }
    int connection(){
        pollfd connect_fd;
        if( (connect_fd.fd = accept(sock_tcp, (struct sockaddr*)NULL, NULL)) == -1){
            printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
        }
        connect_fd.events = POLL_IN;
        connect_fd.revents = 0;
        poll_vec.push_back(connect_fd);
        return connect_fd.fd;
    }

    void inputcontrol(int fd){
        input_buf = new char[1024];
        string tmp;
        sockaddr_in src_addr;
        socklen_t src_len = sizeof(src_addr);    
        getpeername(fd,(struct sockaddr*) &src_addr, &src_len);
        if((length = recvfrom(fd, input_buf, 1024, 0, (struct sockaddr*) &src_addr, &src_len)) == -1){
            printf("recvform message error: %s(errno: %d)\n",strerror(errno),errno);
            return;
        }
        input_buf[length]='\0';
        stringstream ss;
        ss<<inet_ntoa(src_addr.sin_addr)<< ntohs(src_addr.sin_port);
        string inputstr=input_buf;
        if(tcpudp_map[inputstr] == "undefined"){
            stringstream key;
            key<<inputstr<<inet_ntoa(src_addr.sin_addr)<< ntohs(src_addr.sin_port);
            tcpudp_map[inputstr]=key.str();
            tcpudp_map[ss.str()]=key.str();
            tcptoudp[inputstr]=ss.str();
            string welcome="********************************\n** Welcome to the BBS server. **\n********************************";
            sendto(fd, welcome.c_str(), strlen(welcome.c_str()), 0, (struct sockaddr*) &src_addr, src_len);
        }
        else{
            if(length){
            string back = mescontrol(input_buf,tcpudp_map[ss.str()]);
            if((sendto(fd, back.c_str(), strlen(back.c_str()), 0, (struct sockaddr*) &src_addr, src_len)) == -1){
                printf("sendto message error: %s(errno: %d)\n",strerror(errno),errno);
                return;
            }
            }
            else{
                tcpudp_map[ss.str()].erase();
                tcpudp_map[tcptoudp[ss.str()]].erase();
                tcptoudp[ss.str()].erase();
                shutdown(fd, SHUT_RDWR);
                close(fd);
            }
        }
        delete[] input_buf;
    }
    void key_generate(){
         int connect_fd=connection();
         stringstream ss;
         sockaddr_in c;
         socklen_t cLen = sizeof(c);
         input_buf = new char[1024];
         getpeername(connect_fd, (struct sockaddr*) &c, &cLen);
         //cout<<"SERVER: Client connect from : IP: "<<inet_ntoa(c.sin_addr)<<" Ports : "<<ntohs(c.sin_port)<<endl;
         cout<<"New connection."<<endl;
         ss<<inet_ntoa(c.sin_addr)<< ntohs(c.sin_port);
         string tcp=ss.str();
         sendto(connect_fd, tcp.c_str(), strlen(tcp.c_str()), 0, (struct sockaddr*) &c, cLen);
         tcpudp_map[tcp]="undefined";
    }

    void listencontrol(){
        if(listen(sock_tcp, 10) == -1){
            printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
            exit(0);
        }
        cout<<"======waiting for client's request======\n";
        
        while(1){
          poll(poll_vec.data(), poll_vec.size(), TIME_OUT);
          for (int i = 0; i < poll_vec.size(); i++)
          {
            if ((poll_vec[i].revents & POLL_IN) == POLL_IN){
                if(poll_vec[i].fd == sock_tcp){
                  key_generate();             
                }
                else{
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
    string mescontrol(char *mes, string ss){
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
                    return "register successfully.";
                }
                else return "Username is already used.";
            }
            else{
                return "Usage: register <username> <email> <password>";
            }
        }
        else if(mes_vec[0]=="login"){//tcp
            if(mes_vec.size()==3){
                if(loginfile[mes_vec[1]].second!=mes_vec[2]){
                    return "Login failed.";
                }
                else if(useronline.find(ss)!=useronline.end()){
                   return "Please logout first.";
                }
                else{
                  string res="Welcome, "+mes_vec[1]+".";
                  useronline[ss]=mes_vec[1];
                  return res;
                }
            }
            else{
                return "Usage: login <username> <password>>";
            }
        }
        else if(mes_vec[0]=="logout"){//tcp
            if(mes_vec.size()>1) return "no need for parameter";
            else{
                if(useronline.find(ss)==useronline.end()) return "Please login first.";
                else {
                    string res="Bye, "+useronline[ss]+".";
                    useronline.erase(ss);
                    return res;
                }
            }
        }
        else if(mes_vec[0]=="whoami"){//udp
            if(mes_vec.size()>1) return "no need for parameter";
            else{
                if(useronline.find(ss)==useronline.end()) return "Please login first.";
                else return useronline[ss];
            }
        }
        else if(mes_vec[0]=="list-user"){//tcp
            if(mes_vec.size()>1) return "no need for parameter";
            else{
                string res="Name    Email\n";
                int i=0;
                for(auto it=loginfile.begin();it!=loginfile.end();it++,i++){
                    auto& p=it->second;
                    res+=it->first+"   "+p.first;
                    if(i<loginfile.size()-1) res+="\n";
                }
                return res;
            }
        }
        else if(mes_vec[0]=="exit"){//tcp
            if(mes_vec.size()>1) return "no need for parameter";
        }
        else {
            return "Command not found";
        }
    }
};

int main(int argc, char *argv[]){
    stringstream ss;
    string tmp=argv[1];
    int port;
    ss<<tmp;
    ss>>port;
    BBSserver server(port);
    server.listencontrol();
    return 0;
}