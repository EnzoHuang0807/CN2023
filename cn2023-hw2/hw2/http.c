#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "http.h"

#define MAX 250000000

// helper function
void parse_data(const char ** raw, char ** dst, char * keys);
void lower_case(char * s);
void add_header(Header ** h, char * name, char * value);
void add_startline_value(char ** dst, char * src);

Request * parse_request(const char * raw, int n) {

    const char * start = raw;  // store start position for raw
    Request * req = malloc(sizeof(struct Request));

    // Method
    size_t meth_len = strcspn(raw, " ");
    if (memcmp(raw, "GET", strlen("GET")) == 0)
        req -> method = GET;
    else if (memcmp(raw, "POST", strlen("POST")) == 0)
        req -> method = POST;
    else
        req -> method = UNSUPPORTED;
    
    raw += meth_len + 1; // move past <SP>

    // Request-query_string
    parse_data(&raw, &(req -> query_string), " ");

    // HTTP-Version
    parse_data(&raw, &(req -> version), "\r\n");

    Header *head = NULL, *cur = NULL, *prev = NULL;
    while (raw[0]!='\r') {

        prev = cur;
        cur = malloc(sizeof(Header));

        // name
        parse_data(&raw, &(cur -> name), ":");
        lower_case(cur -> name);
        while ( *raw == ' ') {
            raw ++;
        }

        // value
        parse_data(&raw, &(cur -> value), "\r\n");

        // next
        if (prev)
            prev -> next = cur;
        if (head == NULL)
            head = cur;
        cur -> next = NULL;
    }

    req -> headers = head;
    raw += 2; // move past <CR><LF>

    //body
    size_t length = n - (raw - start);
    req -> body = malloc(MAX);
    req -> body_size = length;
    memcpy(req -> body, raw, length);

    return req;
}

Response * parse_response(const char * raw, int n) {

    const char * start = raw;  // store start position for raw
    Response * rsp = malloc(sizeof(struct Response));

    // HTTP-Version
    parse_data(&raw, &(rsp -> version), " ");

    //status code;
    char * tmp;
    parse_data(&raw, &(tmp), " ");
    rsp -> status_code = atoi(tmp);
    free(tmp);

    // status text
    parse_data(&raw, &(rsp -> status_text), "\r\n");

    Header *head = NULL, *cur = NULL, *prev = NULL;
    while (raw[0]!='\r') {

        prev = cur;
        cur = malloc(sizeof(Header));

        // name
        parse_data(&raw, &(cur -> name), ":");
        lower_case(cur -> name);
        while ( *raw == ' ') {
            raw ++;
        }

        // value
        parse_data(&raw, &(cur -> value), "\r\n");

        // next
        if (prev)
            prev -> next = cur;
        if (head == NULL)
            head = cur;
        cur -> next = NULL;
    }

    rsp -> headers = head;
    raw += 2; // move past <CR><LF>

    //body
    size_t length = n - (raw - start);
    rsp -> body = malloc(MAX);
    rsp -> body_size = length;
    memcpy(rsp -> body, raw, length);

    return rsp;
}

void free_request(Request *r) {
    free(r -> query_string);
    free(r -> version);
    free_header(r -> headers);
    free(r -> body);
    free(r);
}

void free_response(Response *r) {
    free(r -> version);
    free(r -> status_text);
    free_header(r -> headers);
    free(r -> body);
    free(r);
}

void free_header(Header *h) {
    if (h != NULL) {
        free(h -> name);
        free(h -> value);
        free_header(h -> next);
        free(h);
    }
}

char * find_header(char * key, Header * headers){
    
    Header * tmp = headers;
    while(tmp != NULL){
        if (strcmp(tmp -> name, key) == 0)
            return tmp -> value;
        tmp = tmp -> next;
    }
    return NULL;
}

