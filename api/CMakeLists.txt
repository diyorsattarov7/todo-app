cmake_minimum_required(VERSION 3.16)
project(api CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Boost_USE_STATIC_LIBS ON)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

find_package(Boost 1.88 REQUIRED COMPONENTS json charconv)

add_executable(api src/main.cpp)
target_include_directories(api PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(api PRIVATE
  Threads::Threads
  OpenSSL::SSL
  OpenSSL::Crypto
  Boost::json
  Boost::charconv
)
