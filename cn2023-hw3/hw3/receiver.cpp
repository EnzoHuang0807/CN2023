#include <iostream>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#include <zlib.h>
#include <openssl/evp.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <fcntl.h>

#include "def.h"

using namespace std;

int sock_fd;
struct sockaddr_in recv_addr;

struct segment buffer[MAX_SEG_BUF_SIZE];
int buffer_filled[MAX_SEG_BUF_SIZE];

int base = 1;
int seq_num_base = 1;

EVP_MD_CTX * sha256;
int n_bytes = 0;
unsigned char hash_256[EVP_MAX_MD_SIZE];
unsigned int hash_len;

void init_socket(char * recv_ip, int recv_port);
void init_recv(char * agent_ip, int agent_port);
void init_sha256();

int isAllReceived(int fin);
int isBufferFull();
int isCorrupt(segment &sgmt);
int isOverBuffer(int seqNumber);
void flush(char * filepath);
void endReceive();
void sendSACK(int ackNumber, int sackNumber, int is_fin);
void markSACK(segment &sgmt);
void updateBase();


void setIP(char *dst, char *src);
string hexDigest(const void *buf, int len);
void write_file(char * buffer, int length, char * path);

// ./receiver <recv_ip> <recv_port> <agent_ip> <agent_port> <dst_filepath>
int main(int argc, char *argv[]) {

    // parse arguments
    if (argc != 6) {
        cerr << "Usage: " << argv[0] << " <recv_ip> <recv_port> <agent_ip> <agent_port> <dst_filepath>" << endl;
        exit(1);
    }

    int recv_port, agent_port;
    char recv_ip[50], agent_ip[50];

    // read argument
    setIP(recv_ip, argv[1]);
    sscanf(argv[2], "%d", &recv_port);
    setIP(agent_ip, argv[3]);
    sscanf(argv[4], "%d", &agent_port);
    char * filepath = argv[5];

    init_socket(recv_ip, recv_port);
    init_recv(agent_ip, agent_port);
    init_sha256();

    while (true){

        socklen_t recv_addr_sz;
        segment sgmt{};
        recvfrom(sock_fd, &sgmt, sizeof(sgmt), 0, 
                    (struct sockaddr *)&recv_addr, &recv_addr_sz);

        if (isCorrupt(sgmt)){

            printf("drop\tdata\t#%d\t(corrupted)\n", sgmt.head.seqNumber);
            sendSACK(base - 1, base - 1, false);
        }
        else if(sgmt.head.seqNumber == base){

            if (sgmt.head.fin)
                printf("recv\tfin\n");
            else
                printf("recv\tdata\t#%d\t(in order)\n", sgmt.head.seqNumber);

            markSACK(sgmt);
            updateBase();
            sendSACK(base - 1, sgmt.head.seqNumber, sgmt.head.fin);

            if (isAllReceived(sgmt.head.fin)){
                flush(filepath);
                endReceive();
            }
            else if (isBufferFull()){
                flush(filepath);
            }
        }
        else{

            if (isOverBuffer(sgmt.head.seqNumber)){

                printf("drop\tdata\t#%d\t(buffer overflow)\n",
                        sgmt.head.seqNumber);
                sendSACK(base - 1, base - 1, false);
            }
            else{

                printf("recv\tdata\t#%d\t(out of order, sack-ed)\n",
                        sgmt.head.seqNumber);

                markSACK(sgmt);
                sendSACK(base - 1, sgmt.head.seqNumber, sgmt.head.fin);
            }
        }
    }
}


// -------------- Init Functions --------------

void init_socket(char * recv_ip, int recv_port){

    sock_fd = socket(PF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(recv_port);
    addr.sin_addr.s_addr = inet_addr(recv_ip);
    memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));    
    bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr));
}

void init_recv(char * agent_ip, int agent_port){

    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(agent_port);
    recv_addr.sin_addr.s_addr = inet_addr(agent_ip);
}

void init_sha256(){
    sha256 = EVP_MD_CTX_new();
    EVP_DigestInit_ex(sha256, EVP_sha256(), NULL);
}

// -------------- Functions --------------

void flush(char * filepath){

    printf("flush\n");

    for (int i = 0; i < MAX_SEG_BUF_SIZE; i++){

        // write file
        write_file(buffer[i].data, buffer[i].head.length, filepath);

        // update sha256
        EVP_DigestUpdate(sha256, buffer[i].data, buffer[i].head.length);
        n_bytes += buffer[i].head.length;
    }

    // calculating hash
    EVP_MD_CTX *tmp_sha256 = EVP_MD_CTX_new();
    EVP_MD_CTX_copy_ex(tmp_sha256, sha256);
    EVP_DigestFinal_ex(tmp_sha256, hash_256, &hash_len);
    EVP_MD_CTX_free(tmp_sha256);

    printf("sha256\t%d\t%s\n", n_bytes , hexDigest(hash_256, hash_len).c_str());

    // clear buffer
    seq_num_base += MAX_SEG_BUF_SIZE;
    bzero(&buffer, sizeof(segment) * MAX_SEG_BUF_SIZE);
    bzero(&buffer_filled, sizeof(int) * MAX_SEG_BUF_SIZE);
}

int isAllReceived(int fin){
    return fin;
}

void endReceive(){
    printf("finsha\t%s\n", hexDigest(hash_256, hash_len).c_str());
    exit(0);
}

int isBufferFull(){
    return (base >= seq_num_base + MAX_SEG_BUF_SIZE);
}

int isCorrupt(segment &sgmt){
    return (sgmt.head.checksum != 
                crc32(0L, (const Bytef *)sgmt.data, MAX_SEG_SIZE));
}

void sendSACK(int ackNumber, int sackNumber, int is_fin){

    segment tmp {};
    tmp.head.ack = 1;
    tmp.head.fin = is_fin;
    tmp.head.ackNumber = ackNumber;
    tmp.head.sackNumber = sackNumber;

    if (is_fin)
        printf("send\tfinack\n");
    else
        printf("send\tack\t#%d,\tsack\t#%d\n", ackNumber, sackNumber);

    sendto(sock_fd, &tmp, sizeof(tmp), 0, 
            (struct sockaddr *)&recv_addr, sizeof(sockaddr));
}

void markSACK(segment &sgmt){

    int seqNumber = sgmt.head.seqNumber;

    if (seqNumber < seq_num_base + MAX_SEG_BUF_SIZE &&
        seqNumber >= seq_num_base){

        buffer_filled[(seqNumber - 1) % MAX_SEG_BUF_SIZE] = 1;
        memcpy(&buffer[(seqNumber - 1) % MAX_SEG_BUF_SIZE], 
            &sgmt, sizeof(sgmt));
    }
}

void updateBase(){

    while(base < seq_num_base + MAX_SEG_BUF_SIZE &&
            buffer_filled[(base - 1) % MAX_SEG_BUF_SIZE]){
        base ++;
    }
}

int isOverBuffer(int seqNumber){
    return (seqNumber >= seq_num_base + MAX_SEG_BUF_SIZE);
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

string hexDigest(const void *buf, int len) {
    const unsigned char *cbuf = static_cast<const unsigned char *>(buf);
    ostringstream hx{};

    for (int i = 0; i != len; ++i)
        hx << hex << setfill('0') << setw(2) << (unsigned int)cbuf[i];

    return hx.str();
}

void write_file(char * buffer, int length, char * path){

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    write(fd, buffer, length);
    close(fd);
}

