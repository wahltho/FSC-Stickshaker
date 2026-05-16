#include "FSCStickShaker/Transport.h"

#include "FSCStickShaker/Log.h"
#include "FSCStickShaker/Protocol.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace fsc::stickshaker {
namespace {

#if defined(_WIN32)
using socket_t = SOCKET;
using socklen_compat_t = int;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
using socket_t = int;
using socklen_compat_t = socklen_t;
constexpr socket_t kInvalidSocket = -1;
#endif

void closeSocket(socket_t& socket)
{
    if (socket == kInvalidSocket) {
        return;
    }
#if defined(_WIN32)
    closesocket(socket);
#else
    close(socket);
#endif
    socket = kInvalidSocket;
}

std::string lastSocketError()
{
#if defined(_WIN32)
    return "WSA error " + std::to_string(WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

bool setSocketBlocking(socket_t socket, bool blocking)
{
#if defined(_WIN32)
    u_long mode = blocking ? 0UL : 1UL;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    const int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    const int updated = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    return fcntl(socket, F_SETFL, updated) == 0;
#endif
}

bool isConnectInProgress()
{
#if defined(_WIN32)
    const int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEINVAL;
#else
    return errno == EINPROGRESS || errno == EWOULDBLOCK;
#endif
}

bool finishNonBlockingConnect(socket_t socket, int timeoutMs)
{
    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(socket, &writeSet);

    timeval timeout {};
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

#if defined(_WIN32)
    constexpr int nfds = 0;
#else
    const int nfds = socket + 1;
#endif
    const int selected = select(nfds, nullptr, &writeSet, nullptr, &timeout);
    if (selected == 0) {
#if defined(_WIN32)
        WSASetLastError(WSAETIMEDOUT);
#else
        errno = ETIMEDOUT;
#endif
        return false;
    }
    if (selected < 0) {
        return false;
    }

    int socketError = 0;
#if defined(_WIN32)
    int socketErrorSize = sizeof(socketError);
#else
    socklen_t socketErrorSize = sizeof(socketError);
#endif
    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &socketErrorSize) != 0) {
        return false;
    }
    if (socketError != 0) {
#if defined(_WIN32)
        WSASetLastError(socketError);
#else
        errno = socketError;
#endif
        return false;
    }
    return true;
}

bool connectWithTimeout(socket_t socket, const addrinfo& candidate, int timeoutMs)
{
    if (!setSocketBlocking(socket, false)) {
        return false;
    }
    if (::connect(socket, candidate.ai_addr, static_cast<int>(candidate.ai_addrlen)) == 0) {
        return setSocketBlocking(socket, true);
    }
    if (!isConnectInProgress()) {
        return false;
    }
    if (!finishNonBlockingConnect(socket, timeoutMs)) {
        return false;
    }
    return setSocketBlocking(socket, true);
}

void setSocketSendTimeout(socket_t socket, int timeoutMs)
{
#if defined(_WIN32)
    const DWORD timeout = static_cast<DWORD>(timeoutMs);
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    timeval timeout {};
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

class LoggingTransport final : public ITransport {
public:
    bool open(const Config& config) override
    {
        selectedTransport_ = config.transport;
        logInfo("transport open: " + name());
        return true;
    }

    void close() override
    {
        logInfo("transport close: " + name());
    }

    bool send(bool active) override
    {
        if (selectedTransport_ == TransportKind::Udp || selectedTransport_ == TransportKind::Tcp) {
            const auto frames = asciiRelayFrames(active, {1, 2});
            logInfo(toString(selectedTransport_) + " send log: " + (frames.empty() ? "<none>" : frames[0]));
        } else {
            logInfo("serial send log: " + bytesToHex(serialFrame(active)));
        }
        return true;
    }

    std::string name() const override
    {
        return toString(selectedTransport_);
    }

private:
    TransportKind selectedTransport_ = TransportKind::LogOnly;
};

class SerialTransport final : public ITransport {
public:
    ~SerialTransport() override
    {
        close();
    }

    bool open(const Config& config) override
    {
        close();
        config_ = config.serial;
        if (config_.port.empty()) {
            logInfo("serial port is not configured");
            return false;
        }

#if defined(_WIN32)
        std::string path = config_.port;
        if (path.rfind("\\\\.\\", 0) != 0) {
            path = "\\\\.\\" + path;
        }

        handle_ = CreateFileA(path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            logInfo("serial open failed for " + config_.port + ": Windows error " + std::to_string(GetLastError()));
            return false;
        }

        DCB dcb {};
        dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(handle_, &dcb)) {
            logInfo("serial GetCommState failed: Windows error " + std::to_string(GetLastError()));
            close();
            return false;
        }

        dcb.BaudRate = static_cast<DWORD>(config_.baud);
        dcb.ByteSize = static_cast<BYTE>(config_.dataBits);
        dcb.Parity = parityFromString(config_.parity);
        dcb.StopBits = config_.stopBits == 2 ? TWOSTOPBITS : ONESTOPBIT;
        dcb.fBinary = TRUE;
        dcb.fParity = dcb.Parity == NOPARITY ? FALSE : TRUE;
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;

        if (!SetCommState(handle_, &dcb)) {
            logInfo("serial SetCommState failed: Windows error " + std::to_string(GetLastError()));
            close();
            return false;
        }

        COMMTIMEOUTS timeouts {};
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.WriteTotalTimeoutConstant = 50;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        SetCommTimeouts(handle_, &timeouts);
#else
        fd_ = ::open(config_.port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd_ < 0) {
            logInfo("serial open failed for " + config_.port + ": " + std::strerror(errno));
            return false;
        }

        termios tty {};
        if (tcgetattr(fd_, &tty) != 0) {
            logInfo("serial tcgetattr failed: " + std::string(std::strerror(errno)));
            close();
            return false;
        }

        cfmakeraw(&tty);
        tty.c_cflag |= CLOCAL | CREAD;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= dataBitsFlag(config_.dataBits);
        tty.c_cflag &= ~CSTOPB;
        if (config_.stopBits == 2) {
            tty.c_cflag |= CSTOPB;
        }
        configureParity(tty, config_.parity);
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 10;

        const speed_t baud = baudFlag(config_.baud);
        if (baud == 0) {
            logInfo("unsupported serial baud rate: " + std::to_string(config_.baud));
            close();
            return false;
        }
        cfsetispeed(&tty, baud);
        cfsetospeed(&tty, baud);

        if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
            logInfo("serial tcsetattr failed: " + std::string(std::strerror(errno)));
            close();
            return false;
        }

#endif

        logInfo("serial open: " + config_.port + " @" + std::to_string(config_.baud));
        return true;
    }

