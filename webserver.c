#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_PORT 8080
#define MAX_MESSAGE_SIZE 256
#define MAX_PENDING_CONNECTIONS 5

int main(void)
{
    // creates an endpoint for communication and returns a file descriptor that refers to that endpoint
    // SOCK_STREAM = communicate over TCP
    int socket_file_descriptor = socket(AF_INET, SOCK_STREAM, 0);

    // Create a socket
    struct sockaddr_in server_sockaddr_in;

    // define the socket type, AF_INET is for IPv4
    server_sockaddr_in.sin_family = AF_INET;

    // htons converts the port number to network byte order
    server_sockaddr_in.sin_port = htons(DEFAULT_PORT);

    // htonl converts the IP address to network byte order
    server_sockaddr_in.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the socket to the port
    bind(socket_file_descriptor, (struct sockaddr *)&server_sockaddr_in, sizeof(server_sockaddr_in));

    // Listen for incoming connections, MAX_PENDING_CONNECTIONS is the maximum number of connections that can be queued
    listen(socket_file_descriptor, MAX_PENDING_CONNECTIONS);

    struct sockaddr_in client_sockaddr_in;
    socklen_t len = sizeof(client_sockaddr_in);

    // Accepts an incoming connection on a socket
    int client_fd = accept(socket_file_descriptor, (struct sockaddr *)&client_sockaddr_in, &len);

    // store the message in a char array
    char buffer[MAX_MESSAGE_SIZE] = {};

    // Read the message from the client
    read(client_fd, buffer, sizeof(buffer));

    // Print the message to the console
    printf("%s", buffer);

    // Send "Response" back to the client
    write(client_fd, "Response", 9);

    // Close the connection
    close(socket_file_descriptor);

    return 0;
}