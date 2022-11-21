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

typedef struct dynamic_content dynamic_content;
struct dynamic_content {
    char* name;
    char* content;
    dynamic_content* next;
};

typedef enum {
    GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH
} HTTP_METHOD;
const char* method_table[] = {"GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE", "PATCH"};

typedef enum {
    SUCCESS = 0, NO_CRLF = -1, PARSE_HEADER_FAULT = -2, CONTENT_LENGTH_KV_FAULT = -3, TIMEOUT = -4,
    R_GET_FAULT = -5, R_STRING_FAULT = -6, R_SEND_FAULT = -7, BODY_SUCCESS = -8
} CONNECTION_STATE;

typedef struct {
    HTTP_METHOD method;
    char* url;
    char* version;
    key_value_pair* kv_block;
    size_t body_size;
    size_t body_used;
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
int handle_http_connection(int connection);
int receive_bytes(int connection);
/**
 * Takes a char* memory segment 'request' and extracts the beginning string until the first
 * "CRLF" into a new buffer. Then replaces the global 'request_mem' with a new memory segment
 * containing the remaining string and frees the old 'request_mem' memory.
 * @param request
 * @return A buffer containing the first string including "CRLF" or NULL if there was no "CRLF".
 */
char* extract_until_empty_line(char* request);
/**
 * Extracts n or used bytes of data (whichever is less) from request_mem to request->body and moves the remaining data
 * in request_mem to the front of the allocated memory.
 * @param request
 * @param n
 * @return The difference between the requested extraction n and the actual amount of extracted bytes, ideally this is 0.
 */
int extract_n_from_req_mem(http_request* request, size_t n);

// handle http strings and structs
int handle_http_request(int connection);
bool parse_http_header(char* request, http_request* ptr);
http_response* get_http_response(http_request* request);
char* http_response_to_string(http_response* response);
int respond_to_request(http_request* request, int connection);

// resources
int get_resource(char* url, char** dest);
int get_static(char* name, char** dest);
int get_dynamic(char* name, char** dest);
int set_dynamic(char* name, char* dest);
int delete_dynamic(char* name);
void absolute_path(char* relative_path, char** dest);
bool url_is_save(char* url);
bool is_save_char(char c);
// small helper functions
HTTP_METHOD method_str_to_enum(char* strtok);
char* status_code_to_phrase(int status_code);
bool request_has_body(HTTP_METHOD method);
bool response_has_body(HTTP_METHOD method);
size_t max(size_t a,size_t b);
size_t min(size_t a,size_t b);
size_t num_to_digits(size_t num);

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
