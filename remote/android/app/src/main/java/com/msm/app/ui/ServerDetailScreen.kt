package com.msm.app.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.msm.app.data.ServerDetail

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ServerDetailScreen(
    detail: ServerDetail,
    console: String,
    players: List<String>,
    onBack: () -> Unit,
    onStart: () -> Unit,
    onStop: () -> Unit,
    onRefresh: () -> Unit,
    onCommand: (String) -> Unit,
    onProxy: () -> Unit,
    onOptMods: () -> Unit,
    onWatchdog: () -> Unit
) {
    var cmd by remember { mutableStateOf("") }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(detail.name) },
                navigationIcon = {
                    IconButton(onClick = onBack) { Text("←") }
                },
                actions = {
                    IconButton(onClick = onRefresh) { Text("⟳") }
                }
            )
        }
    ) { pad ->
        Column(
            modifier = Modifier.padding(pad).padding(12.dp).fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                if (detail.running)
                    Button(onClick = onStop, colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error)) { Text("停止") }
                else
                    Button(onClick = onStart) { Text("启动") }
                Text(
                    if (detail.running) "● 运行中" else "○ 已停止",
                    modifier = Modifier.alignByBaseline()
                )
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = onProxy) { Text("代理") }
                OutlinedButton(onClick = onOptMods) { Text("优化模组") }
                OutlinedButton(onClick = onWatchdog) { Text("看门狗") }
            }
            Text("版本 ${detail.mcVersion} · ${detail.loader} · 端口 ${detail.port}", style = MaterialTheme.typography.bodySmall)
            Text("玩家 ${detail.players}/${detail.maxPlayers} · ${detail.javaInfo}", style = MaterialTheme.typography.bodySmall)

            Text("控制台", style = MaterialTheme.typography.titleSmall)
            LazyColumn(
                modifier = Modifier.fillMaxWidth().weight(1f).padding(8.dp),
                reverseLayout = true
            ) {
                item { Text(console, style = MaterialTheme.typography.bodySmall, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace) }
            }

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    value = cmd, onValueChange = { cmd = it },
                    label = { Text("输入指令 (如 list)") },
                    modifier = Modifier.weight(1f),
                    singleLine = true
                )
                Button(onClick = { onCommand(cmd); cmd = "" }, enabled = detail.running) { Text("发送") }
            }

            if (players.isNotEmpty()) {
                Text("在线玩家", style = MaterialTheme.typography.titleSmall)
                players.forEach { Text("• $it") }
            }
        }
    }
}
