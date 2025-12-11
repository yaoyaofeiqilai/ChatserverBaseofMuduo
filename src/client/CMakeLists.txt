#编译目录下的源文件
aux_source_directory(../public PUBLIC_LIST)
find_package(OpenSSL REQUIRED)
if(OpenSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})  # 添加头文件路径
else()
    message(FATAL_ERROR "未找到 OpenSSL")  # 如果没找到就报错
endif()
#生成可执行文件
add_executable(ChatClient ${PROJECT_SOURCE_DIR}/src/client/client.cpp ${PROJECT_SOURCE_DIR}/src/client/main.cpp ${PUBLIC_LIST})
add_executable(StressTest ${PROJECT_SOURCE_DIR}/src/client/stresstest.cpp ${PROJECT_SOURCE_DIR}/src/client/client.cpp ${PUBLIC_LIST})
#链接库
target_link_libraries(ChatClient pthread ${OPENSSL_LIBRARIES})
target_link_libraries(StressTest pthread ${OPENSSL_LIBRARIES})