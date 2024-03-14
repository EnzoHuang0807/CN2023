#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <net/if.h>
#include <poll.h>
#include <unistd.h> 
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "http.h"
#include "utils/base64.h"

#define MAX 250000000
#define PATH 1000
#define FD_SIZE 10000
#define BUFF_SIZE MAX
#define ERR_EXIT(a){ perror(a); exit(1); }

int server_init(unsigned short port );
int connection_establish(int listen_fd);
void routing(Request * req, int conn_fd);
void fd_init(int fd, int * nfds, struct pollfd * fd_array, short events);

// helper function

int read_file(char * buffer, char * path);
void read_directory(char * buffer, char * path, char * url);
void write_file(char * buffer, int length, char * path);
void send_file(char * file_name, char * dir, Request * req, char * file, char * buffer, int conn_fd);
char * upload_file(char * dir, Request * req, char * buffer, int conn_fd);

void find_and_replace(char * key, char * src, char * dst);

int authorize(Request * req, char * file, char * buffer, int conn_fd);
void authorize_and_get(char * dst, Request * req, 
                        char * file, char * buf, int conn_fd);
void get_and_list(char * dst, char * dir, Request * req, 
                        char * file, char * buffer, char * table, int conn_fd);

char * encode_url(char* raw);
char * decode_url(char * raw);

void send_404(Request * req, char * buffer, int conn_fd);
void send_405(Request * req, char * buffer, int conn_fd, char * allow);

void * memmem(void * haystack, size_t haystack_len, 
                void * const needle, size_t needle_len);
char * content_type(char * file_name);


int main(int argc, char *argv[]){

    //check input
    if (argc != 2){
        fprintf(stderr, "Usage: ./server [port]\n");
        exit(-1);
    }

    //create directory
    mkdir("./web/files", 0775);
    mkdir("./web/tmp", 0775);
    mkdir("./web/videos", 0775);

    // Initialize server and listen_fd
    int listen_fd = server_init((unsigned short) atoi(argv[1]));

    // initialize fd_array
    int nfds = 0;
    struct pollfd fd_array[FD_SIZE];
    fd_init(listen_fd, &nfds, fd_array, POLLIN);

    //initialize request array
    Request * requests[FD_SIZE];
    for (int i = 0; i < FD_SIZE; i++)
        requests[i] = NULL;

    while (1) {
        
        // Check new connection
	    poll(fd_array, nfds, -1);

        for (int i = 0; i < nfds; i ++){

            if ((fd_array[i].revents & POLLIN) && (fd_array[i].fd == listen_fd)){
                
                int conn_fd = connection_establish(listen_fd);
                fd_init(conn_fd, &nfds, fd_array, POLLIN | POLLOUT);
            }

            else if (fd_array[i].revents & POLLIN){
                
                int conn_fd = fd_array[i].fd;

                // Receive message from server
                ssize_t n;
                char * buffer = malloc(BUFF_SIZE);
                Request * req = requests[conn_fd];

                if ((req == NULL) || 
                    (find_header("content-length", req -> headers) != NULL &&
                     atoi(find_header("content-length", req -> headers)) > req -> body_size)){

                    if((n = read(conn_fd, buffer, BUFF_SIZE)) < 0){
                        ERR_EXIT("read()");
                    }
                }
                
                if (n > 0){
                    if (requests[conn_fd] == NULL){

                        buffer[n] = '\0';
                        fprintf(stderr, 
                        "\nrequest received :   \n"
                        "-----------------------\n"
                        "%s\n"
                        "-----------------------\n",
                        buffer);

                        requests[conn_fd] = parse_request(buffer, n);
                    }
                    else{
                        memcpy(requests[conn_fd] -> body + requests[conn_fd] -> body_size, 
                        buffer, n);
                        requests[conn_fd] -> body_size += n;
                    }

                }

                free(buffer);
            }
            
            if (fd_array[i].revents & POLLOUT){
                
                int conn_fd = fd_array[i].fd;
                Request * req = requests[conn_fd];

                if (req != NULL){

                    // check if content has fully uploaded
                    if (find_header("content-length", req -> headers) != NULL &&
                        atoi(find_header("content-length", req -> headers)) > req -> body_size){
                        continue;
                    }

                    routing(req, conn_fd);

                    //Check connection
                    char * connection = find_header("connection", req -> headers);
                    lower_case(connection);

                    if (connection != NULL && strcmp(connection, "close") == 0)
                        close(conn_fd);

                    free_request(requests[conn_fd]);
                    requests[conn_fd] = NULL;
                }
            }
        }
    }
}

