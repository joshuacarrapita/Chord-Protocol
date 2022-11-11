// Includes (necessary)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

// Includes (maybe necessary)
#include <string.h>
#include <sys/un.h>

// Constants
#define DEFAULT_PORT 80
#define MAX_BACKLOG 1
#define ASSERT_SUCCESS(v) if (v<0) exit(1);

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
    int l = listen(s,MAX_BACKLOG);
    ASSERT_SUCCESS(l)

    /* Extracts the first connection request in queue and creates a new connected socket,
     * returns a file descriptor referring to that socket. The newly created socket is not
     * in the listening state, the original socket is unaffected by this call.
     */
    int connection = accept(s, (struct sockaddr*)NULL, NULL);
    ASSERT_SUCCESS(connection)

    // read & reply
    char buffer[256];
    char reply[]="Reply";
    {

            ASSERT_SUCCESS(send(connection, reply, sizeof(reply),0))

    }
    return EXIT_SUCCESS;
}
