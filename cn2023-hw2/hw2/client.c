#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <libgen.h>
#include <unistd.h> 
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h> 
#include <sys/stat.h>

#include "http.h"
#include "utils/base64.h"

#define MAX 250000000
#define CMD_LEN   100
#define PATH     1000
#define BUFF_SIZE MAX
#define ERR_EXIT(a){ perror(a); exit(1);}

int connection_establish(char * ip, unsigned short port);


int read_file(char * buffer, char * path);
void write_file(char * buffer, int length, char * path);
int send_file(int type, char * file_name, char * file, char * buffer,
                char * host, int port, char * credential);

char * encode_url(char* raw);
char * content_type(char * file_name);
int receive_response(char * buffer, char * ip, int port);
void parse_input(char * line, char ** cmd, char ** file_name);

//server fd
int conn_fd;

int main(int argc , char *argv[]){

    //check input
    if (argc != 4){
        fprintf(stderr, "Usage: ./client [host] [port] [username:password]\n");
        exit(-1);
    }

    char * host = argv[1];
    int port = (int) atoi(argv[2]);
    char * credential = argv[3];

    // make directory
    mkdir("./files", 0775);

    //encode
    size_t encode_length;
    char * encoded_data = base64_encode(credential, 
            (size_t) strlen(credential), &encode_length);

    char encoded_credential[encode_length + strlen("Basic ") + 1];
    strcpy(encoded_credential, "Basic ");
    strncat(encoded_credential + strlen("Basic "), encoded_data, encode_length);
    encoded_credential[encode_length + strlen("Basic ")] = '\0';


    //convert hostname to ip
    struct hostent *host_entry;
    host_entry = gethostbyname(host);
    char * ip = inet_ntoa(*((struct in_addr*) host_entry -> h_addr_list[0]));

    //connection
    //fprintf(stderr, "connect to ip = %s\n", ip);
    conn_fd = connection_establish(ip, port);

    char * buffer = malloc(BUFF_SIZE), * file = malloc(BUFF_SIZE);

    while(1){

        printf("> ");

        //load input
        char * line = malloc(CMD_LEN);
        size_t len = CMD_LEN;
        int nread = getline(&line, &len, stdin);
        *(line + nread) = '\0' ;

        //parse
        char * cmd, * file_name;
        parse_input(line, &cmd, &file_name);

        //fprintf(stderr, "input : %s ; %s\n", cmd, file_name);

        int success = 0;

        if (strcmp(cmd, "put") == 0){

            if (strcmp(file_name, "") == 0){
                fprintf(stdout, "Usage: put [file]\n");
                continue;
            }

            else if (send_file(0, file_name, file, buffer, host, port, encoded_credential) == 0)
                success = receive_response(buffer, ip, port);
        }
        else if (strcmp(cmd, "putv") == 0){

            if (strcmp(file_name, "") == 0){
                fprintf(stdout, "Usage: putv [file]\n");
                continue;
            }

            else if (send_file(1, file_name, file, buffer, host, port, encoded_credential) == 0)
                success = receive_response(buffer, ip, port);
        }

        else if (strcmp(cmd, "get") == 0){

            if (strcmp(file_name, "") == 0){
                fprintf(stdout, "Usage: get [file]\n");
                continue;
            }

            else{

                Request * req;
                char path[PATH];
                char * tmp = encode_url(file_name);
                sprintf(path, "/api/file/%s", tmp);
                free(tmp);

                req = generate_request(GET, path, NULL, 0, NULL, host, port, NULL);
                stringify_request(req, buffer);

                // send request
                if(send(conn_fd, buffer, strlen(buffer), 0) < 0){
                    ERR_EXIT("send()");
                }

                // save response file
                ssize_t n = 0;
                while(n == 0){
                    if((n = read(conn_fd, buffer, BUFF_SIZE)) < 0){
                        ERR_EXIT("read()");
                    }
                }

                Response * rsp = parse_response(buffer, n);
            
                while (atoi(find_header("content-length", rsp -> headers)) > rsp -> body_size){
                
                    if((n = read(conn_fd, rsp -> body + rsp -> body_size, BUFF_SIZE - rsp -> body_size)) < 0){
                        ERR_EXIT("read()");
                    }
                    rsp -> body_size += n;
                }

                if (rsp -> status_code == 200){
                    success = 1;

                    char path[PATH];
                    sprintf(path, "./files/%s", file_name);
                    write_file(rsp -> body, rsp -> body_size, path);
                }

                if (find_header("connection", rsp -> headers) != NULL && 
                    strcmp(find_header("connection", rsp -> headers), "close") == 0){ 
                        // restart connection 
                        conn_fd = connection_establish(ip, port); 
                }

                free(rsp);
                free(req);
            }
        }
        else if (strcmp(cmd, "quit") == 0){
            close(conn_fd);
            fprintf(stdout, "Bye.\n");
            return 0;
        }
        else{
            fprintf(stderr, "Command Not Found.\n");
            continue;
        }

        if (success)
            fprintf(stdout, "Command succeeded.\n");
        else
            fprintf(stderr, "Command failed.\n");
        
        free(line);
    }

}

