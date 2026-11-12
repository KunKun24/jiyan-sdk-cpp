/* --------------------------------- 关于 -------------------------------------
    
                    JiYan网络服务卡密验证对接

----------------------------------------------------------------------------- */

#ifndef JIYAN_SDK_HTTP_CPP
#define JIYAN_SDK_HTTP_CPP

#include <string>
#include <chrono>
#include <algorithm>
#include <limits>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <stdexcept>
#include <memory>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

namespace JiYanNet {
namespace HttpDetail {
constexpr std::size_t MaxBytes = 8u * 1024u * 1024u;
constexpr std::size_t MaxHeaders = 32u * 1024u;
using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;

struct Failure : std::runtime_error {
    explicit Failure(const std::string& message) : std::runtime_error(message) {}
};
inline void fail(const std::string& message) { throw Failure(message); }
inline std::string lower(std::string text) {
    for (char& c : text) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return text;
}
inline std::string trim(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}
inline bool token(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || std::string("!#$%&'*+-.^_`|~").find(c) != std::string::npos;
}
inline std::size_t number(const std::string& text, unsigned base, std::size_t limit) {
    if (text.empty()) fail("HTTP 数字字段为空");
    std::size_t result = 0;
    for (unsigned char c : text) {
        unsigned digit = c >= '0' && c <= '9' ? c - '0' :
            c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 255;
        if (digit >= base || digit > limit || result > (limit - digit) / base)
            fail("HTTP 长度或端口字段无效、溢出或超出限制");
        result = result * base + digit;
    }
    return result;
}
struct Url { std::string host, port, authority, target; };
inline Url parse_url(std::string url) {
    if (url.empty() || url.size() > MaxHeaders) fail("HTTP 地址为空或过长");
    for (unsigned char c : url)
        if (c <= 0x20 || c >= 0x7f || c == '\\' || c == '#') fail("HTTP 地址含空白、片段或非法字符");
    if (lower(url.substr(0, 8)) == "https://") fail("纯 Socket HTTP 不支持 HTTPS，禁止降级；请使用 http:// 地址");
    if (lower(url.substr(0, 7)) == "http://") url.erase(0, 7);
    else if (url.find("://") != std::string::npos) fail("HTTP 地址协议不支持");
    const std::size_t split = url.find_first_of("/?");
    Url out;
    out.authority = url.substr(0, split);
    out.target = split == std::string::npos ? "/" : url.substr(split);
    if (out.target[0] == '?') out.target.insert(0, "/");
    if (out.authority.empty() || out.authority.find('@') != std::string::npos) fail("HTTP 主机为空或含用户信息");
    out.port = "80";
    if (out.authority[0] == '[') {
        const std::size_t end = out.authority.find(']');
        if (end == std::string::npos) fail("HTTP IPv6 地址缺少右括号");
        out.host = out.authority.substr(1, end - 1);
        in6_addr address{};
        if (inet_pton(AF_INET6, out.host.c_str(), &address) != 1) fail("HTTP IPv6 地址无效");
        if (end + 1 < out.authority.size()) {
            if (out.authority[end + 1] != ':') fail("HTTP IPv6 端口格式无效");
            out.port = out.authority.substr(end + 2);
        }
    } else {
        const std::size_t colon = out.authority.find(':');
        out.host = out.authority.substr(0, colon);
        if (colon != std::string::npos) out.port = out.authority.substr(colon + 1);
        if (out.host.empty() || out.host.size() > 253) fail("HTTP 主机名为空或过长");
        for (unsigned char c : out.host)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '.')) fail("HTTP 主机名含非法字符，IPv6 需使用方括号");
    }
    if (number(out.port, 10, 65535) == 0) fail("HTTP 端口不能为零");
    return out;
}
class Socket {
    int fd_;
public:
    explicit Socket(int fd = -1) : fd_(fd) {}
    ~Socket() { if (fd_ >= 0) ::close(fd_); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    int get() const { return fd_; }
    void reset(int fd) { if (fd_ >= 0) ::close(fd_); fd_ = fd; }
};
inline int remaining(Deadline deadline) {
    const auto now = Clock::now();
    if (now >= deadline) fail("HTTP 网络操作超时");
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
    return static_cast<int>(std::min<std::int64_t>(ms + 1, std::numeric_limits<int>::max()));
}
inline void ready(int fd, short events, Deadline deadline) {
    for (;;) {
        pollfd item{fd, events, 0};
        const int result = ::poll(&item, 1, remaining(deadline));
        if (result > 0) {
            if (item.revents & POLLNVAL) fail("HTTP Socket 已失效");
            return; // ERR/HUP 交给 send/recv/getsockopt 读取实际错误。
        }
        if (result == 0) fail("HTTP 网络操作超时");
        if (errno != EINTR) fail(std::string("HTTP 等待 Socket 失败：") + std::strerror(errno));
    }
}
inline void connect_socket(Socket& socket, const Url& url) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_NUMERICSERV;
    addrinfo* raw = nullptr;
    const int resolved = ::getaddrinfo(url.host.c_str(), url.port.c_str(), &hints, &raw);
    if (resolved != 0) fail(std::string("HTTP DNS/地址解析失败：") + gai_strerror(resolved));
    std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> addresses(raw, ::freeaddrinfo);
    const Deadline deadline = Clock::now() + std::chrono::seconds(15);
    int last_error = ECONNREFUSED;
    for (addrinfo* address = raw; address; address = address->ai_next) {
        remaining(deadline);
        do {
            remaining(deadline);
            socket.reset(::socket(address->ai_family, address->ai_socktype, address->ai_protocol));
        } while (socket.get() < 0 && errno == EINTR);
        if (socket.get() < 0) { last_error = errno; continue; }
        const auto control = [&](int command, int value) {
            int result;
            do {
                remaining(deadline);
                result = ::fcntl(socket.get(), command, value);
            } while (result < 0 && errno == EINTR);
            return result;
        };
        const int flags = control(F_GETFL, 0);
        if (flags < 0 || control(F_SETFL, flags | O_NONBLOCK) < 0) {
            last_error = errno; continue;
        }
        const int fdflags = control(F_GETFD, 0);
        if (fdflags < 0 || control(F_SETFD, fdflags | FD_CLOEXEC) < 0) {
            last_error = errno; continue;
        }
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
        const int one = 1;
        if (::setsockopt(socket.get(), SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) < 0) {
            last_error = errno; continue;
        }
#elif !defined(MSG_NOSIGNAL)
#error "SDK_Http requires MSG_NOSIGNAL or SO_NOSIGPIPE to suppress SIGPIPE safely"
#endif
        if (::connect(socket.get(), address->ai_addr, address->ai_addrlen) == 0) { remaining(deadline); return; }
        if (errno != EINPROGRESS && errno != EINTR && errno != EWOULDBLOCK) { last_error = errno; continue; }
        ready(socket.get(), POLLOUT, deadline);
        int error = 0;
        socklen_t length = sizeof(error);
        int result;
        do {
            remaining(deadline);
            result = ::getsockopt(socket.get(), SOL_SOCKET, SO_ERROR, &error, &length);
        } while (result < 0 && errno == EINTR);
        if (result < 0) error = errno;
        if (error == 0) { remaining(deadline); return; }
        last_error = error;
    }
    fail(std::string("HTTP 连接失败：") + std::strerror(last_error));
}
inline void send_all(int fd, const std::string& data, Deadline deadline) {
    std::size_t offset = 0;
    while (offset < data.size()) {
        remaining(deadline);
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;
#endif
        const ssize_t count = ::send(fd, data.data() + offset, data.size() - offset, flags);
        if (count > 0) offset += static_cast<std::size_t>(count);
        else if (count < 0 && errno == EINTR) continue;
        else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) ready(fd, POLLOUT, deadline);
        else fail(std::string("HTTP 发送失败：") + (count == 0 ? "连接已关闭" : std::strerror(errno)));
    }
}
class Reader {
    int fd_;
    Deadline deadline_;
    std::string pending_;
    std::size_t offset_ = 0;
public:
    Reader(int fd, Deadline deadline) : fd_(fd), deadline_(deadline) {}
    bool fill() {
        for (;;) {
            remaining(deadline_);
            char bytes[16384];
            const ssize_t count = ::recv(fd_, bytes, sizeof(bytes), 0);
            if (count > 0) {
                if (offset_) { pending_.erase(0, offset_); offset_ = 0; }
                pending_.append(bytes, static_cast<std::size_t>(count));
                return true;
            }
            if (count == 0) return false;
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) ready(fd_, POLLIN, deadline_);
            else fail(std::string("HTTP 接收失败：") + std::strerror(errno));
        }
    }
    std::string line(std::size_t& budget) {
        for (;;) {
            const std::size_t end = pending_.find("\r\n", offset_);
            if (end != std::string::npos) {
                const std::size_t size = end - offset_ + 2;
                if (size > budget) fail("HTTP 响应头、分块行或尾部超过 32 KiB 限制");
                budget -= size;
                std::string result = pending_.substr(offset_, size - 2);
                offset_ = end + 2;
                return result;
            }
            if (pending_.size() - offset_ >= budget) fail("HTTP 响应头、分块行或尾部超过 32 KiB 限制");
            if (!fill()) fail("HTTP 响应头或分块行被截断");
        }
    }
    void take(std::size_t count, std::string& out) {
        if (count > MaxBytes - out.size()) fail("HTTP 响应正文超过 8 MiB");
        while (count) {
            if (offset_ == pending_.size() && !fill()) fail("HTTP 响应正文被截断");
            const std::size_t part = std::min(count, pending_.size() - offset_);
            out.append(pending_, offset_, part);
            offset_ += part;
            count -= part;
        }
    }
    void close_body(std::string& out) {
        for (;;) {
            take(pending_.size() - offset_, out);
            if (!fill()) return;
        }
    }
    void finish() {
        // 本客户端发送 Connection: close；等待 EOF 才能发现分段到达的超长正文。
        // 忽略 close 的服务器会在同一收发总截止时间到达时失败。
        if (offset_ < pending_.size() || fill()) fail("HTTP 响应正文超过声明长度或结束边界");
    }
};
struct Headers {
    bool has_length = false, chunked = false, encoded = false;
    std::size_t length = 0;
};
inline void field(const std::string& line, std::string& name, std::string& value) {
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos || colon == 0) fail("HTTP 响应头格式无效");
    for (std::size_t i = 0; i < colon; ++i) if (!token(static_cast<unsigned char>(line[i]))) fail("HTTP 响应头名称无效");
    for (std::size_t i = colon + 1; i < line.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(line[i]);
        if ((c < 0x20 && c != '\t') || c == 0x7f) fail("HTTP 响应头含控制字符");
    }
    name = lower(line.substr(0, colon));
    value = trim(line.substr(colon + 1));
}
inline Headers read_headers(Reader& reader, std::size_t& budget) {
    Headers headers;
    for (;;) {
        const std::string line = reader.line(budget);
        if (line.empty()) break;
        std::string name, value;
        field(line, name, value);
        if (name == "content-length") {
            // 拒绝重复字段/逗号列表，即使数值相同，避免不同解析器产生歧义。
            if (headers.has_length) fail("HTTP Content-Length 重复或冲突");
            headers.length = number(value, 10, MaxBytes);
            headers.has_length = true;
        } else if (name == "transfer-encoding") {
            if (headers.chunked || lower(value) != "chunked") fail("HTTP Transfer-Encoding 重复或不支持");
            headers.chunked = true;
        } else if (name == "content-encoding") {
            if (headers.encoded || lower(value) != "identity") fail("HTTP 不支持压缩或重复 Content-Encoding");
            headers.encoded = true;
        }
    }
    if (headers.has_length && headers.chunked) fail("HTTP Content-Length 与 Transfer-Encoding 冲突");
    return headers;
}
inline int read_status(Reader& reader, std::size_t& budget) {
    const std::string line = reader.line(budget);
    if (line.size() < 12 || (lower(line.substr(0, 9)) != "http/1.1 " && lower(line.substr(0, 9)) != "http/1.0 ") ||
        line[9] < '1' || line[9] > '5' || line[10] < '0' || line[10] > '9' || line[11] < '0' || line[11] > '9' ||
        (line.size() > 12 && line[12] != ' ')) fail("HTTP 状态行无效");
    for (std::size_t i = 12; i < line.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(line[i]);
        if ((c < 0x20 && c != '\t') || c == 0x7f) fail("HTTP 状态行含控制字符");
    }
    return (line[9] - '0') * 100 + (line[10] - '0') * 10 + line[11] - '0';
}
inline void chunk_extension(const std::string& line, std::size_t pos) {
    // RFC chunk-ext：token 名，可选 token 或带反斜线转义的 quoted-string 值。
    while (pos < line.size()) {
        if (line[pos++] != ';') fail("HTTP 分块扩展无效");
        const std::size_t start = pos;
        while (pos < line.size() && token(static_cast<unsigned char>(line[pos]))) ++pos;
        if (pos == start) fail("HTTP 分块扩展名称为空");
        if (pos == line.size() || line[pos] == ';') continue;
        if (line[pos++] != '=' || pos == line.size()) fail("HTTP 分块扩展值无效");
        if (line[pos] == '"') {
            ++pos;
            bool closed = false;
            while (pos < line.size()) {
                unsigned char c = static_cast<unsigned char>(line[pos++]);
                if (c == '"') { closed = true; break; }
                if (c == '\\') {
                    if (pos == line.size()) fail("HTTP 分块扩展转义被截断");
                    c = static_cast<unsigned char>(line[pos++]);
                }
                if ((c < 0x20 && c != '\t') || c == 0x7f) fail("HTTP 分块扩展含控制字符");
            }
            if (!closed) fail("HTTP 分块扩展引号未闭合");
        } else {
            const std::size_t begin = pos;
            while (pos < line.size() && token(static_cast<unsigned char>(line[pos]))) ++pos;
            if (pos == begin) fail("HTTP 分块扩展值无效");
        }
    }
}
inline void read_body(Reader& reader, const Headers& headers, int status, std::size_t& budget, std::string& body) {
    if (status == 204 || status == 304) {
        if (headers.chunked || (headers.has_length && headers.length != 0)) fail("HTTP 无正文状态包含冲突长度");
    } else if (headers.chunked) {
        for (;;) {
            // 分块大小行各自受限；响应头和 trailer 则共用累计预算。
            std::size_t line_budget = MaxHeaders;
            const std::string line = reader.line(line_budget);
            const std::size_t semicolon = line.find(';');
            const std::size_t size = number(line.substr(0, semicolon), 16, MaxBytes - body.size());
            if (semicolon != std::string::npos) chunk_extension(line, semicolon);
            if (size == 0) break;
            reader.take(size, body);
            std::size_t ending_budget = 2;
            if (!reader.line(ending_budget).empty()) fail("HTTP 分块正文缺少结束换行");
        }
        for (;;) {
            const std::string line = reader.line(budget);
            if (line.empty()) break;
            std::string name, value;
            field(line, name, value);
            if (name == "content-length" || name == "transfer-encoding" || name == "host" ||
                name == "content-encoding" || name == "trailer" || name == "connection")
                fail("HTTP trailer 包含禁止或冲突字段");
        }
    } else if (headers.has_length) reader.take(headers.length, body);
    else { reader.close_body(body); return; }
    reader.finish();
}
} // namespace HttpDetail

