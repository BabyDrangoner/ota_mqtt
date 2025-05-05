cmake_minimum_required(VERSION 2.8)
project(sherry)

include (./cmake/utils.cmake)

# 设置C++17标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++17 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined")

# 添加头文件路径，包含paho.mqtt.cpp的头文件路径
include_directories(
   .
    ${PROJECT_SOURCE_DIR}/include
    /build/yaml-cpp-0.8.0/include
)

# 添加库文件路径，包含paho.mqtt.cpp库的路径
link_directories(
    ${PROJECT_SOURCE_DIR}/lib64
    /apps/sherry/lib
)

# 查找依赖库
find_library(YAMLCPP NAMES yaml-cpp)
find_library(SSL_LIB NAMES ssl)
find_library(CRYPTO_LIB NAMES crypto)
find_library(PAHO_MQTTPP3 NAMES paho-mqttpp3 PATHS ${PROJECT_SOURCE_DIR}/lib64)
find_library(PAHO_MQTT3C NAMES paho-mqtt3c PATHS ${PROJECT_SOURCE_DIR}/lib64)
find_library(PAHO_MQTT3AS NAMES paho-mqtt3as PATHS ${PROJECT_SOURCE_DIR}/lib64)

set(LIB_SRC
    sherry/address.cc
    sherry/bytearray.cc
    sherry/log.cc
    sherry/fd_manager.cc
    sherry/fiber.cc
    sherry/util.cc
    sherry/hook.cc
    sherry/scheduler.cc
    sherry/socket.cc
    sherry/timer.cc
    sherry/config.cc
    sherry/thread.cc
    sherry/iomanager.cc
    sherry/ota_client_callback.cc
    sherry/mqtt_client.cc
    sherry/ota_notifier.cc
    sherry/ota_query_responder.cc
    sherry/ota_message.cc
    sherry/ota_subscribe_download.cc
    sherry/myfiber.cc
    sherry/http/http_server.cc
    sherry/http/http_send_buffer.cc
    sherry/ota_http_response_builder.cc
    sherry/ota_manager.cc
    sherry/ota_http_command_dispatcher.cc
    
    
)

