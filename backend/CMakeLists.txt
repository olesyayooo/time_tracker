cmake_minimum_required(VERSION 3.24)
project(time_tracker_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Проверяем libpq до сборки Drogon, чтобы поддержка PostgreSQL не пропала молча.
find_package(PostgreSQL REQUIRED)
find_package(OpenSSL REQUIRED)
find_path(SODIUM_INCLUDE_DIR sodium.h REQUIRED)
find_library(SODIUM_LIBRARY sodium REQUIRED)
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_CTL OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_ORM ON CACHE BOOL "" FORCE)
set(BUILD_POSTGRESQL ON CACHE BOOL "" FORCE)
set(BUILD_MYSQL OFF CACHE BOOL "" FORCE)
set(BUILD_SQLITE OFF CACHE BOOL "" FORCE)
set(BUILD_REDIS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(drogon
    GIT_REPOSITORY https://github.com/drogonframework/drogon.git
    GIT_TAG v1.9.11
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(drogon)

FetchContent_Declare(jwt_cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG v0.7.1
    GIT_SHALLOW TRUE
)
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(jwt_cpp)

add_executable(time_tracker_backend
    main.cpp config/Config.cpp
    controllers/TimerController.cpp controllers/ReportController.cpp
    services/TimerService.cpp services/ReportService.cpp
    repositories/TimeEntryRepository.cpp repositories/ReportRepository.cpp
    controllers/AuthController.cpp controllers/ProjectController.cpp controllers/TaskController.cpp
    services/AuthService.cpp services/ProjectService.cpp services/TaskService.cpp
    repositories/UserRepository.cpp repositories/ProjectRepository.cpp repositories/TaskRepository.cpp
)
target_include_directories(time_tracker_backend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SODIUM_INCLUDE_DIR})
target_link_libraries(time_tracker_backend PRIVATE drogon jwt-cpp::jwt-cpp ${SODIUM_LIBRARY} OpenSSL::Crypto)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(time_tracker_backend PRIVATE -Wall -Wextra -Wpedantic)
endif()