namespace {
// HTTP 状态码 → 中文含义（便于日志定位；不改变协议语义）
static std::string http_status_hint(long long status)
{
    switch (status)
    {
    case 400: return "请求格式错误：检查请求体是否为 JSON、字段是否完整（服务端拒绝解析时不会给出业务码）";
    case 401: return "未授权：账号/口令/Token 无效或已过期（管理接口或需要登录的接口）";
    case 402: return "需要付费：账户余额不足或未开通";
    case 403: return "被拒绝：无权限、IP 被限制、WAF/防火墙拦截、或该接口不允许当前来源访问";
    case 404: return "接口不存在：检查 api_url 是否以 /api/ 结尾（正确示例 http://api.xn--0try77c6um82w.cn/api/），以及接口文件名是否正确";
    case 405: return "请求方法不允许：这些接口只接受 POST，不要用 GET 访问";
    case 408: return "请求超时：网络慢或服务端繁忙，稍后重试";
    case 413: return "请求体过大：卡密数量/参数太多，分批请求";
    case 429: return "触发频率限制：短时间请求过多，降低频率或稍后再试";
    case 500: return "服务端内部错误：后端异常，查看服务端日志或联系管理员";
    case 502: return "网关错误：后端 PHP-FPM 未启动/重启中/超时";
    case 503: return "服务暂不可用：站点维护、进程池满、或反向代理拒绝";
    case 504: return "网关超时：后端处理时间过长（通常是数据库锁/慢查询）";
    default:  return status >= 500 ? "服务端错误" : "非预期的 HTTP 状态";
    }
}
} // namespace


