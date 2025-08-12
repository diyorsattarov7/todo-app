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

int main()
{
    return 0;
}
