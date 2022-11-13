// Includes (necessary)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// Includes (maybe necessary)
#include <string.h>
#include <sys/un.h>
#include <stdbool.h>

#define DEBUG
// Constants
#define DEFAULT_PORT 80
#define MAX_BACKLOG 1
#define ASSERT_SUCCESS(v) if (v<0) exit(1);
#define BUFFER_SIZE 256
#define MEM_CHECK(p) if(p==NULL){\
fprintf(stderr, "Allocation failed near line %d.\n", __LINE__);\
exit(1);\
}

typedef struct key_value_pair key_value_pair;
struct key_value_pair {
    char* key;
    char* value;
    key_value_pair* next;
};

typedef enum {
    GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH
} HTTP_VERB;

typedef struct {
    HTTP_VERB verb;
    char* url;
    char* version;
    key_value_pair* kv_block;
    char* body; // optional, NULL if no body
//    void* free_on_lifetime_end; // to retain and free data source memory only if
    // http_request struct outlives its data source
} http_request;

typedef struct {
    char* version;
    int status_code;
    char* phrase;
    key_value_pair* kv_block;
    char* body
} http_response;

// Functions
bool handle_http_connection(int connection);

HTTP_VERB verb_str_to_enum(char* strtok);

int main(int argc, char** argv) {

    // Assign port
    unsigned short port = DEFAULT_PORT;

    // Parse commandline arguments
    if (argc > 1) {

        // Convert the passed arg to int
        int arg1 = atoi(argv[1]);
        // If conversion was successful
        if (arg1 > 0)
            // Assign this number as our port variable number
            port = arg1;

    }

    // Create socket
    int s = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_SUCCESS(s)

    // Create address struct
    struct sockaddr_in s_addr;

    // Create address descriptor
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind port number to socket
    int b = bind(s, (struct sockaddr*) &s_addr, sizeof(s_addr));
    ASSERT_SUCCESS(b)

    /*
     Marks the socket as a passive socket, that is,
     as a socket that will be used to accept incoming connections requests using accept
     */
    int l = listen(s, MAX_BACKLOG);
    ASSERT_SUCCESS(l)

    // Possibly loops over the connection handling so that a number of connections is handled sequentially
    {
        /* Extracts the first connection request in queue and creates a new connected socket,
         * returns a file descriptor referring to that socket. The newly created socket is not
         * in the listening state, the original socket is unaffected by this call.
         */
        int connection = accept(s, (struct sockaddr*) NULL, NULL);
        ASSERT_SUCCESS(connection)

        handle_http_connection(connection);

        // Clean up connection socket
        close(connection);
    }
    // Clean up listening socket
    close(s);

    return EXIT_SUCCESS;
}

// returns string with owned memory
char* read_until_empty_line(int connection);

bool parse_http_header(char* request, http_request* ptr);

bool request_has_body(HTTP_VERB verb) {
    const bool lookup[] = {
            0, 0, 1, 1, 1/* may */, 0, 0, 0, 1
    };
    return lookup[verb];
}

bool response_has_body(HTTP_VERB verb) {
    const bool lookup[] = {
            1, 0, 1, 1/* may */, 1/* may */, 1, 1, 0, 1
    };
    return lookup[verb];
}

http_response* handle_http_request(http_request* request);
// Recognize packages


// true on success false on failure
bool parse_http_header(char* request, http_request* ptr) {
    http_request parsed = {};
    {
        HTTP_VERB verb = verb_str_to_enum(strtok(request, " "));
        if (verb == -1) return false;
        parsed.verb = verb;
    }
    {
        char* url = strtok(NULL, " ");
        if (url == NULL) return false;
        parsed.url = url;
    }
    {
        char* version = strtok(NULL, "\r");
        strtok(NULL, "\n");
        if (version == NULL) return false;
        parsed.version = version;
    }
    {
        char* line;
        while (strlen(line = strtok(NULL, "\r")) > 1) {
            strtok(NULL, "\n");
            key_value_pair* kv = calloc(1, sizeof(key_value_pair));
            MEM_CHECK(kv)
            // insert into linked list
            // TODO: (maybe) Insert in not reversed order if that matters
            kv->next = parsed.kv_block;
            parsed.kv_block = kv;
            // parse single key value pair
            kv->key = line;
            char* v = strchr(line, ':');
            // If we didn't find a ':'; request is malformed, for now free all assigned elements in key_value_pair
            if (!(*v)) {
                for (key_value_pair* elem = parsed.kv_block; elem != NULL;) {
                    key_value_pair* next = elem->next;
                    free(elem);
                    elem = next;
                }
                return false;
            }
            do {
                *v = '\0';
                v++;
            } while ((*v) == ' '); // trims extra spaces
            kv->value = v;
        }
        strtok(NULL, "\n");
    }
    *ptr = parsed;
    return true;
}

char* verb_table[] = {"GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE", "PATCH"};
#ifdef DEBUG
#define debug_print_request(request) debug_print_request_func(request)
#define debug_print_response(response) debug_print_response_func(response)