inline bool http_post(const std::string& url, const std::string& body, const std::string& content_type,
                      std::string& response, std::string& error, int& status) {
    response.clear();
    error.clear();
    status = 0;
    try {
        using namespace HttpDetail;
        const Url target = parse_url(url);
        if (body.size() > MaxBytes) fail("HTTP 请求正文超过 8 MiB");
        if (content_type.empty() || content_type.size() > 1024) fail("HTTP Content-Type 为空或过长");
        for (unsigned char c : content_type) if (c < 0x20 || c >= 0x7f) fail("HTTP Content-Type 含非法字符");
        const std::string headers = "POST " + target.target + " HTTP/1.1\r\nHost: " + target.authority +
            "\r\nContent-Type: " + content_type + "\r\nContent-Length: " + std::to_string(body.size()) +
            "\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n";
        if (headers.size() > MaxHeaders) fail("HTTP 请求头超过 32 KiB");
        Socket socket;
        connect_socket(socket, target);
        // 从首次发送开始，发送和接收共用一个 30 秒总截止时间。
        const Deadline deadline = Clock::now() + std::chrono::seconds(30);
        send_all(socket.get(), headers, deadline);
        send_all(socket.get(), body, deadline);
        Reader reader(socket.get(), deadline);
        std::size_t budget = MaxHeaders;
        Headers framing;
        unsigned informational = 0;
        for (;;) {
            status = read_status(reader, budget);
            framing = read_headers(reader, budget);
            if (status >= 200) break;
            if (status == 101 || ++informational > 8 || framing.has_length || framing.chunked)
                fail("HTTP 协议升级或临时响应格式不支持");
        }
        std::string received;
        read_body(reader, framing, status, budget, received);
        remaining(deadline);
        if (status < 200 || status >= 300)
            fail("HTTP " + std::to_string(status) + " - " + http_status_hint(status));
        response.swap(received);
        return true;
    } catch (const std::exception& failure) {
        error = failure.what();
    } catch (...) {
        error = "HTTP 请求发生未知错误";
    }
    response.clear();
    return false;
}
} // namespace JiYanNet

#endif // JIYAN_SDK_HTTP_CPP
