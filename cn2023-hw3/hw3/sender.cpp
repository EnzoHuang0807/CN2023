#include <iostream>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#include <zlib.h>
#include <fcntl.h>
#include <vector>
#include <time.h>

#include "def.h"

using namespace std;
enum FSM {
    SS,
    CA
};

int sock_fd;
struct sockaddr_in recv_addr;
vector<segment> transmit_queue;
vector<int> sent_seq;
int base = 1;

double cwnd;
int thresh, dup_ack;
int state;
struct timespec start_time;
struct timespec end_time;


void init_socket(char * send_ip, int send_port);
void init_recv(char * agent_ip, int agent_port);
void init_queue(char * filepath);

void resetTimer();
void transmitNew(int num);
void transmitMissing();
void setState();
int markSACK(int sackNumber);
void updateBase();

void init();
void timeout();
void dupACK(segment &sgmt);
void newACK(segment &sgmt);
void leave(int seq_num);

// Helper Function
void setIP(char *dst, char *src);
long long time_elapsed();

// ./sender <send_ip> <send_port> <agent_ip> <agent_port> <src_filepath>
int main(int argc, char *argv[]) {

    // parse arguments
    if (argc != 6) {
        cerr << "Usage: " << argv[0] << " <send_ip> <send_port> <agent_ip> <agent_port> <src_filepath>" << endl;
        exit(1);
    }

    int send_port, agent_port;
    char send_ip[50], agent_ip[50];

    setIP(send_ip, argv[1]);
    sscanf(argv[2], "%d", &send_port);
    setIP(agent_ip, argv[3]);
    sscanf(argv[4], "%d", &agent_port);
    char * filepath = argv[5];

    init_socket(send_ip, send_port);
    init_recv(agent_ip, agent_port);
    init_queue(filepath);
    int max_seq_num = transmit_queue.size();

    // init stage
    init();

    while(true){

        // check if queue is emptied
        if (transmit_queue.size() == 0){
            leave(max_seq_num + 1);
        }

        // check timeout
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        if (time_elapsed() > TIMEOUT_MILLISECONDS){
            timeout();
        }

        socklen_t recv_addr_sz = sizeof(recv_addr);
        segment sgmt {};
        ssize_t length = 
            recvfrom(sock_fd, &sgmt, sizeof(sgmt), 0, 
                        (struct sockaddr *)&recv_addr, &recv_addr_sz);

        if (length > 0){

            printf("recv\tack\t#%d,\tsack\t#%d\n", sgmt.head.ackNumber , sgmt.head.sackNumber);

            if (sgmt.head.ackNumber < base){
                dupACK(sgmt);
            }
            else{
                newACK(sgmt);
            }
        }
    }
}

// -------------- Init Functions --------------

void init_socket(char * send_ip, int send_port){

    sock_fd = socket(PF_INET, SOCK_DGRAM, 0);

    // make nonblocking socket
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10;

    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, 
                (const char*)&timeout, sizeof(timeout));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(send_port);
    addr.sin_addr.s_addr = inet_addr(send_ip);
    memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));    
    bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr));
}

void init_recv(char * agent_ip, int agent_port){

    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(agent_port);
    recv_addr.sin_addr.s_addr = inet_addr(agent_ip);
}

void init_queue(char * filepath){

    char buffer[MAX_SEG_SIZE];
    int fd = open(filepath, O_RDONLY);
    int length = read(fd, buffer, MAX_SEG_SIZE);
    sent_seq.push_back(0);

    while(length > 0){

        segment tmp {};
        memcpy(tmp.data, buffer, length);

        tmp.head.length = length;
        tmp.head.seqNumber = transmit_queue.size() + 1;
        tmp.head.checksum = crc32(0L, (const Bytef *)tmp.data, MAX_SEG_SIZE);

        transmit_queue.push_back(tmp);
        sent_seq.push_back(0);
        
        length = read(fd, buffer, MAX_SEG_SIZE);
    }
}

// -------------- Event Functions --------------

void resetTimer(){
    clock_gettime(CLOCK_MONOTONIC, &start_time);
}

