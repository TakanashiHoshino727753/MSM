import Foundation

// MARK: - 数据模型（对应 WebUI JSON API）

struct ServerSummary: Codable, Identifiable {
    var id: String { name }
    let name: String
    let type: String
    let running: Bool
    let version: String
    let port: Int
    let players: Int
    let maxPlayers: Int
    let mcVersion: String
    let loader: String
    let path: String
    let hasError: Bool
}

struct ServerDetail: Codable {
    let name: String
    let type: String
    let running: Bool
    let version: String
    let mcVersion: String
    let loader: String
    let port: Int
    let path: String
    let players: Int
    let maxPlayers: Int
    let eulaAccepted: Bool
    let console: String
    let playerList: [String]
    let properties: [String: String]
    let javaInfo: String
}

struct ErrorRecord: Codable, Identifiable {
    var id: String { path + type }
    let name: String
    let path: String
    let type: String
    let typeLabel: String
    let time: String
    let logTail: String
    let retrying: Bool
    let retryCount: Int
    let maxRetries: Int
    let nextRetryInSec: Int
    let fatal: Bool
    let autoRestart: Bool
}

struct SystemInfo: Codable {
    let hostname: String
    let os: String
    let arch: String
    let cpuCores: Int
    let totalMemMB: Int64
    let freeMemMB: Int64
    let javaAvailable: Bool
    let proxyCount: Int
    let botLinked: Bool
}

struct PairInfo: Codable {
    let code: String
    let uri: String
    let remainSec: Int
    let used: Bool
}

struct PairResult: Codable {
    let token: String
    let port: Int
    let https: Bool
}

struct SavedConnection: Codable {
    let host: String
    let port: Int
    let useHttps: Bool
    let token: String
    var baseURL: String { "\(useHttps ? "https" : "http")://\(host):\(port)" }
}
