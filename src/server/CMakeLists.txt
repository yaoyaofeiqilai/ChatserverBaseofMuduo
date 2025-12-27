#包含目录下所有·cpp文件
aux_source_directory(. SRC_LIST)
aux_source_directory(./db DB_LIST)
aux_source_directory(./dataoperate DATAOPERATE_LIST)
aux_source_directory(./redis REDIS_LIST)
aux_source_directory(../public PUBLIC_LIST)
aux_source_directory(./test TEST_LIST)
find_package(OpenSSL REQUIRED)
if(OpenSSL_FOUND)
    message(STATUS "找到 OpenSSL 版本: ${OpenSSL_VERSION}")
    include_directories(${OPENSSL_INCLUDE_DIR})  # 添加头文件路径
else()
    message(FATAL_ERROR "未找到 OpenSSL")  # 如果没找到就报错
endif()

#指定生成可执行文件
add_executable(ChatServer ${SRC_LIST} ${DB_LIST} ${DATAOPERATE_LIST} ${REDIS_LIST} ${PUBLIC_LIST})
add_executable(test ${TEST_LIST} ${DB_LIST} ${DATAOPERATE_LIST} ${REDIS_LIST} ${PUBLIC_LIST})

#添加链接库
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient hiredis pthread ${OPENSSL_LIBRARIES})
target_link_libraries(test muduo_net muduo_base mysqlclient hiredis pthread ${OPENSSL_LIBRARIES})