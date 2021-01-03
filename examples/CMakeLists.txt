add_executable (example-server ${CMAKE_CURRENT_SOURCE_DIR}/server.c)
target_link_libraries (example-server symbiomon-server)

add_executable (example-admin ${CMAKE_CURRENT_SOURCE_DIR}/admin.c)
target_link_libraries (example-admin symbiomon-admin)

add_executable (example-client ${CMAKE_CURRENT_SOURCE_DIR}/client.c)
target_link_libraries (example-client symbiomon-client)
