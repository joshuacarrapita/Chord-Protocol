cmake_minimum_required (VERSION 3.0)
project (RN-Praxis3)
set (CMAKE_C_STANDARD 11)

# Client
add_executable(client src/client.c src/packet.c src/util.c)
target_include_directories(client PRIVATE include)
set_target_properties(client PROPERTIES OUTPUT_NAME "client")
target_compile_options (client PRIVATE -Wall -Wextra -Wpedantic)

# Peer
add_executable(peer src/peer.c include/peer.h src/server.c src/packet.c src/util.c src/hash_table.c src/neighbour.c include/neighbour.h include/requests.h src/requests.c include/fingertable.h src/fingertable.c)
target_include_directories(peer PRIVATE include)
set_target_properties(peer PROPERTIES OUTPUT_NAME "peer")
target_compile_options (peer PRIVATE -Wall -Wextra -Wpedantic -pthread)
find_package(Threads REQUIRED)
target_link_libraries(peer PRIVATE m Threads::Threads)

# Packaging
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
  ${CMAKE_BINARY_DIR}
  /\\..*$
)
set(CPACK_VERBATIM_VARIABLES YES)
include(CPack)
