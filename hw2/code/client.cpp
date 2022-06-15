#include <bits/stdc++.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>

#define TIME_OUT -1

using namespace std;

class Client{
    private:
        int csock_tcp, csock_udp;
        sockaddr_in address;
        socklen_t addr_len = sizeof(address);        
        vector<pollfd> poll_vec;
        char *input_buf;
        int length;

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

    void sendmessage(string mes){
        if(mes[0]=='l' || mes[0]=='e') sendto(csock_tcp, mes.c_str(), strlen(mes.c_str()), 0, (sockaddr * ) &address, addr_len);
        else sendto(csock_udp, mes.c_str(), strlen(mes.c_str()), 0, (sockaddr * ) &address, addr_len);
    }

    void listenmessage(){
        poll(poll_vec.data(), poll_vec.size(), TIME_OUT);
        for (int i = 0; i < poll_vec.size(); i++) {
            if ((poll_vec[i].revents & POLL_IN) == POLL_IN) {
                input_buf = new char[1024];
                if((length = recv(poll_vec[i].fd, input_buf, 1024, 0)) == -1){
                     printf("recv message error: %s(errno: %d)\n",strerror(errno),errno);
                     return;
                }
                input_buf[length] = '\0';
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