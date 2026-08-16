import Foundation

// MARK: - API 客户端（自签 HTTPS 信任 + Bearer 鉴权 + 配对）

actor MsmClient {
    private var connection: SavedConnection?
    private var session: URLSession

    init() {
        let cfg = URLSessionConfiguration.ephemeral
        // 信任所有证书（仅用于自签 LAN 管理面板）
        cfg.urlCredentialStorage = nil
        self.session = URLSession(configuration: cfg,
            delegate: TrustAllDelegate(), delegateQueue: nil)
    }

    func configure(_ c: SavedConnection) { self.connection = c }

    private func request(_ path: String, method: String = "GET", body: Data? = nil) throws -> URLRequest {
        guard let c = connection else { throw APIError.notConfigured }
        var req = URLRequest(url: URL(string: c.baseURL + path)!)
        req.httpMethod = method
        req.setValue("Bearer \(c.token)", forHTTPHeaderField: "Authorization")
        if let body { req.httpBody = body; req.setValue("application/json", forHTTPHeaderField: "Content-Type") }
        return req
    }

    func get<T: Decodable>(_ path: String) async throws -> T {
        let (data, resp) = try await session.data(for: try request(path))
        try check(resp)
        return try JSONDecoder().decode(T.self, from: data)
    }

    func post(_ path: String, body: [String: Any] = [:]) async throws -> [String: Any] {
        let (data, resp) = try await session.data(for: try request(path, method: "POST",
            body: try JSONSerialization.data(withJSONObject: body)))
        try check(resp)
        return try JSONSerialization.jsonObject(with: data) as? [String: Any] ?? [:]
    }

    // 免令牌配对
    func pair(host: String, port: Int, useHttps: Bool, code: String) async throws -> PairResult {
        var req = URLRequest(url: URL(string: "\(useHttps ? "https" : "http")://\(host):\(port)/api/pair")!)
        req.httpMethod = "POST"
        req.httpBody = try JSONSerialization.data(withJSONObject: ["code": code.uppercased()])
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let (data, resp) = try await session.data(for: req)
        try check(resp)
        return try JSONDecoder().decode(PairResult.self, from: data)
    }

    private func check(_ resp: URLResponse) throws {
        let code = (resp as? HTTPURLResponse)?.statusCode ?? 0
        if code == 401 { throw APIError.unauthorized }
        if code >= 400 { throw APIError.http(code) }
    }

    enum APIError: Error { case notConfigured, unauthorized, http(Int) }
}

// 信任所有证书（仅本 App 内 LAN 管理用途）
final class TrustAllDelegate: NSObject, URLSessionDelegate {
    func urlSession(_ session: URLSession, didReceive challenge: URLAuthenticationChallenge,
                    completionHandler: @escaping (URLSession.AuthChallengeDisposition, URLCredential?) -> Void) {
        if let serverTrust = challenge.protectionSpace.serverTrust {
            completionHandler(.useCredential, URLCredential(trust: serverTrust))
        } else {
            completionHandler(.performDefaultHandling, nil)
        }
    }
}
