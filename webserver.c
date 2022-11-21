#define DEBUG

#include "webserver.h"
// Constants
#define DEFAULT_PORT 80
#define MAX_BACKLOG 1
#define BUFFER_SIZE 256
#define RECV_TIMEOUT_SEC 0
#define RECV_TIMEOUT_USEC 0
#define MAX_URL_LENGTH 256
#define RELATIVE_RESOURCE_PATH "/.."

// Global variables
size_t used = 0;
char* request_mem;
size_t request_mem_size;
int conn_state = 0;
http_request* curr_request;
dynamic_content *dynamicContent;


// TODO: check with python test
int main(int argc, char** argv) {

    // initialize dynamicContent
    dynamicContent = calloc(1, sizeof(dynamic_content));
    dynamicContent->name = NULL;
    dynamicContent->content = NULL;
    dynamicContent->next = NULL;

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

    /* Possibly loops over the connection handling so that a number of connections is handled sequentially
     * Not needed according to Tutor on ISIS because we only need to handle one client (= one connection).
     */
    // TODO: Shut down after a few minutes of no connections?
    while (1){

        // TODO: Check why these variables have to be nulled and whether we shouldn't do that before ourselves
        // NULL global variables
        used = 0;
        request_mem = NULL;
        request_mem_size = 0;
        conn_state = 0;
        curr_request = NULL;

        /* Extracts the first connection request in queue and creates a new connected socket,
         * returns a file descriptor referring to that socket. The newly created socket is not
         * in the listening state, the original socket is unaffected by this call.
         */
        int connection = accept(s, (struct sockaddr*) NULL, NULL);
        ASSERT_SUCCESS(connection)

        /*
         * Sets the connection socket to time out if it hasn't received requests for
         * a certain amount of time.
         */
        struct timeval conn_recv_timeout = {
                conn_recv_timeout.tv_sec = RECV_TIMEOUT_SEC,
                conn_recv_timeout.tv_usec = RECV_TIMEOUT_USEC
        };
        int sopt = setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (char*) &conn_recv_timeout,
                              sizeof(conn_recv_timeout));
        ASSERT_SUCCESS(sopt)

        // Create a memory segment holding a full request that we can save the content
        // first saved in buffer into and then expand when needed
        request_mem = calloc(BUFFER_SIZE, sizeof(char));
        MEM_CHECK(request_mem)
        request_mem_size = BUFFER_SIZE;

        handle_http_connection(connection);

        // Clean up connection socket
        free(request_mem);
        close(connection);
    }
    // Clean up listening socket
    close(s);

    return EXIT_SUCCESS;
}

int handle_http_connection(int connection){
    //while(1) // test scheinen zu wollen, dass die connection geschlossen wird, da sie sonst getimeoutet wurden
    {

        // On SUCCESS or NO_CRLF we receive bytes again in hope of retrieving the next message or full message
        if (conn_state == SUCCESS || conn_state == NO_CRLF) {

            // Tries to process requests that are already inside request_mem first
            while((conn_state = handle_http_request(connection)) == SUCCESS);

            /*
             * Previous while statement will end with NO_CRLF unless there has been an error processing an http_request or
             * a body hasn't been fully read, so in any normal case try to read more bytes / the next request .. any other
             * case we skip this part to handle the error code in the next loop of the while statement
             */
            if (conn_state == NO_CRLF){

                // Else we try to receive the next bytestream. If nothing is sent we can assume that the connection can be closed
                if(receive_bytes(connection) == TIMEOUT) return TIMEOUT;

                // If we received bytes we can try to handle the received request
                conn_state = handle_http_request(connection);

            }

        // If we had fault while parsing the header or there was no CL-Header the request was incorrect
        } else if (conn_state == PARSE_HEADER_FAULT || conn_state == CONTENT_LENGTH_KV_FAULT){
            //TODO: Respond 400 Bad Request
            return conn_state;

        /*
         * If we were able to successfully fill in the body of an http request even though we couldn't at first
         * we reflect that in conn_state so the next iteration can take the appropriate next action
         */
        } else if (conn_state == BODY_SUCCESS) {
            conn_state = respond_to_request(curr_request, connection);
            free(curr_request->body);

        // If we had a response error we can return the error code here
        } else if (conn_state == R_GET_FAULT || conn_state == R_STRING_FAULT || conn_state == R_SEND_FAULT) {
            return conn_state;

        // In this case we still have conn_state many bytes to retrieve for the body of curr_request
        } else if (conn_state > 0){

            // We try to receive more bytes for the body. If nothing is sent we can assume that the connection can be closed
            if(receive_bytes(connection) == TIMEOUT) return TIMEOUT;

            conn_state = extract_n_from_req_mem(curr_request, conn_state);
        }
    }

}