    void close() override
    {
#if defined(_WIN32)
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
#else
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
#endif
    }

    bool send(bool active) override
    {
        const auto frame = serialFrame(active);
        const bool ok = writeAll(frame.data(), frame.size());
        if (!ok) {
            logInfo("serial write failed");
            close();
        }
        return ok;
    }

    std::string name() const override
    {
        return "serial";
    }

private:
#if defined(_WIN32)
    static BYTE parityFromString(const std::string& parity)
    {
        if (parity == "even") {
            return EVENPARITY;
        }
        if (parity == "odd") {
            return ODDPARITY;
        }
        return NOPARITY;
    }
#else
    static tcflag_t dataBitsFlag(int dataBits)
    {
        switch (dataBits) {
        case 5:
            return CS5;
        case 6:
            return CS6;
        case 7:
            return CS7;
        default:
            return CS8;
        }
    }

    static void configureParity(termios& tty, const std::string& parity)
    {
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~PARODD;
        if (parity == "even") {
            tty.c_cflag |= PARENB;
        } else if (parity == "odd") {
            tty.c_cflag |= PARENB;
            tty.c_cflag |= PARODD;
        }
    }

    static speed_t baudFlag(int baud)
    {
        switch (baud) {
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
#if defined(B230400)
        case 230400:
            return B230400;
#endif
        default:
            return 0;
        }
    }
#endif