void debug_print_request_func(http_request* request) {
    printf("method: %d %s\n", request->verb, verb_table[request->verb]);
    printf("url: %s\n", request->url);
    printf("version: %s\n", request->version);

    for (key_value_pair* current = request->kv_block; current != NULL; current = current->next) {
        printf("key: %s, value: %s\n", current->key, current->value);
    }
    if (request->body != NULL)
        printf("body: %s\n", request->body);
    else
        printf("no body\n");
}
void debug_print_response_func(http_response* response) {
    printf("version: %s\n", response->version);
    printf("status: %d\n", response->status_code);

    for (key_value_pair* current = response->kv_block; current != NULL; current = current->next) {
        printf("key: %s, value: %s\n", current->key, current->value);
    }
    if (response->body != NULL)
        printf("body: %s\n", response->body);
    else
        printf("no body\n");
}
#else
#define debug_print_request(request)
#endif
bool is_save_char(char c){
    return 'A'<=c&&c<='Z'||'a'<=c&&c<='z'||'0'<=c&&c<='9'||c=='_'||c=='-';
}
bool url_is_save(char* url){
    if(!strncmp(url, "/dynamic", 8 /* length of "/dynamic"*/))
        url+=8;
    else
        return false;
    bool save_sub_path=true;
    while(*url!='\0'){
        char c=*url;
        if(c=='/'){
            if(save_sub_path)
                save_sub_path=false;
            else
                return false;
        }
        else if(is_save_char(c)){
            save_sub_path=true;
        }
        else if (c == '.'){} // ignore
        else return false;
    }
    return save_sub_path;
}
int get_static(char* name, char** dest){
    if(strcmp(name, "foo"))*dest=strdup("Foo");
    else if(strcmp(name, "bar"))*dest=strdup("Bar");
    else if(strcmp(name, "baz"))*dest=strdup("Baz");
    else return 404; // not found
    return 200; // OK
}
int get_resource(char* url, char** dest){
    // handle simple urls
    if(url[0]!='/') return 400; // bad request
    if(url[1]=='\0') {  // GET nothing for "/"
        *dest=strdup(""); // TODO: find out what to answer to empty GET requests
        return 200;
    }
    // handle static content
    if(!strncmp(url, "/static/", 8 /* length of "/static/"*/))
        return get_static(url+8, dest);

    // handle dynamic content
    if(!url_is_save(url)){
        return 403; // forbidden
    }
    return 501; //TODO implement
}
http_response* handle_http_request(http_request* request) {
    debug_print_request(request);
    http_response* response_ptr= malloc(sizeof(http_response));
    switch (request->verb) {
        case GET: {
            char* resource;
            http_response response = {.status_code=get_resource(request->url, &resource)};
            response.body=resource;

            *response_ptr=response;
            return response_ptr;
        }
        default: {
            http_response response = {.status_code=400};
            *response_ptr=response;
            return response_ptr;
        }
    }
}

char* read_until_empty_line(int connection) {
    // Create a memory segment holding a full request that we can save the content
    // first saved in buffer into and then expand when needed
    char* request = calloc(BUFFER_SIZE, sizeof(char));
    MEM_CHECK(request)

    // Variables to indicate length of request memory and actually used space within request memory
    size_t size = BUFFER_SIZE;
    size_t used = 0;

    // Read from the bytestream into a buffer
    while (1) { // TODO: implement limit

        // Initiate an 'in between storage' buffer
        char buffer[BUFFER_SIZE];

        // Reads n bytes into said buffer
        int n = recv(connection, buffer, BUFFER_SIZE, 0);
        ASSERT_SUCCESS(n)

        /* If actually used up space in request memory + size of "to be written" buffer ('n') > than total size of
         * request memory try to double memory allocated for the request
         */
        if (used + n > size) {
            size *= 2;
            request = realloc(request, size);
            MEM_CHECK(request)
        }

        /* Copies buffer content into the request memory and
         * adjusts the variable indicating the size of the used memory accordingly
         */
        memcpy(request + used, buffer, n);
        used += n;

        /* While 'HTTP' end of message is not reached, keep reading the bytestream of the connection
         */
        if (0 == strncmp(request + used - 4, "\r\n\r\n", 4)) {
/*
#ifdef DEBUG
            if (used < size)
                request[used] = '\0';
            else
                request[size - 1] = '\0';
            printf("(%zu/%zu):\n%s\n", used, size, request);
#endif
 */
            return request;
        }
    }
}
bool handle_http_connection(int connection) {

    char* request = read_until_empty_line(connection);

    http_request parsed;
    if (!parse_http_header(request, &parsed))
        return false;

    if (request_has_body(parsed.verb)) {
        // TODO: may not end in empty line, use content length field instead
        parsed.body = read_until_empty_line(connection);
    }
    http_response* response = handle_http_request(&parsed);
    debug_print_response(response)
    return
}

HTTP_VERB verb_str_to_enum(char* str) {
    for (int i = 0; i < sizeof(verb_table) / sizeof(verb_table[0]); i++) {
        if (!strcmp(verb_table[i], str)) {
            return i;
        }
    }
    return -1;
}