int connection_establish(char * ip, unsigned short port){

    int sock_fd;
    struct sockaddr_in addr;

    // Get socket file descriptor
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        ERR_EXIT("socket()");
    }

    // Set server address
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);

    // Connect to the server
    if(connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        ERR_EXIT("connect()");
    }

    return sock_fd;
}

int read_file(char * buffer, char * path){

   int fd = open(path, O_RDONLY);
   if (fd != -1){

        int length = read(fd, buffer, BUFF_SIZE);
        close(fd);
        return length;

   }
   return -1;
}

int send_file(int type, char * file_name, char * file, char * buffer,
                char * host, int port, char * credential){
    
    int n = read_file(file, file_name);
    if (n == -1)
        return -1;

    char * multipart = "multipart/form-data; boundary=----boundary";
    char * boundary = "----boundary";
    
    char field[PATH];
    sprintf(field, 
    "Content-Disposition: form-data; name=\"upfile\"; filename=\"%s\"\r\n"
    "Content-Type: %s\r\n", 
    basename(file_name), content_type(file_name));


    int info_length = strlen("--") + strlen(boundary) + strlen("\r\n") + 
                      strlen(field) + strlen("\r\n") +
                      strlen("\r\n--") + strlen(boundary) + strlen("--");

    Request * req;
    if (type == 0)
        req = generate_request(POST, "/api/file", multipart,
                                n + info_length, NULL, host, port, credential);
    else
        req = generate_request(POST, "/api/video", multipart,
                                n + info_length, NULL, host, port, credential);
     
    stringify_request(req, buffer);

    if(send(conn_fd, buffer, strlen(buffer), 0) < 0){
        ERR_EXIT("send()");
    }

    sprintf(buffer, "--%s\r\n%s\r\n", boundary, field);

    if(send(conn_fd, buffer, strlen(buffer), 0) < 0){
        ERR_EXIT("send()");
    }

    for(int i = 0 ; i < n / 10000; i++){
        if(send(conn_fd, file + (i * 10000), 10000, 0) < 0)
            ERR_EXIT("send()");
    }

    if(send(conn_fd, file + n - (n % 10000), n % 10000, 0) < 0)
            ERR_EXIT("send()");

    sprintf(buffer, "\r\n--%s--", boundary);

    if(send(conn_fd, buffer, strlen(buffer), 0) < 0){
        ERR_EXIT("send()");
    }

    free_request(req);
    return 0;
}

char * content_type(char * file_name){

    int index = strcspn(file_name, ".") + 1;
    char * ext = file_name + index;

    if (ext > file_name + strlen(file_name))
        return "text/plain";

    if (strcmp(ext, "html") == 0 || strcmp(ext, "rhtml") == 0)
        return "text/html";
    else if (strcmp(ext, "mp4") == 0 || strcmp(ext, "m4v") == 0)
        return "video/mp4";
    else if (strcmp(ext, "m4s") == 0)
        return "video/iso.segment";
    else if (strcmp(ext, "m4d") == 0)
        return "audio/mp4";
    else if (strcmp(ext, "m4d") == 0)
        return "application/dash+xml";
    else 
        return "text/plain";
}

void parse_input(char * line, char ** cmd, char ** file_name){

    while (isspace(*line))
        line ++;

    *cmd = line;

    // end for cmd
    while (!isspace(*line))
        line ++;

    *line = '\0';
    line ++;

    while (isspace(*line))
        line ++;
    * file_name = line;

    // end for file_name
    while (!(*line == '\n' || *line == '\0'))
        line ++;

    *line = '\0';
}

int receive_response(char * buffer, char * ip, int port){

    int success = 1;
    ssize_t n = 0;
    while(n == 0){
        if((n = read(conn_fd, buffer, BUFF_SIZE)) < 0){
            ERR_EXIT("read()");
        }
    }

    Response * rsp = parse_response(buffer, n);

    //fprintf(stderr, "status code : %d\n", rsp -> status_code);

    if (rsp -> status_code != 200)
        success = 0;

    if (find_header("connection", rsp -> headers) != NULL && 
        strcmp(find_header("connection", rsp -> headers), "close") == 0){ 
            // restart connection 
            conn_fd = connection_establish(ip, port); 
    }

    free(rsp);
    return success;
}

void write_file(char * buffer, int length, char * path){

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    write(fd, buffer, length);
    close(fd);
}

char * encode_url(char* raw){

    char * res = malloc(PATH);
    const char *hex = "0123456789abcdef";
    
    int pos = 0;
    for (int i = 0; i < strlen(raw); i++) {
        if (('a' <= raw[i] && raw[i] <= 'z')
            || ('A' <= raw[i] && raw[i] <= 'Z')
            || ('0' <= raw[i] && raw[i] <= '9')
            || raw[i] == '-' || raw[i] == '_' || raw[i] == '.'
            || raw[i] == '~' || raw[i] == '/') {

                res[pos++] = raw[i];
        } 

        else{
            res[pos++] = '%';
            res[pos++] = hex[raw[i] >> 4];
            res[pos++] = hex[raw[i] & 15];
        }
    }

    res[pos] = '\0';
    return res;
}