int server_init(unsigned short port){
    
    int listen_fd;
    struct sockaddr_in server_addr;

    // Set server address information
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    
    // Get socket file descriptor
    if((listen_fd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        ERR_EXIT("socket()");
    }
    // Bind the server file descriptor to the server address
    if(bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        ERR_EXIT("bind()");
    }
    // Listen on the server file descriptor
    if(listen(listen_fd , 3) < 0){
        ERR_EXIT("listen()");
    }

    return listen_fd;
}

int connection_establish(int listen_fd){
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    int conn_fd;

    // Accept the client and get client file descriptor
    if((conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, (socklen_t*)&client_addr_len)) < 0){
        ERR_EXIT("accept()");
    }

    fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, inet_ntoa(client_addr.sin_addr));
    return conn_fd;
}

void routing(Request * req, int conn_fd){

    char * file = malloc(BUFF_SIZE), * buffer = malloc(BUFF_SIZE), * table = malloc(BUFF_SIZE);
    char * url = req -> query_string;
    
    if (strcmp(url, "/") == 0){

        if (req -> method == GET){

            int n = read_file(file, "./web/index.html");
            file[n] = '\0';

            Response * rsp = generate_response(req -> version, 200,
                "text/html", strlen(file), file);
            
            fprintf(stderr, "\nSending reponse with status code 200\n");

            stringify_response(rsp, buffer);

            if(send(conn_fd, buffer, strlen(buffer), 0) < 0){
                ERR_EXIT("send()");
            }

            free_response(rsp);
        }
        else
            send_405(req, buffer, conn_fd, "GET");   
    }
    else if (strcmp(url, "/upload/file") == 0){ 

        if (req -> method == GET)
            authorize_and_get("./web/uploadf.html", req, file, buffer, conn_fd);
        else
            send_405(req, buffer, conn_fd, "GET");   
    }
    else if (strcmp(url, "/upload/video") == 0){

        if (req -> method == GET)
            authorize_and_get("./web/uploadv.html", req, file, buffer, conn_fd);
        else
            send_405(req, buffer, conn_fd, "GET");   
    }
    else if (strcmp(url, "/file/") == 0){

        if (req -> method == GET)
            get_and_list("./web/listf.rhtml", "./web/files/", 
            req, file, buffer, table, conn_fd);
        else
            send_405(req, buffer, conn_fd, "GET");   
    }
    else if (strcmp(url, "/video/") == 0){

        if (req -> method == GET)
            get_and_list("./web/listv.rhtml", "./web/videos/", 
            req, file, buffer, table, conn_fd);
        else
            send_405(req, buffer, conn_fd, "GET");   
    }
    else if (strncmp(url, "/video/", strlen("/video/")) == 0){

        if(req -> method == GET){

            char * video_name = decode_url(url + strlen("/video/"));
            int n = read_file(file, "./web/player.rhtml");
            file[n] = '\0';

            //replace
            char path[PATH];
            sprintf(path, "\"/api/video/%s/dash.mpd\"", url + strlen("/video/"));
            find_and_replace("<?MPD_PATH?>", file, path);
            find_and_replace("<?VIDEO_NAME?>", file, video_name);

            Response * rsp = generate_response(req -> version, 200,
                "text/html", strlen(file), file);
            
            fprintf(stderr, "\nSending reponse with status code 200\n");

            stringify_response(rsp, buffer);

            if(send(conn_fd, buffer, strlen(buffer), 0) < 0){
                ERR_EXIT("send()");
            }

            free(video_name);
            free_response(rsp);
        }
        else
            send_405(req, buffer, conn_fd, "GET");   
    }
    else if (strcmp(url, "/api/file") == 0){

        if (req -> method == POST){

            if(authorize(req, file, buffer, conn_fd) == 1){
                upload_file("./web/files/", req, buffer, conn_fd);
            }
        }
        else 
            send_405(req, buffer, conn_fd, "POST");
    }
    else if (strncmp(url, "/api/file/", strlen("/api/file/")) == 0){

        if(req -> method == GET){
            char * file_name = decode_url(url + strlen("/api/file/"));
            send_file(file_name, "./web/files/", req, file, buffer, conn_fd);
            free(file_name);
        }
        else
            send_405(req, buffer, conn_fd, "GET");   
    }
    else if (strcmp(url, "/api/video") == 0){

        if (req -> method == POST){

            if(authorize(req, file, buffer, conn_fd) == 1){

                char * file_name = upload_file("./web/tmp/", req, buffer, conn_fd);

                char src_path[100], dst_path[100];
                sprintf(src_path, "./web/tmp/%s", file_name);
                sprintf(dst_path, "./web/videos/%s", file_name);

                dst_path[strlen(dst_path) - strlen(".mp4")] = '\0';
                mkdir(dst_path, 0775);

                if (fork() == 0){
                    if (fork() == 0){
                        execl("./convert.sh", "convert.sh", src_path, dst_path, NULL);
                    }
                    exit(0);
                }
                wait(NULL);
            }
        }
        else 
            send_405(req, buffer, conn_fd, "POST");
    }
    else if (strncmp(url, "/api/video/", strlen("/api/video/")) == 0){

        if(req -> method == GET){
            char * file_name = decode_url(url + strlen("/api/video/"));
            send_file(file_name, "./web/videos/", req, file, buffer, conn_fd);
            free(file_name);
        }
        else
            send_405(req, buffer, conn_fd, "GET");   
    }
    else
        send_404(req, buffer, conn_fd);

    free(file);
    free(buffer);
    free(table);
}

