package com.msm.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.*
import androidx.lifecycle.viewmodel.compose.viewModel
import com.msm.app.ui.*

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // 处理 msm:// 深链（从其他应用扫码跳转）
        val deepLink = intent?.data?.toString()
        setContent {
            val vm: MainViewModel = viewModel()
            val state by vm.state.collectAsState()
            var screen by remember { mutableStateOf("list") } // list | detail | errors | scanner
            var pendingUri by remember { mutableStateOf<String?>(null) }

            LaunchedEffect(deepLink) {
                deepLink?.let {
                    if (it.startsWith("msm://")) {
                        vm.connectWithUri(it)
                        pendingUri = null
                    }
                }
            }

            MaterialTheme {
                Surface {
                    when {
                        !state.connected -> ConnectScreen(
                            connecting = state.connecting,
                            error = state.error,
                            onConnect = { h, p, s, t -> vm.connect(com.msm.app.data.SavedConnection(h, p, s, t)) },
                            onPair = { h, p, s, c -> vm.connectWithPair(h, p, s, c) },
                            onScan = { screen = "scanner" },
                            onUri = { vm.connectWithUri(it) }
                        )
                        screen == "scanner" -> QrScannerScreen(
                            onResult = {
                                if (it.startsWith("msm://")) vm.connectWithUri(it)
                                screen = "list"
                            },
                            onCancel = { screen = "list" }
                        )
                        state.selected != null && screen == "detail" -> ServerDetailScreen(
                            detail = state.selected!!,
                            console = state.console,
                            players = state.players,
                            onBack = { vm.clearSelection(); screen = "list" },
                            onStart = { vm.start(state.selected!!.name) },
                            onStop = { vm.stop(state.selected!!.name) },
                            onRefresh = { vm.selectServer(state.selected!!.name) },
                            onCommand = { vm.sendCommand(state.selected!!.name, it) }
                        )
                        screen == "errors" -> ErrorScreen(
                            errors = state.errors,
                            onBack = { screen = "list" },
                            onRetry = { vm.retryError(it) },
                            onStop = { vm.stopError(it) },
                            onClear = { vm.clearError(it) },
                            onRefresh = { vm.refreshAll() }
                        )
                        else -> ServerListScreen(
                            servers = state.servers,
                            errorCount = state.errors.size,
                            onOpen = { vm.selectServer(it); screen = "detail" },
                            onErrors = { screen = "errors" },
                            onRefresh = { vm.refreshAll() },
                            onDisconnect = { vm.disconnect() }
                        )
                    }
                }
            }
        }
    }
}
