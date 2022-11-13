#define DEBUG

#include "webserver.h"
// Constants
#define DEFAULT_PORT 80
#define MAX_BACKLOG 1
#define BUFFER_SIZE 256
#define RECV_TIMEOUT_SEC 10
#define RECV_TIMEOUT_USEC 0

// Global variables
size_t used = 0;
char *request_mem;
size_t request_mem_size;

int main(int argc, char **argv) {

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
    int b = bind(s, (struct sockaddr *) &s_addr, sizeof(s_addr));
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
        int connection = accept(s, (struct sockaddr *) NULL, NULL);
        ASSERT_SUCCESS(connection)

        /*
         * Sets the connection socket to time out if hasn't received requests for
         * a certain amount of time.
         */
        struct timeval conn_recv_timeout = {
                conn_recv_timeout.tv_sec = RECV_TIMEOUT_SEC,
                conn_recv_timeout.tv_usec = RECV_TIMEOUT_USEC
        };
        setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (char*)&conn_recv_timeout, sizeof(conn_recv_timeout));

        // Create a memory segment holding a full request that we can save the content
        // first saved in buffer into and then expand when needed
        request_mem = calloc(BUFFER_SIZE, sizeof(char));
        MEM_CHECK(request_mem)
        request_mem_size = BUFFER_SIZE;

        while(handle_http_connection(connection));

        // Clean up connection socket
        close(connection);
    }
    // Clean up listening socket
    close(s);

    return EXIT_SUCCESS;
}


bool handle_http_connection(int connection) {

    /*
     * Read from connection into request_mem but behind already used part if it exists,
     * return pointer to beginning of request_mem
     */
    char *request = read_until_empty_line(connection);

    /*
     * Try to find "\r\n\r\n" in request_mem, if found copy it into temporary buffer
     * and make a new memory with what remained in request_mem, free old request_mem
     */
    char *buffer = extract_until_empty_line(request);
    request = request_mem;
    if (!buffer) {
        // TODO: Respond 400
        return false;
    }

    http_request parsed;
    if (!parse_http_header(buffer, &parsed)) {
        free(buffer);
        return false;
    }

    if (request_has_body(parsed.verb)) {
        // TODO: may not end in empty line, use content length field instead
        // TODO: Check if request contains "Content-Length Header", if not return 400 and false
        parsed.body = extract_until_empty_line(request);
        if (!parsed.body) {
            // TODO: Respond 400
            /* Possibly do something else to handle messages that should contain a body but don't
             * seem to have one. One option would be to read and try again but how often would we
             * want to do that.
             */
            return false;
        }
    }

     http_response *response = handle_http_request(&parsed);
    /* if (response == NULL) return false;
     * debug_print_response(response);
     */
    free(buffer);
    return true;
}


// returns string with owned memory
char *read_until_empty_line(int connection) {

    // Read from the bytestream into a buffer
    while (1) { // TODO: implement limit

        // Initiate an 'in between storage' buffer
        char buffer[BUFFER_SIZE];

        // Reads n bytes into said buffer
        int n = recv(connection, buffer, BUFFER_SIZE, 0);
        // Checks if recv has timed out
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return request_mem;
        }
        ASSERT_SUCCESS(n)

        /* If actually used up space in request memory + size of "to be written" buffer ('n') > than total size of
         * request memory try to double memory allocated for the request
         */
        if (used + n > request_mem_size) {
            request_mem_size *= 2;
            request_mem = realloc(request_mem, request_mem_size);
            MEM_CHECK(request_mem)
        }

        /* Copies buffer content into the request memory and
         * adjusts the variable indicating the size of the used memory accordingly
         */
        memcpy(request_mem + used, buffer, n);
        used += n;

        /* While 'HTTP' end of message is not reached, keep reading the bytestream of the connection
         */
//        char* request_end=strstr(request, "\r\n\r\n");
        if (0 == strncmp(request_mem + used - 4, "\r\n\r\n", 4)) {
/*
#ifdef DEBUG
            if (used < size)
                request[used] = '\0';
            else
                request[size - 1] = '\0';
            printf("(%zu/%zu):\n%s\n", used, size, request);
#endif
 */
            return request_mem;
        }
    }
}


char *extract_until_empty_line(char *request) {
    char *buffer = NULL;
    size_t i = 0;
    while (i <= ((request_mem_size / sizeof(request[0])) - 4)) {
        if ((strncmp(request + i, "\r\n\r\n", 4)) != 0) {
            i++;
        } else {
            i = i + 4;
            // Make new buffer and copy content
            buffer = (char *) calloc(i, sizeof(request[0]));
            memcpy(buffer, request, i * sizeof(request[0]));

            // Make new buffer for remaining content and copy it
            size_t remaining = request_mem_size - i * sizeof(request[0]);
            request = (char *) calloc(max(remaining, BUFFER_SIZE),
                                      sizeof(request[0]));
            memcpy(request, request_mem + i, remaining);
            used = used - i;

            // Free old buffer and reassign pointers
            free(request_mem);
            request_mem = request;
            return buffer;
        }
    }
    return buffer;
}