int handle_http_request(int connection) {

    /*
     * Tries to read a http request from memory
     */
    char* buffer = extract_until_empty_line(request_mem);
    if (!buffer) {
        return NO_CRLF;
    }

    /*
     * Tries to parse said request
     */
    http_request parsed;
    if (!parse_http_header(buffer, &parsed)) {
        free(buffer);
        return PARSE_HEADER_FAULT;
    }

    /*
     * Since request has been parsed correctly we will work with it further and need to assign it to the global variable
     * in order to do that
     */
    curr_request = &parsed;

    /*
     * Tries to get the body if said request should have one
     */
    if (request_has_body(parsed.method)) {
        if(parsed.body_size == 0){
            return CONTENT_LENGTH_KV_FAULT;
        }
        /*
         * Allocate memory for the body
         */
        parsed.body = (char*) calloc(parsed.body_size, sizeof(char));
        MEM_CHECK(parsed.body)
        /*
         * Extract data from request_mem into the body
         */
        int x = extract_n_from_req_mem(&parsed, parsed.body_size);
        if(x > 0){
            return x;
        }
    }

    /*
     * Handles the response to the request
     */
    respond_to_request(&parsed, connection);

    /*
     * Cleanup
     */
    free(buffer);
    free(parsed.body);
    // TODO: Free KV pair memory
    curr_request = NULL;
    return SUCCESS;
}

