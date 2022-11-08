#include <stdlib.h>

#define DEFAULT_PORT 80
int port = DEFAULT_PORT;
int main(int argc, char** argv) {
    // parse commandline arguments
    if (argc > 1){
        port = atoi(argv[1]);
    }
    return EXIT_SUCCESS;
}