// Recognize packages
// true on success false on failure
bool parse_http_header(char *request, http_request *ptr) {
    http_request parsed = {};
    {
        HTTP_VERB verb = verb_str_to_enum(strtok(request, " "));
        if (verb == -1) return false;
        parsed.verb = verb;
    }
    {
        char *url = strtok(NULL, " ");
        if (url == NULL) return false;
        parsed.url = url;
    }
    {
        char *version = strtok(NULL, "\r");
        strtok(NULL, "\n");
        if (version == NULL) return false;
        parsed.version = version;
    }
    {
        char *line;
        while (strlen(line = strtok(NULL, "\r")) > 1) {
            strtok(NULL, "\n");
            key_value_pair *kv = calloc(1, sizeof(key_value_pair));
            MEM_CHECK(kv)
            // insert into linked list
            // TODO: (maybe) Insert in not reversed order if that matters
            kv->next = parsed.kv_block;
            parsed.kv_block = kv;
            // parse single key value pair
            kv->key = line;
            char *v = strchr(line, ':');
            // If we didn't find a ':'; request is malformed, for now free all assigned elements in key_value_pair
            if (!(*v)) {
                for (key_value_pair *elem = parsed.kv_block; elem != NULL;) {
                    key_value_pair *next = elem->next;
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

http_response *handle_http_request(http_request *request) {
    debug_print_request(request);
    return NULL;
/*    http_response *response_ptr = malloc(sizeof(http_response));
    switch (request->verb) {
        case GET: {
            char *resource;
            http_response response = {.status_code=get_resource(request->url, &resource)};
            response.body = resource;

            response = r;
            break;
        }
        default: {
            http_response r = {.status_code=400};
            response = r;
            break;
        }
    }
    if (response.phrase == NULL)
        response.phrase = status_code_to_phrase(response.status_code);
    if (response.version == NULL)
        response.version = "HTTP/1.1";

    http_response *response_ptr = malloc(sizeof(http_response));
    return response_ptr;
*/
}


int get_resource(char *url, char **dest) {
    // handle simple urls
    if (url[0] != '/') return 400; // bad request
    if (url[1] == '\0') {  // GET nothing for "/"
        *dest = strdup(""); // TODO: find out what to answer to empty GET requests
        return 200;
    }
    // handle static content
    if (!strncmp(url, "/static/", 8 /* length of "/static/"*/))
        return get_static(url + 8, dest);

    // handle dynamic content
    if (!url_is_save(url)) {
        return 403; // forbidden
    }
    return 501; //TODO implement
}

int get_static(char *name, char **dest) {
    if (strcmp(name, "foo"))*dest = strdup("Foo");
    else if (strcmp(name, "bar"))*dest = strdup("Bar");
    else if (strcmp(name, "baz"))*dest = strdup("Baz");
    else return 404; // not found
    return 200; // OK
}

bool url_is_save(char *url) {
    if (!strncmp(url, "/dynamic", 8 /* length of "/dynamic"*/))
        url += 8;
    else
        return false;
    bool save_sub_path = true;
    while (*url != '\0') {
        char c = *url;
        if (c == '/') {
            if (save_sub_path)
                save_sub_path = false;
            else
                return false;
        } else if (is_save_char(c)) {
            save_sub_path = true;
        } else if (c == '.') {} // ignore
        else return false;
    }
    return save_sub_path;
}

bool is_save_char(char c) {
    return 'A' <= c && c <= 'Z' || 'a' <= c && c <= 'z' || '0' <= c && c <= '9' || c == '_' || c == '-';
}

HTTP_VERB verb_str_to_enum(char *str) {
    for (int i = 0; i < sizeof(verb_table) / sizeof(verb_table[0]); i++) {
        if (!strcmp(verb_table[i], str)) {
            return i;
        }
    }
    return -1;
}

char *status_code_to_phrase(int status_code) {
    switch (status_code) {
        case 200:
            return "OK";
        case 202:
            return "Accepted";
        case 204:
            return "No Content";
        case 400:
            return "Bad Request";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 501:
            return "Not Implemented";
        default:
            return "Status Code Not Implemented";
    }
}

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

size_t max(size_t a, size_t b) {
    if (a >= b) {
        return a;
    } else {
        return b;
    }
}

#ifdef DEBUG

void debug_print_request_func(http_request *request) {
    printf("method: %d %s\n", request->verb, verb_table[request->verb]);
    printf("url: %s\n", request->url);
    printf("version: %s\n", request->version);

    for (key_value_pair *current = request->kv_block; current != NULL; current = current->next) {
        printf("key: %s, value: %s\n", current->key, current->value);
    }
    if (request->body != NULL)
        printf("body: %s\n\n", request->body);
    else
        printf("no body\n\n");
}

void debug_print_response_func(http_response *response) {
    printf("version: %s\n", response->version);
    printf("status: %d\n", response->status_code);

    for (key_value_pair *current = response->kv_block; current != NULL; current = current->next) {
        printf("key: %s, value: %s\n", current->key, current->value);
    }
    if (response->body != NULL)
        printf("body: %s\n\n", response->body);
    else
        printf("no body\n\n");
}

#endif