void fd_init(int fd, int * nfds, struct pollfd * fd_array, short events){
    fd_array[*nfds].fd = fd;
    fd_array[*nfds].events = events;
    (*nfds) ++;
}

//-------------- Helper Function -------------

int read_file(char * buffer, char * path){

    int fd = open(path, O_RDONLY);

    if (fd != -1){

        int length = read(fd, buffer, BUFF_SIZE);
        close(fd);
        return length;
    }

   return -1;
}

void write_file(char * buffer, int length, char * path){

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    write(fd, buffer, length);
    close(fd);
}

void read_directory(char * buffer, char * path, char * url){

    DIR * d;
    struct dirent * dir;
    d = opendir(path);
    * buffer = '\0';

    if (d != NULL) {
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(url, "/file/") == 0 && dir -> d_type == DT_REG){
                
                    char * url = encode_url(dir -> d_name);
                    sprintf(buffer + strlen(buffer), 
                    "<tr><td><a href=\"/api/file/%s\">%s</a></td></tr>\n",
                    url , dir -> d_name);
                    free(url);
            }

            else if (strcmp(url, "/video/") == 0 && dir -> d_type == DT_DIR 
                && strchr(dir -> d_name, '.') == NULL){

                    char * url = encode_url(dir -> d_name);
                    sprintf(buffer + strlen(buffer), 
                    "<tr><td><a href=\"/video/%s\">%s</a></td></tr>\n",
                    url, dir -> d_name);
                    free(url);
            }
        }
        closedir(d);
    }
}

