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

#include <iostream>

namespace net   = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
using     tcp   = net::ip::tcp;
namespace json  = boost::json;
namespace mysql = boost::mysql;

static std::string env(const char *k, const char *d) { if (const char *v = std::getenv(k)) return v; return d; }

static bool fv_to_bool(mysql::field_view f)
{
    using kind = mysql::field_kind;
    switch (f.kind())
    {
        case kind::int64:  return f.get_int64() != 0;
        case kind::uint64: return f.get_uint64() != 0;
        case kind::string: return (f.get_string() == "1" || f.get_string() == "true");
        default:           return false;
    }
}

static long long fv_to_i64(mysql::field_view f)
{
    using kind = mysql::field_kind;
    switch (f.kind())
    {
        case kind::int64:  return f.get_int64();
        case kind::uint64: return static_cast<long long>(f.get_uint64());
        case kind::string: return std::stoll(std::string(f.get_string()));
        default:           throw std::runtime_error("numeric field has incompatible type");
    }
}

static std::string fv_to_string(mysql::field_view f)
{
    using kind = mysql::field_kind;
    switch (f.kind())
    {
        case kind::string:  return std::string(f.get_string());
        case kind::int64:   return std::to_string(f.get_int64());
        case kind::uint64:  return std::to_string(f.get_uint64());
        case kind::float_:  return std::to_string(f.get_float());
        case kind::double_: return std::to_string(f.get_double());
        case kind::null:    return std::string{};
        default:            return std::string{};
    }
}

struct app_ctx
{
    net::io_context         &ioc;
    mysql::tcp_connection   db;
    mysql::handshake_params params;
    std::string             db_host, db_port, cors_origin{"*"};
    std::mutex              db_mtx;

    mysql::statement stmt_list;
    mysql::statement stmt_insert;
    mysql::statement stmt_update;
    mysql::statement stmt_delete;
    mysql::statement stmt_lastid;

    app_ctx(net::io_context &ioc_,
            std::string host, std::string port,
            std::string user, std::string pass, std::string name,
            std::string cors)
        : ioc(ioc_), db(ioc_), params(std::move(user), std::move(pass), std::move(name)),
        db_host(std::move(host)), db_port(std::move(port)), cors_origin(std::move(cors)) {}

    void prepare_all()
    {
        stmt_list   = db.prepare_statement(
                "SELECT id, title, done, DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') AS created_at "
                "FROM todos ORDER BY id"
                );
        stmt_insert = db.prepare_statement("INSERT INTO todos(title,done) VALUES(?, false)");
        stmt_update = db.prepare_statement("UPDATE todos SET title=?, done=? WHERE id=?");
        stmt_delete = db.prepare_statement("DELETE FROM todos WHERE id=?");
        stmt_lastid = db.prepare_statement("SELECT LAST_INSERT_ID()");
    }

    void ensure_prepared() { if (!stmt_list.valid() || !stmt_insert.valid() || !stmt_update.valid() || !stmt_delete.valid() || !stmt_lastid.valid()) prepare_all(); }

    void ensure_db()
    {
        std::lock_guard<std::mutex> lock(db_mtx);
        try { db.ping(); return; } catch (...) {}

        tcp::resolver resolver(ioc);
        auto results = resolver.resolve(db_host, db_port);
        for (const auto &r : results)
        {
            try { db.connect(r.endpoint(), params); db.ping(); prepare_all(); return; }
            catch (...) {}
        }
        throw std::runtime_error("DB reconnect failed");
    }
};

static void add_cors(http::response<http::string_body>& res, const std::string& origin)
{
    res.set(http::field::access_control_allow_origin, origin);
    res.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, OPTIONS");
    res.set(http::field::access_control_allow_headers, "Content-Type, Accept");
}
static http::response<http::string_body> make_json(unsigned ver, bool ka, int code, const json::value& body, const std::string& origin)
{
    http::response<http::string_body> res{http::status(code), ver};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");
    res.keep_alive(ka);
    res.body() = json::serialize(body);
    add_cors(res, origin);
    res.prepare_payload();
    return res;
}
static http::response<http::string_body> make_text(unsigned ver, bool ka, int code, const std::string& body, const std::string& origin)
{
    http::response<http::string_body> res{http::status(code), ver};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/plain; charset=utf-8");
    res.keep_alive(ka);
    res.body() = body;
    add_cors(res, origin);
    res.prepare_payload();
    return res;
}

template <class Body, class Alloc>
http::message_generator handle_request(app_ctx& ctx,
                                       http::request<Body, http::basic_fields<Alloc>>&& req)
{
    const unsigned ver = req.version();
    const bool     ka  = req.keep_alive();
    const auto&    origin = ctx.cors_origin;
    const beast::string_view target = req.target();

    if (req.method() == http::verb::options)
    {
        http::response<http::string_body> res{http::status::ok, ver};
        res.keep_alive(ka);
        add_cors(res, origin);
        res.prepare_payload();
        return res;
    }

    if (req.method() == http::verb::get && target == "/healthz")
        return make_json(ver, ka, 200, json::object{{"status","ok"}}, origin);

    return make_text(ver, ka, 404, "Not found", origin);
}

static void fail(beast::error_code ec, const char* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

static void do_session(tcp::socket raw_socket, std::shared_ptr<app_ctx> ctx)
{
    beast::tcp_stream stream(std::move(raw_socket));
    beast::flat_buffer buffer;
    beast::error_code ec;

    for (;;)
    {
        stream.expires_after(std::chrono::seconds(30));

        http::request<http::string_body> req;
        http::read(stream, buffer, req, ec);
        if (ec == http::error::end_of_stream) break;
        if (ec) { fail(ec, "read"); break; }

        auto msg = handle_request(*ctx, std::move(req));
        const bool keep_alive = msg.keep_alive();

        beast::write(stream, std::move(msg), ec);
        if (ec) { fail(ec, "write"); break; }

        if (!keep_alive) break;
    }

    stream.socket().shutdown(tcp::socket::shutdown_send, ec);
}


int main()
{
    return 0;
}
