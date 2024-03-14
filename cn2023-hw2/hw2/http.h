typedef enum Method {UNSUPPORTED, GET, POST} Method;

typedef struct Header {
    char * name;
    char * value;
    struct Header *next;
} Header;

typedef struct Request {
    enum Method method;
    char * query_string;
    char * version;
    Header * headers;
    char * body;
    size_t body_size;
} Request;

typedef struct Response {
    char * version;
    int status_code;
    char * status_text;
    Header * headers;
    char * body;
    size_t body_size;
} Response;


Request * parse_request(const char * raw, int n);
Response * parse_response(const char * raw, int n);

char * find_header(char * key, Header * headers);

Request * generate_request(int method, char * query_string, 
                            char * content_type, int length, char * text,
                            char * hostname, int port, char * credential);
                            
Response * generate_response(char * version, int status_code, char * content_type, 
                                int length, char * text);

void stringify_request(Request * req, char * buf);
void stringify_response(Response * rsp, char * buf);

void free_request(Request * r);
void free_response(Response * r);
void free_header(Header * h);

//helper function
void lower_case(char * s);