// returns string with owned memory
int receive_bytes(int connection) {

    // Initiate an 'in between storage' buffer
    char buffer[BUFFER_SIZE];

    // Reads n bytes into said buffer
    int n = recv(connection, buffer, BUFFER_SIZE, 0);
    // Checks if recv has timed out
    if ((n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) || n == 0) {
        return TIMEOUT;
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

    return n;
    /* While 'HTTP' end of message is not reached, keep reading the bytestream of the connection
     */
    /*char* request_end strstr(request_mem, "\r\n\r\n");
    if (request_end) {
        return request_mem;
        stringstrinstring\r\n\r\n
    }*/
    /*if (0 == strncmp(request_mem + used - 4, "\r\n\r\n", 4)) {
        return request_mem;
    }*/

}


char* extract_until_empty_line(char* request) {
    char* buffer = NULL;
    size_t i = 0;
    while (i <= ((request_mem_size / sizeof(request[0])) - 4)) {
        if ((strncmp(request + i, "\r\n\r\n", 4)) != 0) {
            i++;
        } else {
            i = i + 4;
            // Make new buffer and copy content
            buffer = (char*) calloc(i, sizeof(request[0]));
            memcpy(buffer, request, i * sizeof(request[0]));

            // Make new buffer for remaining content and copy it
            size_t remaining = request_mem_size - i * sizeof(request[0]);
            request_mem_size = max(remaining, BUFFER_SIZE);
            request = (char*) calloc(request_mem_size,sizeof(request[0]));
            MEM_CHECK(request)
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

int extract_n_from_req_mem(http_request* request, size_t n){

    // Makes sure we're not trying to copy more than what is available in request_mem
    size_t tmp = min(used, n);

    /*
     * Makes sure the remaining available space in request->body is not too small,
     * it shouldn't be though as it's initialized with n
     */
    //if((request->body_size-request->body_used) > tmp){
        // Copy more body content into the request body behind already written content
        memcpy(request->body+request->body_used, request_mem, tmp);

        // Adjust the variables indicating used space both for req_mem_size and request->body
        request->body_used += tmp;

        size_t remaining = request_mem_size - tmp;
        request_mem_size = max(remaining, BUFFER_SIZE);
        // TODO: Use realloc
        // Hatte nur SEG FAULTS -- jakob
        /*
        char* request_mem_new = (char*) calloc(request_mem_size,sizeof(request[0]));
        MEM_CHECK(request_mem_new)
        memcpy(request, request_mem + tmp, remaining);
        used = used - tmp;

        // Free old buffer and reassign pointers
        free(request_mem);
        request_mem = request_mem_new;
         */

        /* Return the difference between what should have been extracted and what was actually extracted, ideally this
         * value is 0. I know, there is an unsigned long to int narrowing here, if you have a better idea let me know
         */
        if(n-tmp == 0) return BODY_SUCCESS;
        return n-tmp;
    /*} else {
        // Assign more space even though this else branch shouldn't happen, just to be sure though
        request->body = realloc(request->body, request->body_size*2);
        MEM_CHECK(request->body)
        request->body_size += request->body_size;
        return extract_n_from_req_mem(request, n);
    }*/
}

// Recognize packages; true on success false on failure
bool parse_http_header(char* request, http_request* ptr) {
    http_request parsed = {};
    {
        HTTP_METHOD method = method_str_to_enum(strtok(request, " "));
        if (method == -1) return false;
        parsed.method = method;
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
        while (line = strtok(NULL, "\r\n")) {
            if (strlen(line) <= 1)
                break;
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

            if(strcasecmp(kv->key, "Content-Length") == 0){
                parsed.body_size= atoi(kv->value);
            }
        }
        strtok(NULL, "\n");
    }
    *ptr = parsed;
    return true;
}

int respond_to_request(http_request* request, int connection){

    // Get a response struct
    http_response* response = get_http_response(request);
    if (response == NULL) return R_GET_FAULT;

    debug_print_response(response);

    // Transform it to string
    char* response_string = http_response_to_string(response); //TODO: If you want you could implement a R_STRING_FAULT error return

    // Send the response
    if (send(connection, response_string, strlen(response_string), 0) < 0) return R_SEND_FAULT;

    // Cleanup
    free(response_string);
    return SUCCESS;

}

http_response* get_http_response(http_request* request) {
    debug_print_request(request);
    http_response response;
    switch (request->method) {
        case GET: {
            char* resource = NULL;
            http_response r = {.status_code=get_resource(request->url, &resource)};
            r.body = resource;
            response = r;
            break;
        }
        case PUT: {
            if(url_is_save(request->url)) {
                int status_code = set_dynamic(request->url, request->body);
                http_response r = {.status_code=status_code};
                r.body = NULL;
                response = r;
                break;
            }
            else {
                http_response r = {.status_code=403};
                response = r;
                break;
            }
        }
        case DELETE: {
            if(url_is_save(request->url)) {
                int status_code = delete_dynamic(request->url);
                http_response r = {.status_code=status_code};
                r.body = NULL;
                response = r;
                break;
            }
            else {
                http_response r = {.status_code=403};
                response = r;
                break;
            }
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

    http_response* response_ptr = malloc(sizeof(http_response));
    *response_ptr = response;
    return response_ptr;
}

char* http_response_to_string(http_response* response) {
    size_t len;

    // http status codes (i.e. 200, 404) have usually 3 digits
    int size_status_code = 3;

    // TODO: add key-value-pairs to response text

    //char* kv_block = response->kv_block;

    if (response->body) {
        len = strlen(response->version) + size_status_code + strlen(response->phrase) + num_to_digits(strlen(response->body)) + strlen(response->body) + 24 + 1;
        char *text = calloc(len, 1);
        snprintf(text, len, "%s %d %s\r\nContent-Length: %lu\r\n\r\n%s", response->version,
                 response->status_code, response->phrase, strlen(response->body),response->body);
        return text;
    } else {
        len = strlen(response->version) + size_status_code + strlen(response->phrase) + 6 +1;
        char *text = calloc(len, 1);
        snprintf(text, len, "%s %d %s\r\n\r\n", response->version,
                 response->status_code, response->phrase);
        return text;
    }
}

size_t num_to_digits(size_t num) {
    size_t count = 0;
    do {
        num /= 10;
        ++count;
    } while(num != 0);
    return count;
}

int get_resource(char* url, char** dest) {
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
    return get_dynamic(url, dest);
}

int get_static(char* name, char** dest) {
    if (!strcmp(name, "foo"))*dest = strdup("Foo");
    else if (!strcmp(name, "bar"))*dest = strdup("Bar");
    else if (!strcmp(name, "baz"))*dest = strdup("Baz");
    else return 404; // not found
    return 200; // OK
}

int get_dynamic(char* name, char** dest) {
    dynamic_content *curr = dynamicContent;
    if(curr->name == NULL) {
        return 404;
    }
    while(strcmp(curr->name, name) != 0) {
        if(curr->next == NULL) {
            return 404;
        }
        curr = curr->next;
    }
    *dest = curr->content;
    return 200;
}

int set_dynamic(char* name, char* data) {
    if(dynamicContent->name == NULL) {
        dynamicContent->name = calloc(strlen(name), sizeof(char *));
        strncpy(dynamicContent->name, name, strlen(name)+1);
        dynamicContent->content = calloc(strlen(data), sizeof(char *));
        strncpy(dynamicContent->content, data, strlen(data)+1);
    }
    else {
        dynamic_content* curr = dynamicContent;
        while(curr->next != NULL) {
            curr = curr->next;
        }
        dynamic_content *new = calloc(1, sizeof(dynamic_content));
        new->name = calloc(strlen(name), sizeof(char *));
        strncpy(new->name, name, strlen(name)+1);
        new->content = calloc(strlen(data), sizeof(char *));
        strncpy(new->content, data, strlen(data)+1);
        new->next = NULL;
        curr->next = new;
    }
    return 200;
}

int delete_dynamic(char *name) { // TODO: DELETE struct entry
    char path[MAX_URL_LENGTH];
    absolute_path(name, path);
    if (remove(path) == 0) {
        return 200;
    } else {
        return 404;
    }
}

void absolute_path(char* relative_path, char** absolut_path) {
    // TODO: Fix warning: passing argument 1 of ‘snprintf’ from incompatible pointer type [-Wincompatible-pointer-types] (absolute_path)
    snprintf(absolut_path, MAX_URL_LENGTH, "%s"RELATIVE_RESOURCE_PATH"%s", getenv("PWD"), relative_path);
}

bool url_is_save(char* url) {
    if (!strncmp(url, "/dynamic", 8 /* length of "/dynamic"*/))
        url += 8;
    else
        return false;
    bool save_sub_path = true;
    while (*url != '\0') {
        char c = *url;
        url++;
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

HTTP_METHOD method_str_to_enum(char* str) {
    if (str == NULL) return -1;
    for (int i = 0; i < sizeof(method_table) / sizeof(method_table[0]); i++) {
        if (!strcmp(method_table[i], str)) {
            return i;
        }
    }
    return -1;
}

char* status_code_to_phrase(int status_code) {
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
        case 500:
            return "Internal Server Error";
        case 501:
            return "Not Implemented";
        default:
            return "Status Code Not Implemented";
    }
}

bool request_has_body(HTTP_METHOD method) {
    const bool lookup[] = {
            0, 0, 1, 1, 1/* may */, 0, 0, 0, 1
    };
    return lookup[method];
}

bool response_has_body(HTTP_METHOD method) {
    const bool lookup[] = {
            1, 0, 1, 1/* may */, 1/* may */, 1, 1, 0, 1
    };
    return lookup[method];
}

size_t max(size_t a, size_t b) {
    return a > b ? a : b;
}

size_t min(size_t a, size_t b){
    return a < b ? a : b;
}

#ifdef DEBUG

void debug_print_request_func(http_request* request) {
    printf("method: %d %s\n", request->method, method_table[request->method]);
    printf("url: %s\n", request->url);
    printf("version: %s\n", request->version);

    for (key_value_pair* current = request->kv_block; current != NULL; current = current->next) {
        printf("key: %s, value: %s\n", current->key, current->value);
    }
    if (request->body != NULL)
        printf("body: %s\n\n", request->body);
    else
        printf("no body\n\n");
}

void debug_print_response_func(http_response* response) {
    printf("version: %s\n", response->version);
    printf("status: %d %s\n", response->status_code, response->phrase);

    for (key_value_pair* current = response->kv_block; current != NULL; current = current->next) {
        printf("key: %s, value: %s\n", current->key, current->value);
    }
    if (response->body != NULL)
        printf("body: %s\n\n", response->body);
    else
        printf("no body\n\n");
}

#endif