void find_and_replace(char * key, char * src, char * dst){
    char * prefix = strstr(src, key);
    char * suffix = prefix + strlen(key);
    
    //Add temporary space
    char * tmp = malloc(BUFF_SIZE);
    strcpy(tmp, suffix);
    strcpy(prefix, dst);
    strcpy(src + strlen(src), tmp);
    free(tmp);
}

int authorize(Request * req, char * file, char * buffer, int conn_fd){

    int valid  = 0;
    if (find_header("authorization", req -> headers)){

        size_t decode_length;
        char * encoded_data = find_header("authorization", req -> headers);

        // ignore "basic " prefix
        unsigned char * decoded_data = base64_decode(&encoded_data[6], 
            (size_t)strlen(encoded_data) - 6, &decode_length);

        // validation
        int n = read_file(file, "./secret");
        file[n] = '\0';

        char * tmp = file;
        while((tmp < file + strlen(file)) && valid == 0){
            
            size_t length = strcspn(tmp, "\n\0");
            if (memcmp(tmp, decoded_data, length) == 0) valid = 1;
            tmp += length + 1;
        }
    }

    if (!valid){

        Response * rsp = generate_response(req -> version, 401,
        "text/plain", strlen("Unauthorized\n"), "Unauthorized\n");

        stringify_response(rsp, buffer);

        if(send(conn_fd, buffer, strlen(buffer), 0) < 0){
            ERR_EXIT("send()");
        }

        free_response(rsp);
    }

    return valid;
}

void authorize_and_get(char * dst, Request * req, 
        char * file, char * buffer, int conn_fd){

    if (authorize(req, file, buffer, conn_fd) == 1){

        int n = read_file(file, dst);
        file[n] = '\0';

        Response * rsp = generate_response(req -> version, 200,
            "text/html", strlen(file), file);
        
        fprintf(stderr, "\nSending reponse with status code 200\n");

        stringify_response(rsp, buffer);

        if(send(conn_fd, buffer, strlen(buffer), 0) < 0){
            ERR_EXIT("send()");
        }

        free_response(rsp);
    }
}

void send_404(Request * req, char * buffer, int conn_fd){

    Response * rsp = generate_response(req -> version, 404,
                "text/plain", strlen("Not Found\n"), "Not Found\n");
    
    fprintf(stderr, "\nSending reponse with status code 404\n");
            
    stringify_response(rsp, buffer);

    if(send(conn_fd, buffer, strlen(buffer), 0) < 0){
        ERR_EXIT("send()");
    }

    free_response(rsp);
}

void send_405(Request * req, char * buffer, int conn_fd, char * allow){

    Response * rsp = generate_response(req -> version, 405,
                NULL, 0, allow);
    
    fprintf(stderr, "\nSending reponse with status code 405\n");
    
    stringify_response(rsp, buffer);
    
    if(send(conn_fd, buffer, strlen(buffer), 0) < 0){
        ERR_EXIT("send()");
    }
    
    free_response(rsp);
}

void get_and_list(char * dst, char * dir, Request * req,
        char * file, char * buffer, char * table, int conn_fd){

    int n = read_file(file, dst);
    file[n] = '\0';

    read_directory(table, dir, req -> query_string);

    if (strcmp(req -> query_string, "/file/") == 0)
        find_and_replace("<?FILE_LIST?>", file, table);
    else 
        find_and_replace("<?VIDEO_LIST?>", file, table);

    Response * rsp = generate_response(req -> version, 200,
        "text/html", strlen(file), file);

    fprintf(stderr, "\nSending reponse with status code 200\n");
        
    stringify_response(rsp, buffer);

    if(send(conn_fd, buffer, strlen(buffer), 0) < 0){
        ERR_EXIT("send()");
    }

    free_response(rsp);
}