    bool writeAll(const unsigned char* data, std::size_t size)
    {
#if defined(_WIN32)
        DWORD written = 0;
        return WriteFile(handle_, data, static_cast<DWORD>(size), &written, nullptr) && written == size;
#else
        std::size_t offset = 0;
        while (offset < size) {
            const ssize_t written = ::write(fd_, data + offset, size - offset);
            if (written < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return false;
            }
            offset += static_cast<std::size_t>(written);
        }
        return true;
#endif
    }

    SerialConfig config_;
#if defined(_WIN32)
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif
};

class UdpTransport final : public ITransport {
public:
    ~UdpTransport() override
    {
        close();
    }

    bool open(const Config& config) override
    {
        close();
        config_ = config.udp;
        if (config_.ip.empty() || config_.destinationPort <= 0) {
            logInfo("UDP target is not configured");
            return false;
        }

#if defined(_WIN32)
        if (!wsaStarted_) {
            WSADATA data {};
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
                logInfo("WSAStartup failed");
                return false;
            }
            wsaStarted_ = true;
        }
#endif

        addrinfo hints {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_NUMERICHOST;

        addrinfo* result = nullptr;
        const std::string destinationPort = std::to_string(config_.destinationPort);
        const int gai = getaddrinfo(config_.ip.c_str(), destinationPort.c_str(), &hints, &result);
        if (gai != 0) {
            logInfo("UDP resolve failed for " + config_.ip + ":" + destinationPort);
            return false;
        }

        for (addrinfo* candidate = result; candidate != nullptr; candidate = candidate->ai_next) {
            socket_ = ::socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
            if (socket_ == kInvalidSocket) {
                continue;
            }

            targetLength_ = static_cast<socklen_compat_t>(candidate->ai_addrlen);
            std::memcpy(&target_, candidate->ai_addr, candidate->ai_addrlen);
            break;
        }

        freeaddrinfo(result);

        if (socket_ == kInvalidSocket) {
            logInfo("UDP socket failed for " + config_.ip + ":" + destinationPort + ": " + lastSocketError());
            return false;
        }

        bindSourcePortIfConfigured();
        setSocketSendTimeout(socket_, kSendTimeoutMs);
        logInfo("UDP ready: " + config_.ip + ":" + destinationPort);
        return true;
    }

    void close() override
    {
        closeSocket(socket_);
        targetLength_ = 0;
    }

    bool send(bool active) override
    {
        const auto frames = asciiRelayFrames(active, config_.relayChannels);
        for (const auto& frame : frames) {
            const int sent = ::sendto(socket_,
                frame.data(),
                static_cast<int>(frame.size()),
                0,
                reinterpret_cast<const sockaddr*>(&target_),
                targetLength_);
            if (sent != static_cast<int>(frame.size())) {
                logInfo("UDP send failed: " + lastSocketError());
                return false;
            }
        }
        logInfo(std::string("UDP sent ") + (active ? "ON" : "OFF") + ": " + (frames.empty() ? "<none>" : frames.front()));
        return true;
    }

    std::string name() const override
    {
        return "udp";
    }

private:
    static constexpr int kSendTimeoutMs = 50;

    void bindSourcePortIfConfigured()
    {
        if (config_.sourcePort <= 0) {
            return;
        }

        sockaddr_storage local {};
        socklen_compat_t localLength = 0;
        if (target_.ss_family == AF_INET) {
            auto* addr = reinterpret_cast<sockaddr_in*>(&local);
            addr->sin_family = AF_INET;
            addr->sin_addr.s_addr = htonl(INADDR_ANY);
            addr->sin_port = htons(static_cast<unsigned short>(config_.sourcePort));
            localLength = sizeof(sockaddr_in);
        } else if (target_.ss_family == AF_INET6) {
            auto* addr = reinterpret_cast<sockaddr_in6*>(&local);
            addr->sin6_family = AF_INET6;
            addr->sin6_port = htons(static_cast<unsigned short>(config_.sourcePort));
            localLength = sizeof(sockaddr_in6);
        }

        if (localLength == 0) {
            return;
        }

        if (::bind(socket_, reinterpret_cast<const sockaddr*>(&local), localLength) != 0) {
            logInfo("UDP source port bind failed for " + std::to_string(config_.sourcePort) + ": " + lastSocketError());
        }
    }

