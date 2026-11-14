import Foundation
import Darwin

public final class OBDConnection {
    private var sockfd: Int32 = -1
    private let readQueue = DispatchQueue(label: "com.dsobd.socket.read", qos: .userInitiated)
    private var isReading = false

    public var onBytesReceived: ((Data) -> Void)?
    public var onDisconnected: ((String) -> Void)?

    public init() {}

    public var isConnected: Bool {
        return sockfd >= 0
    }

    public func connect(host: String, port: Int, timeoutSeconds: Double = 5.0) -> Result<Void, Error> {
        disconnect()

        var hints = addrinfo(
            ai_flags: AI_ADDRCONFIG,
            ai_family: AF_INET,
            ai_socktype: SOCK_STREAM,
            ai_protocol: IPPROTO_TCP,
            ai_addrlen: 0,
            ai_canonname: nil,
            ai_addr: nil,
            ai_next: nil
        )

        var res: UnsafeMutablePointer<addrinfo>?
        let portStr = String(port)
        let status = getaddrinfo(host, portStr, &hints, &res)
        guard status == 0, let servinfo = res else {
            let err = String(cString: gai_strerror(status))
            return .failure(NSError(domain: "OBDConnection", code: 1, userInfo: [NSLocalizedDescriptionKey: "Failed to resolve \(host): \(err)"]))
        }
        defer { freeaddrinfo(servinfo) }

        let sock = Darwin.socket(servinfo.pointee.ai_family, servinfo.pointee.ai_socktype, servinfo.pointee.ai_protocol)
        guard sock >= 0 else {
            return .failure(NSError(domain: "OBDConnection", code: 2, userInfo: [NSLocalizedDescriptionKey: "Socket creation failed: \(String(cString: strerror(errno)))"]))
        }

        // 1. Enable TCP_NODELAY (Crucial: prevents Nagle algorithm stalling small ELM AT writes)
        var nodelay: Int32 = 1
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, socklen_t(MemoryLayout<Int32>.size))

        // 2. Enable SO_KEEPALIVE
        var keepalive: Int32 = 1
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, socklen_t(MemoryLayout<Int32>.size))

        // 3. Set non-blocking for connect timeout
        let flags = fcntl(sock, F_GETFL, 0)
        _ = fcntl(sock, F_SETFL, flags | O_NONBLOCK)

        let connRes = Darwin.connect(sock, servinfo.pointee.ai_addr, servinfo.pointee.ai_addrlen)
        if connRes < 0 && errno != EINPROGRESS {
            close(sock)
            return .failure(NSError(domain: "OBDConnection", code: 3, userInfo: [NSLocalizedDescriptionKey: "Connect failed: \(String(cString: strerror(errno)))"]))
        }

        if connRes != 0 {
            var fdset = fd_set()
            fdset.zero()
            fdset.set(sock)

            var tv = timeval(tv_sec: Int(timeoutSeconds), tv_usec: Int32((timeoutSeconds.truncatingRemainder(dividingBy: 1)) * 1_000_000))
            let selRes = select(sock + 1, nil, &fdset, nil, &tv)
            if selRes <= 0 {
                close(sock)
                return .failure(NSError(domain: "OBDConnection", code: 4, userInfo: [NSLocalizedDescriptionKey: "Connection timed out after \(timeoutSeconds)s"]))
            }

            var so_error: Int32 = 0
            var len = socklen_t(MemoryLayout<Int32>.size)
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len)
            if so_error != 0 {
                close(sock)
                return .failure(NSError(domain: "OBDConnection", code: 5, userInfo: [NSLocalizedDescriptionKey: "Connect error: \(String(cString: strerror(so_error)))"]))
            }
        }

        // Restore blocking with read timeout
        _ = fcntl(sock, F_SETFL, flags & ~O_NONBLOCK)
        var readTimeout = timeval(tv_sec: 1, tv_usec: 0)
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &readTimeout, socklen_t(MemoryLayout<timeval>.size))

        self.sockfd = sock
        startReading()
        return .success(())
    }

    public func send(string: String) -> Bool {
        guard sockfd >= 0 else { return false }
        let data = Data(string.utf8)
        return send(data: data)
    }

    public func send(data: Data) -> Bool {
        guard sockfd >= 0 else { return false }
        var bytesWritten = 0
        let total = data.count

        while bytesWritten < total {
            let written: Int = data.withUnsafeBytes { raw in
                guard let base = raw.bindMemory(to: UInt8.self).baseAddress else { return -1 }
                return Darwin.send(sockfd, base + bytesWritten, total - bytesWritten, 0)
            }
            if written <= 0 {
                if errno == EAGAIN || errno == EWOULDBLOCK {
                    usleep(1000)
                    continue
                }
                disconnect(reason: "Send error: \(String(cString: strerror(errno)))")
                return false
            }
            bytesWritten += written
        }
        return true
    }

    private func startReading() {
        guard sockfd >= 0 && !isReading else { return }
        isReading = true
        let currentSock = sockfd

        readQueue.async { [weak self] in
            var buffer = [UInt8](repeating: 0, count: 2048)

            while let self = self, self.sockfd == currentSock {
                let n = Darwin.recv(currentSock, &buffer, buffer.count, 0)
                if n > 0 {
                    let chunk = Data(buffer[0..<n])
                    self.onBytesReceived?(chunk)
                } else if n == 0 {
                    // Socket closed by peer
                    DispatchQueue.main.async {
                        self.disconnect(reason: "Remote peer disconnected")
                    }
                    break
                } else {
                    if errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR {
                        continue
                    }
                    DispatchQueue.main.async {
                        self.disconnect(reason: "Recv error: \(String(cString: strerror(errno)))")
                    }
                    break
                }
            }
        }
    }

    public func disconnect(reason: String = "User disconnected") {
        guard sockfd >= 0 else { return }
        let sock = sockfd
        sockfd = -1
        isReading = false
        Darwin.shutdown(sock, SHUT_RDWR)
        Darwin.close(sock)
        onDisconnected?(reason)
    }
}

// Helpers for fd_set manipulation in Swift
private extension fd_set {
    mutating func zero() {
        memset(&self, 0, MemoryLayout<fd_set>.size)
    }

    mutating func set(_ fd: Int32) {
        let intOffset = Int(fd / 32)
        let bitOffset = fd % 32
        withUnsafeMutableBytes(of: &self) { raw in
            let ptr = raw.bindMemory(to: Int32.self)
            ptr[intOffset] |= (1 << bitOffset)
        }
    }
}