Request * generate_request(int method, char * query_string, 
                            char * content_type, int length, char * text,
                            char * hostname, int port, char * credential){

    Request * req = malloc(sizeof(Request));
    req -> headers = NULL;

    add_startline_value(&(req -> version), "HTTP/1.1");
    add_startline_value(&(req -> query_string), query_string);
    req -> method = method;

    add_header(&(req -> headers), "User-Agent", "CN2023Client/1.0");
    add_header(&(req -> headers), "Connection", "keep-alive");

    if (credential != NULL)
        add_header(&(req -> headers), "Authorization", credential);

    if(content_type != NULL){
        add_header(&(req -> headers),"Content-Type", content_type);
        add_startline_value(&(req -> body), text);
    }
    else
        req -> body = NULL;

    char content_length[50];
    sprintf(content_length, "%d", length);
    add_header(&(req -> headers), "Content-Length", content_length);

    char host[50];
    sprintf(host, "%s:%d", hostname, port);
    add_header(&(req -> headers), "Host", host);


    return req;
}

Response * generate_response(char * version, int status_code, 
                                char * content_type, int length, char * text){

    Response * rsp = malloc(sizeof(Response));
    rsp -> headers = NULL;
    rsp -> status_code = status_code;
    add_startline_value(&(rsp -> version), version);

    if(content_type != NULL){
        add_header(&(rsp -> headers), "Content-Type", content_type);
        add_startline_value(&(rsp -> body), text);
    }
    else
        rsp -> body = NULL;

    char content_length[50];
    sprintf(content_length, "%d", length);
    add_header(&(rsp -> headers), "Content-Length", content_length);

    add_header(&(rsp -> headers), "Server", "CN2023Server/1.0");

    if (status_code == 200)
        add_startline_value(&(rsp -> status_text), "OK");

    else if (status_code == 401){
        add_startline_value(&(rsp -> status_text), "Unauthorized");
        add_header(&(rsp -> headers), "WWW-Authenticate", "Basic realm=\"B10902068\"");
    }

    else if (status_code == 404)
        add_startline_value(&(rsp -> status_text), "Not Found");

    else if (status_code == 405){
        add_startline_value(&(rsp -> status_text), "Method Not Allowed");
        add_header(&(rsp -> headers), "Allow", text);
    }

    return rsp;
}

void stringify_request(Request * req, char * buf){

    char * method;
    if (req -> method == GET)
        method = "GET";
    else 
        method = "POST";

    sprintf(
        buf, 
        "%s %s %s\r\n",
        method , req -> query_string, req -> version
    );

    Header * tmp = req -> headers;
    while(tmp != NULL){
        sprintf(buf + strlen(buf), "%s: %s\r\n", tmp -> name, tmp -> value);
        tmp = tmp -> next;
    }
    sprintf(buf + strlen(buf), "\r\n");

    if (req -> body)
        sprintf(buf + strlen(buf), "%s", req -> body);
}

void stringify_response(Response * rsp, char * buf){

    sprintf(
        buf, 
        "%s %d %s\r\n",
        rsp -> version, rsp -> status_code, rsp -> status_text
    );

    Header * tmp = rsp -> headers;
    while(tmp != NULL){
        sprintf(buf + strlen(buf), "%s: %s\r\n", tmp -> name, tmp -> value);
        tmp = tmp -> next;
    }
    sprintf(buf + strlen(buf), "\r\n");

    if (rsp -> body)
        sprintf(buf + strlen(buf), "%s", rsp -> body);
}

// ------------------ Helper Function --------------------

void parse_data(const char ** raw, char ** dst, char * keys){

    size_t length = strcspn(*raw, keys);

    *dst = malloc(length + 1);
    memcpy(*dst, *raw, length);
    
    *(*dst + length) = '\0';
    *raw += length + strlen(keys);
} 

void lower_case(char * s){
    if (s == NULL)
        return;
    for ( ; *s; s++) *s = tolower(*s);
}

void add_header(Header ** h, char * name, char * value){

    Header * tmp = malloc(sizeof(Header));

    //name and value
    tmp -> name = malloc(strlen(name) + 1);
    tmp -> value = malloc(strlen(value) + 1);
    strcpy(tmp -> name, name);
    strcpy(tmp -> value, value);
    
    tmp -> next = *h;
    *h = tmp;
}

void add_startline_value(char ** dst, char * src){

    if (src == NULL){
        *dst = NULL;
        return;
    }

    *dst = malloc(strlen(src) + 1);
    strcpy(*dst, src);
}
