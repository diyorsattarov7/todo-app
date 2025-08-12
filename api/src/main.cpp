// src/main.cpp
// clang-format off

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/json.hpp>
#include <boost/mysql.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/tcp.hpp>

namespace net   = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
using     tcp   = net::ip::tcp;
namespace json  = boost::json;
namespace mysql = boost::mysql;

int main()
{
    return 0;
}
