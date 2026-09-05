package com.msm.app.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.msm.app.data.ServerSummary

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ServerListScreen(
    servers: List<ServerSummary>,
    errorCount: Int,
    onOpen: (String) -> Unit,
    onErrors: () -> Unit,
    onRefresh: () -> Unit,
    onDisconnect: () -> Unit
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("服务器列表") },
                actions = {
                    if (errorCount > 0)
                        AssistChip(onClick = onErrors, label = { Text("异常 $errorCount") })
                    IconButton(onClick = onRefresh) { Text("⟳") }
                    IconButton(onClick = onDisconnect) { Text("⏻") }
                }
            )
        },
        floatingActionButton = {
            FloatingActionButton(onClick = onRefresh) { Text("⟳") }
        }
    ) { pad ->
        if (servers.isEmpty()) {
            Box(Modifier.padding(pad).fillMaxSize(), contentAlignment = androidx.compose.ui.Alignment.Center) {
                Text("暂无服务器")
            }
        } else {
            LazyColumn(
                modifier = Modifier.padding(pad).fillMaxSize(),
                contentPadding = PaddingValues(12.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(servers) { s ->
                    ServerCard(s) { onOpen(s.name) }
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ServerCard(s: ServerSummary, onClick: () -> Unit) {
    Card(onClick = onClick, modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.padding(12.dp).fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Column {
                Text(s.name, style = MaterialTheme.typography.titleMedium)
                Text(
                    "${s.type} · ${s.mcVersion} · ${s.loader}".trimEnd(' ', '·'),
                    style = MaterialTheme.typography.bodySmall
                )
                Text(
                    "玩家 ${s.players}/${s.maxPlayers} · 端口 ${s.port}",
                    style = MaterialTheme.typography.bodySmall
                )
            }
            AssistChip(
                onClick = {},
                label = { Text(if (s.running) "运行中" else "已停止") },
                colors = AssistChipDefaults.assistChipColors(
                    containerColor = if (s.running)
                        androidx.compose.material3.MaterialTheme.colorScheme.primaryContainer
                    else
                        androidx.compose.material3.MaterialTheme.colorScheme.errorContainer
                )
            )
        }
    }
}