add_library(sherry SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(sherry)  #__FILE__

# 定义链接库列表
set(LIBS
    sherry
    dl
    pthread
    ${YAMLCPP}
    ${PAHO_MQTTPP3}  # 使用正确的库名变量
    ${PAHO_MQTT3C}
    ${PAHO_MQTT3AS}
    ${SSL_LIB}
    ${CRYPTO_LIB}
)

message("***", ${LIBS})

# 定义可执行文件目标
add_executable(test tests/test.cc)
add_dependencies(test sherry)
force_redefine_file_macro_for_sources(test)  #__FILE__
target_link_libraries(test ${LIBS})

add_executable(test_config tests/test_config.cc)
add_dependencies(test_config sherry)
force_redefine_file_macro_for_sources(test_config)  #__FILE__
target_link_libraries(test_config ${LIBS})

add_executable(test_thread tests/test_thread.cc)
add_dependencies(test_thread sherry)
force_redefine_file_macro_for_sources(test_thread)  #__FILE__
target_link_libraries(test_thread ${LIBS})

add_executable(test_util tests/test_util.cc)
add_dependencies(test_util sherry)
force_redefine_file_macro_for_sources(test_util)  #__FILE__
target_link_libraries(test_util ${LIBS})

add_executable(test_fiber tests/test_fiber.cc)
add_dependencies(test_fiber sherry)
force_redefine_file_macro_for_sources(test_fiber)  #__FILE__
target_link_libraries(test_fiber ${LIBS})

add_executable(test_scheduler tests/test_scheduler.cc)
add_dependencies(test_scheduler sherry)
force_redefine_file_macro_for_sources(test_scheduler)  #__FILE__
target_link_libraries(test_scheduler ${LIBS})

add_executable(test_iomanager tests/test_iomanager.cc)
add_dependencies(test_iomanager sherry)
force_redefine_file_macro_for_sources(test_iomanager)  #__FILE__
target_link_libraries(test_iomanager ${LIBS})

add_executable(test_hook tests/test_hook.cc)
add_dependencies(test_hook sherry)
force_redefine_file_macro_for_sources(test_hook)  #__FILE__
target_link_libraries(test_hook ${LIBS})

add_executable(test_address tests/test_address.cc)
add_dependencies(test_address sherry)
force_redefine_file_macro_for_sources(test_address)  #__FILE__
target_link_libraries(test_address ${LIBS})

add_executable(test_socket tests/test_socket.cc)
add_dependencies(test_socket sherry)
force_redefine_file_macro_for_sources(test_socket)  #__FILE__
target_link_libraries(test_socket ${LIBS})

add_executable(test_bytearray tests/test_bytearray.cc)
add_dependencies(test_bytearray sherry)
force_redefine_file_macro_for_sources(test_bytearray)  #__FILE__
target_link_libraries(test_bytearray ${LIBS})

add_executable(test_server tests/test_server.cc)
add_dependencies(test_server sherry)
force_redefine_file_macro_for_sources(test_server)  #__FILE__
target_link_libraries(test_server ${LIBS})

#add_executable(test_mqtt_client tests/test_mqtt_client.cc)
#add_dependencies(test_mqtt_client sherry)
#force_redefine_file_macro_for_sources(test_mqtt_client)  #__FILE__
#target_link_libraries(test_mqtt_client ${LIBS})

#add_executable(test_mqtt_subscribe tests/test_mqtt_subscribe.cc)
#add_dependencies(test_mqtt_subscribe sherry)
#force_redefine_file_macro_for_sources(test_mqtt_subscribe)  #__FILE__
#target_link_libraries(test_mqtt_subscribe ${LIBS})

#add_executable(test_mqtt_publish tests/test_mqtt_publish.cc)
#add_dependencies(test_mqtt_publish sherry)
#force_redefine_file_macro_for_sources(test_mqtt_publish)  #__FILE__
#target_link_libraries(test_mqtt_publish ${LIBS})

add_executable(test_ota_notifier tests/test_ota_notifier.cc)
add_dependencies(test_ota_notifier sherry)
force_redefine_file_macro_for_sources(test_ota_notifier)  #__FILE__
target_link_libraries(test_ota_notifier ${LIBS})

add_executable(test_ota_query tests/test_ota_query.cc)
add_dependencies(test_ota_query sherry)
force_redefine_file_macro_for_sources(test_ota_query)  #__FILE__
target_link_libraries(test_ota_query ${LIBS})

add_executable(test_myfiber tests/test_myfiber.cc)
add_dependencies(test_myfiber sherry)
force_redefine_file_macro_for_sources(test_myfiber)  #__FILE__
target_link_libraries(test_myfiber ${LIBS})

add_executable(test_ota_manager tests/test_ota_manager.cc)
add_dependencies(test_ota_manager sherry)
force_redefine_file_macro_for_sources(test_ota_manager)  #__FILE__
target_link_libraries(test_ota_manager ${LIBS})

#add_executable(test_ota_download tests/test_ota_download.cc)
#add_dependencies(test_ota_download sherry)
#force_redefine_file_macro_for_sources(test_ota_download)  #__FILE__
#target_link_libraries(test_ota_download ${LIBS})

add_executable(test_http_server tests/test_http_server.cc)
add_dependencies(test_http_server sherry)
force_redefine_file_macro_for_sources(test_http_server)  #__FILE__
target_link_libraries(test_http_server ${LIBS})

add_executable(test_http_send_buffer tests/test_http_send_buffer.cc)
add_dependencies(test_http_send_buffer sherry)
force_redefine_file_macro_for_sources(test_http_send_buffer)  #__FILE__
target_link_libraries(test_http_send_buffer ${LIBS})

add_executable(test_http_server_client tests/test_http_server_client.cc)
add_dependencies(test_http_server_client sherry)
force_redefine_file_macro_for_sources(test_http_server_client)  #__FILE__
target_link_libraries(test_http_server_client ${LIBS})


SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)