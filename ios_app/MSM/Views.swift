import SwiftUI

struct ContentView: View {
    @StateObject var vm = AppViewModel()
    @State private var screen: Screen = .list
    @State private var showingScanner = false

    enum Screen { case list, detail, errors }

    var body: some View {
        Group {
            if !vm.connected {
                ConnectView(vm: vm, onScan: { showingScanner = true })
            } else if let d = vm.detail, screen == .detail {
                ServerDetailView(vm: vm, detail: d)
            } else if screen == .errors {
                ErrorView(vm: vm)
            } else {
                ServerListView(vm: vm)
            }
        }
        .sheet(isPresented: $showingScanner) {
            // 实际项目接入 CodeScanner / AVFoundation 扫码，结果回调 vm.connectWithUri
            Text("扫码功能：在 Xcode 工程中接入 CodeScanner 或 AVFoundation，\n扫描 msm:// URI 后调用 vm.connectWithUri")
                .padding()
        }
        .task { if vm.connected { await vm.refreshAll() } }
    }
}

struct ConnectView: View {
    @ObservedObject var vm: AppViewModel
    @State private var host = ""
    @State private var port = "25580"
    @State private var https = false
    @State private var token = ""
    @State private var code = ""
    @State private var uri = ""
    @State private var mode: Int = 0

    var body: some View {
        NavigationStack {
            Form {
                Picker("方式", selection: $mode) {
                    Text("令牌").tag(0); Text("配对码").tag(1); Text("URI").tag(2)
                }.pickerStyle(.segmented)

                TextField("主机 / IP", text: $host)
                HStack {
                    TextField("端口", text: $port).keyboardType(.numberPad)
                    Toggle("HTTPS", isOn: $https)
                }

                if mode == 0 {
                    SecureField("访问令牌", text: $token)
                    Button("连接") { Task { await vm.connect(SavedConnection(host: host, port: Int(port) ?? 25580, useHttps: https, token: token)) } }
                        .disabled(host.isEmpty)
                } else if mode == 1 {
                    TextField("配对码 (ABC-1234)", text: $code)
                    Button("用配对码连接") { Task { await vm.connectWithPair(host: host, port: Int(port) ?? 25580, useHttps: https, code: code) } }
                        .disabled(host.isEmpty || code.isEmpty)
                    Text("配对码在桌面端「移动端配对」页面获取，10 分钟内有效。").font(.caption)
                } else {
                    TextField("msm://token@host:port?t=...", text: $uri)
                    Button("用 URI 连接") { vm.connectWithUri(uri) }.disabled(!uri.hasPrefix("msm://"))
                }

                if vm.connecting { ProgressView() }
                if let e = vm.errorMsg { Text(e).foregroundColor(.red).font(.caption) }
            }
            .navigationTitle("连接 MSM 服务器")
        }
    }
}

struct ServerListView: View {
    @ObservedObject var vm: AppViewModel
    var body: some View {
        NavigationStack {
            List(vm.servers) { s in
                Button {
                    Task { await vm.selectServer(s.name); }
                } label: {
                    VStack(alignment: .leading) {
                        Text(s.name).font(.headline)
                        Text("\(s.type) · \(s.mcVersion) · \(s.loader)").font(.caption)
                        Text("玩家 \(s.players)/\(s.maxPlayers) · 端口 \(s.port)").font(.caption)
                    }
                }
            }
            .navigationTitle("服务器列表")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    if vm.errors.count > 0 {
                        Button("异常 \(vm.errors.count)") {}
                    }
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button("⟳") { Task { await vm.refreshAll() } }
                }
            }
            .refreshable { await vm.refreshAll() }
        }
    }
}

struct ServerDetailView: View {
    @ObservedObject var vm: AppViewModel
    let detail: ServerDetail
    @State private var cmd = ""

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    if detail.running {
                        Button("停止") { Task { await vm.stop(detail.name) } }.foregroundColor(.red)
                    } else {
                        Button("启动") { Task { await vm.start(detail.name) } }
                    }
                    Text(detail.running ? "● 运行中" : "○ 已停止")
                }
                Text("版本 \(detail.mcVersion) · \(detail.loader) · 端口 \(detail.port)").font(.caption)
                Text("玩家 \(detail.players)/\(detail.maxPlayers) · \(detail.javaInfo)").font(.caption)

                Text("控制台").font(.headline)
                Text(vm.consoleText).font(.system(.caption, design: .monospace))

                HStack {
                    TextField("指令 (list)", text: $cmd)
                    Button("发送") { Task { await vm.sendCommand(detail.name, cmd); cmd = "" } }.disabled(!detail.running)
                }

                if !vm.players.isEmpty {
                    Text("在线玩家").font(.headline)
                    ForEach(vm.players, id: \.self) { p in Text("• \(p)") }
                }
            }.padding()
        }
        .navigationTitle(detail.name)
    }
}

struct ErrorView: View {
    @ObservedObject var vm: AppViewModel
    var body: some View {
        NavigationStack {
            if vm.errors.isEmpty {
                Text("运行正常 ✓").padding()
            } else {
                List(vm.errors) { e in
                    VStack(alignment: .leading) {
                        Text(e.name).font(.headline)
                        Text(e.typeLabel).font(.caption)
                        Text(e.time).font(.caption)
                        if e.retrying { Text("重试中… 第 \(e.retryCount)/\(e.maxRetries) 次").font(.caption) }
                        Text(e.logTail).font(.system(.caption, design: .monospace))
                        HStack {
                            if !e.fatal {
                                Button("现在重试") { Task { await vm.retryError(e.path) } }
                                Button("停止重试") { Task { await vm.stopError(e.path) } }
                            }
                            Button("标记已解决") { Task { await vm.clearError(e.path) } }
                        }
                    }
                }
                .refreshable { await vm.refreshAll() }
            }
        }.navigationTitle("异常纠错")
    }
}
