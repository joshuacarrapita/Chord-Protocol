//
// Created by anton on 13.11.22.
//

#ifndef RN_PRAXIS1_WEBSERVER_H
#define RN_PRAXIS1_WEBSERVER_H
// Includes (necessary)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

// Includes (maybe necessary)
#include <string.h>
#include <sys/un.h>
#include <stdbool.h>

#define ASSERT_SUCCESS(v) if (v<0) {fprintf(stderr, "problem near line %d", __LINE__);exit(1);}
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
const char* verb_table[] = {"GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE", "PATCH"};

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
    char* body;
} http_response;

// handle connections
bool handle_http_connection(int connection);
char* read_until_empty_line(int connection);
/**
 * Takes a char* memory segment 'request' and extracts the beginning string until the first
 * "CRLF" into a new buffer. Then replaces the global 'request_mem' with a new memory segment
 * containing the remaining string and frees the old 'request_mem' memory.
 * @param request
 * @return A buffer containing the first string including "CRLF" or NULL if there was no "CRLF".
 */
char* extract_until_empty_line(char* request);

// handle http strings and structs
bool parse_http_header(char* request, http_request* ptr);
http_response* handle_http_request(http_request* request);
char* http_response_to_string(http_response* response);

// resources
int get_resource(char* url, char** dest);
int get_static(char* name, char** dest);
int get_dynamic(char* name, char** dest);
void absolute_path(char* relative_path, char** dest);
bool url_is_save(char* url);
bool is_save_char(char c);

// small helper functions
HTTP_VERB verb_str_to_enum(char* strtok);
char* status_code_to_phrase(int status_code);
bool request_has_body(HTTP_VERB verb);
bool response_has_body(HTTP_VERB verb);
size_t max(size_t a,size_t b);

// debug utilities
#ifdef DEBUG
#define debug_print_request(request) debug_print_request_func(request);
#define debug_print_response(response) debug_print_response_func(response);
void debug_print_request_func(http_request* request);
void debug_print_response_func(http_response* response);
#else
#define debug_print_request(request)
#define debug_print_response(response)
#endif
#endif //RN_PRAXIS1_WEBSERVER_H
