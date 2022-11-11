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

// Constants
#define DEFAULT_PORT 80
#define MAX_BACKLOG 1
#define ASSERT_SUCCESS(v) if (v<0) exit(1);
#define BUFFER_SIZE 256
#define MEM_CHECK(p) if(p==NULL){\
fprintf(stderr, "Allocation failed near line %d.\n", __LINE__);\
exit(1);\
}

// Functions
int handle_http_connection(int connection);

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

// Recognize packages
int handle_http_connection(int connection) {


    // Create a memory segment holding a full request that we can save the content
    // first saved in buffer into and then expand when needed
    char* request = calloc(BUFFER_SIZE, sizeof(char));
    MEM_CHECK(request)

    // Variables to indicate length of request memory and actually used space within request memory
    size_t size = BUFFER_SIZE;
    size_t used = 0;

    // Read from the bytestream into a buffer
    while (1) {

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
        if (0 == strncmp(request + used - 4, "\r\n\r\n", 4))
            break;
    }

    // Debug
    if (used < size)
        request[used] = '\0';
    else
        request[size - 1] = '\0';
    printf("(%zu/%zu):\n%s\n", used, size, request);
}