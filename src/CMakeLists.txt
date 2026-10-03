add_library(ddslite STATIC
    transport/udp_multicast_transport.cpp
    discovery/participant.cpp
    discovery/discovery_service.cpp
)

target_include_directories(ddslite PUBLIC ${PROJECT_SOURCE_DIR}/include)

if(WIN32)
    target_link_libraries(ddslite PUBLIC ws2_32)
endif()
