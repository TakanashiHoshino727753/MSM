import Foundation
import SwiftUI

@MainActor
final class AppViewModel: ObservableObject {
    @Published var connected = false
    @Published var connecting = false
    @Published var errorMsg: String?
    @Published var system: SystemInfo?
    @Published var servers: [ServerSummary] = []
    @Published var errors: [ErrorRecord] = []
    @Published var detail: ServerDetail?
    @Published var consoleText = ""
    @Published var players: [String] = []

    private var client = MsmClient()
    private let store = ConnectionStore()

    init() {
        if let c = store.load() { Task { await connect(c) } }
    }

    func connect(_ c: SavedConnection) async {
        connecting = true; errorMsg = nil
        await client.configure(c)
        do {
            let _: SystemInfo = try await client.get("/api/system")
            connected = true; connecting = false
            store.save(c)
            await refreshAll()
        } catch {
            self.errorMsg = error.localizedDescription
            connected = false; connecting = false
        }
    }

    func connectWithPair(host: String, port: Int, useHttps: Bool, code: String) async {
        connecting = true; errorMsg = nil
        do {
            let r = try await client.pair(host: host, port: port, useHttps: useHttps, code: code)
            let c = SavedConnection(host: host, port: r.port, useHttps: r.https, token: r.token)
            await connect(c)
        } catch {
            errorMsg = error.localizedDescription; connecting = false
        }
    }

    func connectWithUri(_ uri: String) {
        // msm://token@host:port?t=TOKEN
        guard uri.hasPrefix("msm://") else { return }
        let noScheme = uri.dropFirst("msm://".count)
        let parts = noScheme.split(separator: "?")
        let authHost = String(parts.first ?? "")
        let query = parts.count > 1 ? String(parts[1]) : ""
        let token = (query.split(separator: "=").last).map(String.init) ?? ""
        let pair = authHost.split(separator: "@")
        let hostPort = pair.count > 1 ? String(pair[1]) : String(pair[0])
        let hp = hostPort.split(separator: ":")
        let host = String(hp.first ?? "")
        let port = Int(hp.count > 1 ? String(hp[1]) : "25580") ?? 25580
        Task { await connect(SavedConnection(host: host, port: port, useHttps: false, token: token)) }
    }

    func disconnect() { store.clear(); connected = false; servers = []; errors = []; detail = nil }

    func refreshAll() async {
        do {
            servers = try await client.get("/api/servers")
            errors = try await client.get("/api/errors")
        } catch { }
    }

    func selectServer(_ name: String) async {
        do {
            detail = try await client.get("/api/servers/\(name)")
            let c: [String: Any] = try await client.get("/api/servers/\(name)/console")
            consoleText = c["text"] as? String ?? ""
            let p: [String: Any] = try await client.get("/api/servers/\(name)/players")
            players = (p["players"] as? [String]) ?? []
        } catch { }
    }

    func clearSelection() { detail = nil; consoleText = ""; players = [] }

    func start(_ name: String) async { try? await client.post("/api/servers/\(name)/start"); await refreshAll() }
    func stop(_ name: String) async { try? await client.post("/api/servers/\(name)/stop"); await refreshAll() }
    func sendCommand(_ name: String, _ cmd: String) async {
        try? await client.post("/api/servers/\(name)/command", body: ["command": cmd])
        await selectServer(name)
    }
    func retryError(_ path: String) async { try? await client.post("/api/errors/\(path)/retry"); await refreshAll() }
    func stopError(_ path: String) async { try? await client.post("/api/errors/\(path)/stop"); await refreshAll() }
    func clearError(_ path: String) async { try? await client.post("/api/errors/\(path)/clear"); await refreshAll() }
}

// 连接持久化（UserDefaults）
struct ConnectionStore {
    private let key = "msm_conn"
    func load() -> SavedConnection? {
        guard let d = UserDefaults.standard.data(forKey: key) else { return nil }
        return try? JSONDecoder().decode(SavedConnection.self, from: d)
    }
    func save(_ c: SavedConnection) {
        if let d = try? JSONEncoder().encode(c) { UserDefaults.standard.set(d, forKey: key) }
    }
    func clear() { UserDefaults.standard.removeObject(forKey: key) }
}