    UdpConfig config_;
    socket_t socket_ = kInvalidSocket;
    sockaddr_storage target_ {};
    socklen_compat_t targetLength_ = 0;
#if defined(_WIN32)
    bool wsaStarted_ = false;
#endif
};

class TcpTransport final : public ITransport {
public:
    ~TcpTransport() override
    {
        close();
    }

    bool open(const Config& config) override
    {
        close();
        config_ = config.tcp;
        if (config_.ip.empty() || config_.port <= 0) {
            logInfo("TCP target is not configured");
            return false;
        }

#if defined(_WIN32)
        if (!wsaStarted_) {
            WSADATA data {};
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
                logInfo("WSAStartup failed");
                return false;
            }
            wsaStarted_ = true;
        }
#endif

        addrinfo hints {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_NUMERICHOST;

        addrinfo* result = nullptr;
        const std::string port = std::to_string(config_.port);
        const int gai = getaddrinfo(config_.ip.c_str(), port.c_str(), &hints, &result);
        if (gai != 0) {
            logInfo("TCP resolve failed for " + config_.ip + ":" + port);
            return false;
        }

        std::string connectError;
        for (addrinfo* candidate = result; candidate != nullptr; candidate = candidate->ai_next) {
            socket_ = ::socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
            if (socket_ == kInvalidSocket) {
                connectError = lastSocketError();
                continue;
            }
            if (connectWithTimeout(socket_, *candidate, kConnectTimeoutMs)) {
                break;
            }
            connectError = lastSocketError();
            closeSocket(socket_);
        }

        freeaddrinfo(result);

        if (socket_ == kInvalidSocket) {
            if (connectError.empty()) {
                connectError = lastSocketError();
            }
            logInfo("TCP connect failed to " + config_.ip + ":" + port + ": " + connectError);
            return false;
        }

        setSocketSendTimeout(socket_, kSendTimeoutMs);
        logInfo("TCP connected: " + config_.ip + ":" + port);
        return true;
    }

    void close() override
    {
        closeSocket(socket_);
    }

    bool send(bool active) override
    {
        const auto frames = tcpFrames(active);
        for (const auto& frame : frames) {
            if (!sendAll(frame.data(), frame.size())) {
                logInfo("TCP send failed");
                close();
                return false;
            }
        }
        return true;
    }

    std::string name() const override
    {
        return "tcp";
    }

private:
    static constexpr int kConnectTimeoutMs = 50;
    static constexpr int kSendTimeoutMs = 50;

    bool sendAll(const char* data, std::size_t size)
    {
        std::size_t offset = 0;
        while (offset < size) {
#if defined(_WIN32)
            const int sent = ::send(socket_, data + offset, static_cast<int>(size - offset), 0);
#else
            const ssize_t sent = ::send(socket_, data + offset, size - offset, 0);
#endif
            if (sent <= 0) {
#if !defined(_WIN32)
                if (errno == EINTR) {
                    continue;
                }
#endif
                return false;
            }
            offset += static_cast<std::size_t>(sent);
        }
        return true;
    }

    TcpConfig config_;
    socket_t socket_ = kInvalidSocket;
#if defined(_WIN32)
    bool wsaStarted_ = false;
#endif
};

} // namespace

std::unique_ptr<ITransport> makeTransport(const Config& config)
{
    switch (config.transport) {
    case TransportKind::Serial:
        return std::make_unique<SerialTransport>();
    case TransportKind::Udp:
        return std::make_unique<UdpTransport>();
    case TransportKind::Tcp:
        return std::make_unique<TcpTransport>();
    case TransportKind::LogOnly:
        return std::make_unique<LoggingTransport>();
    }
    return std::make_unique<LoggingTransport>();
}

} // namespace fsc::stickshaker