char * decode_url(char * raw){

    char * res = malloc(PATH);
    char * tmp = res;
    char a, b;

    while (* raw) {

        if (* raw == '%'){

            a = raw[1], b = raw[2];

            if (a >= 'a')
                    a -= 'a'-'A';
            if (a >= 'A')
                    a -= ('A' - 10);
            else
                    a -= '0';
            if (b >= 'a')
                    b -= 'a'-'A';
            if (b >= 'A')
                    b -= ('A' - 10);
            else
                    b -= '0';
            * tmp ++ = 16 * a + b;
            raw += 3;
        } 

        else{
                * tmp ++ = * raw ++;
        }
    }

    * tmp ++ = '\0';
    return res;
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

void * memmem(void * haystack, size_t haystack_len, void * const needle, size_t needle_len){
    
    for (char *h = haystack; haystack_len >= needle_len; ++ h, -- haystack_len) {
        if (!memcmp(h, needle, needle_len))
            return h;
    }
    return NULL;
}

char * content_type(char * file_name){

    char * ext = strchr(file_name, '.');

    if (ext == NULL){
        return "text/plain";
    }

    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".rhtml") == 0)
        return "text/html";
    else if (strcmp(ext, ".mp4") == 0 || strcmp(ext, ".m4v") == 0)
        return "video/mp4";
    else if (strcmp(ext, ".m4s") == 0)
        return "video/iso.segment";
    else if (strcmp(ext, ".m4d") == 0)
        return "audio/mp4";
    else if (strcmp(ext, ".m4d") == 0)
        return "application/dash+xml";
    else 
        return "text/plain";
}

void send_file(char * file_name, char * dir, Request * req, 
                char * file, char * buffer, int conn_fd){
    
    char path[PATH];
    sprintf(path, "%s%s", dir, file_name);
    
    int n = read_file(file, path);
    if (n == -1 || strstr(file_name, "..") != NULL){
        send_404(req, buffer, conn_fd);
        return;
    }

    Response * rsp = generate_response(req -> version, 200,
        content_type(file_name), n, NULL);

    fprintf(stderr, "\nSending reponse with status code 200\n");
    
    stringify_response(rsp, buffer);

    if(send(conn_fd, buffer, strlen(buffer), 0) < 0){
        ERR_EXIT("send()");
    }

    for(int i = 0 ; i < n / 10000; i++){
        if(send(conn_fd, file + (i * 10000), 10000, 0) < 0)
            ERR_EXIT("send()");
    }

    if(send(conn_fd, file + n - (n % 10000), n % 10000, 0) < 0)
            ERR_EXIT("send()");

    free_response(rsp);
}

char * upload_file(char * dir, Request * req, char * buffer, int conn_fd){

    // Search for boundary
    char * boundary = find_header("content-type", req -> headers);
    boundary = strstr(boundary, "boundary=") + strlen("boundary=");

    //find start and end
    char * start = memmem(req -> body, req -> body_size, "\r\n\r\n", strlen("\r\n\r\n")) 
                + strlen("\r\n\r\n");
    char * end = memmem(start, req -> body_size - (start - req -> body), boundary, strlen(boundary)) 
                - strlen("\r\n--");

    //find file name
    char * file_name = memmem(req -> body, req -> body_size, "filename=\"", strlen("filename=\"")) 
        + strlen("filename=\"");
    char * file_end = memmem(file_name, req -> body_size - (file_name - req -> body), "\"", strlen("\""));
    * file_end = '\0';

    char path[PATH];
    sprintf(path, "%s%s", dir, basename(file_name));
    write_file(start, end - start, path);

    //Response OK
    Response * rsp;
    if (strcmp(dir, "./web/files/") == 0)
        rsp = generate_response(req -> version, 200,
            "text/plain", strlen("File Uploaded\n"), "File Uploaded\n");
    else
        rsp = generate_response(req -> version, 200,
            "text/plain", strlen("Video Uploaded\n"), "Video Uploaded\n");
    
    fprintf(stderr, "\nSending reponse with status code 200\n");
    
    stringify_response(rsp, buffer);

    if(send(conn_fd, buffer, strlen(buffer), 0) < 0){
        ERR_EXIT("send()");
    }

    free_response(rsp);
    return file_name;
}