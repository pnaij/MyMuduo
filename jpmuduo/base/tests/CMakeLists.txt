# jpmuduo base 模块单元测试
# 每个测试自包含 main()，链接 libjpmuduo

add_executable(date_unittest Date_unittest.cpp)
target_link_libraries(date_unittest jpmuduo)

add_executable(timestamp_unittest TimeStamp_unittest.cpp)
target_link_libraries(timestamp_unittest jpmuduo)

add_executable(logstream_test LogStream_test.cpp)
target_link_libraries(logstream_test jpmuduo)

add_executable(countdownlatch_test CountDownLatch_test.cpp)
target_link_libraries(countdownlatch_test jpmuduo)

add_executable(singleton_test Singleton_test.cpp)
target_link_libraries(singleton_test jpmuduo)

add_executable(blockingqueue_test BlockingQueue_test.cpp)
target_link_libraries(blockingqueue_test jpmuduo)

add_executable(boundedblockingqueue_test BoundedBlockingQueue_test.cpp)
target_link_libraries(boundedblockingqueue_test jpmuduo)

add_executable(threadpool_test ThreadPool_test.cpp)
target_link_libraries(threadpool_test jpmuduo)

add_executable(logfile_test LogFile_test.cpp)
target_link_libraries(logfile_test jpmuduo)

add_executable(asynclogging_test AsyncLogging_test.cpp)
target_link_libraries(asynclogging_test jpmuduo)

add_executable(processinfo_test ProcessInfo_test.cpp)
target_link_libraries(processinfo_test jpmuduo)
