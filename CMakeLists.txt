cmake_minimum_required (VERSION 3.0)
project (RN-Praxis1)
set (CMAKE_C_STANDARD 11)

add_executable (webserver webserver.c webserver.h)

# Do not change beyond this point :)
# Testing
enable_testing ()
add_test(Execute ${PROJECT_SOURCE_DIR}/test/execute.py ${PROJECT_BINARY_DIR}/solution)
add_test(Listen ${PROJECT_SOURCE_DIR}/test/listen.py ${PROJECT_BINARY_DIR}/solution)
add_test(Reply ${PROJECT_SOURCE_DIR}/test/reply.py ${PROJECT_BINARY_DIR}/solution)
add_test(Packets ${PROJECT_SOURCE_DIR}/test/packets.py ${PROJECT_BINARY_DIR}/solution)
add_test(HTTPReply ${PROJECT_SOURCE_DIR}/test/httpreply.py ${PROJECT_BINARY_DIR}/solution)
add_test(HTTPReplies ${PROJECT_SOURCE_DIR}/test/httpreplies.py ${PROJECT_BINARY_DIR}/solution)
add_test(StaticContent ${PROJECT_SOURCE_DIR}/test/static_content.py ${PROJECT_BINARY_DIR}/solution)
add_test(DynamicContent ${PROJECT_SOURCE_DIR}/test/dynamic_content.py ${PROJECT_BINARY_DIR}/solution)

# Packaging
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
  ${CMAKE_BINARY_DIR}
  /\\..*$
)
set(CPACK_VERBATIM_VARIABLES YES)
include(CPack)