void transmitNew(int num){

    for (int i = (int)cwnd - num; i < (int)cwnd; i++){

        if (transmit_queue.size() > i){

            if(sent_seq[transmit_queue[i].head.seqNumber] == 0)
                printf("send\tdata\t#%d,\twinSize = %d\n", 
                    transmit_queue[i].head.seqNumber , int(cwnd));
            else
                printf("resnd\tdata\t#%d,\twinSize = %d\n", 
                    transmit_queue[i].head.seqNumber , int(cwnd));

            sendto(sock_fd, &transmit_queue[i], sizeof(segment), 0, 
                (struct sockaddr *)&recv_addr, sizeof(sockaddr));

            sent_seq[transmit_queue[i].head.seqNumber] = 1;
        }
    }
}

void transmitMissing(){

    printf("resnd\tdata\t#%d,\twinSize = %d\n", 
        transmit_queue[0].head.seqNumber , int(cwnd));

    sendto(sock_fd, &transmit_queue[0], sizeof(segment), 0, 
            (struct sockaddr *)&recv_addr, sizeof(sockaddr));
}

void setState(FSM s){
    state = s;
}

int isAtState(FSM s){
    return (s == state);
}

int markSACK(int sackNumber){

    int windowSpace = 0;
    for (int i = 0; i < transmit_queue.size(); i++){
        if (transmit_queue[i].head.seqNumber == sackNumber){
            transmit_queue.erase(transmit_queue.begin() + i);
            windowSpace = (i < cwnd)? 1 : 0;
            break;
        }
    }
    return windowSpace;
}

void updateBase(){

    if (transmit_queue.size() > 0)
        base = transmit_queue[0].head.seqNumber;
}

// -------------- Events --------------

void init(){

    cwnd = 1;
    thresh = 16;
    dup_ack = 0;

    transmitNew(1);
    resetTimer();
    setState(SS);
}

void timeout(){

    thresh = max(1, (int)(cwnd / 2));
    cwnd = 1;
    dup_ack = 0;

    printf("time\tout,\tthreshold = %d,\twinSize = %d\n", thresh , (int)cwnd);

    transmitMissing();                                                                                                                                                                                                                                                                                                                        
    setState(SS);
}

void dupACK(segment &sgmt){

    dup_ack ++;
    int num = markSACK(sgmt.head.sackNumber);
    transmitNew(num);

    if (dup_ack == 3)
        transmitMissing();
}

void newACK(segment &sgmt){

    dup_ack = 0;
    int num = markSACK(sgmt.head.sackNumber);
    updateBase();

    if (isAtState(SS)){
        num ++;
        cwnd ++;           // exponential increasing
        if (cwnd >= thresh)
            setState(CA);
    }
    else if (isAtState(CA)){
        num += (int)(cwnd + (double) 1 / (int) cwnd) - (int) cwnd;
        cwnd += (double) 1 / (int) cwnd;    // heuristic linear increase
    }

    transmitNew(num);
    resetTimer();
}

void leave(int seq_num){

    segment tmp {};

    tmp.head.length = 0;
    tmp.head.fin = 1;
    tmp.head.seqNumber = seq_num;
    tmp.head.checksum = crc32(0L, (const Bytef *)tmp.data, MAX_SEG_SIZE);


    printf("send\tfin\n");
    sendto(sock_fd, &tmp, sizeof(segment), 0, 
            (struct sockaddr *)&recv_addr, sizeof(sockaddr));

    int cnt = 0;
    while (true){

        tmp = {};
        socklen_t recv_addr_sz = sizeof(recv_addr);
        ssize_t length = recvfrom(sock_fd, &(tmp), sizeof(segment), 0, 
                                (struct sockaddr *)&recv_addr, &recv_addr_sz);

        if (length > 0){
            if (tmp.head.fin == 1 && tmp.head.ack == 1){
                printf("recv\tfinack\n");
                exit(0);
            }
            else{
                printf("recv\tack\t#%d,\tsack\t#%d\n", tmp.head.ackNumber , tmp.head.sackNumber);
            }
        }
    }
}

// -------------- Helper Functions --------------

void setIP(char *dst, char *src){
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost") == 0){
        sscanf("127.0.0.1", "%s", dst);
    }
    else{
        sscanf(src, "%s", dst);
    }
    return;
}

long long time_elapsed() {
    return (end_time.tv_sec - start_time.tv_sec) * 1000LL + 
        (end_time.tv_nsec - start_time.tv_nsec) / 1000000LL;
}
