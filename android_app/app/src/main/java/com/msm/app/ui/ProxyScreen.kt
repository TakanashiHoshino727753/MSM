package com.msm.app.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.google.gson.JsonObject

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ProxyScreen(
    proxies: JsonObject?,
    onBack: () -> Unit,
    onBind: (proxyId: String?) -> Unit
) {
    // proxies 结构: { "proxies": [ {instanceId,name,proxyPort,running,servers:[...]}, ... ], "default": ... }
    val list = remember(proxies) {
        proxies?.getAsJsonArray("proxies")?.mapNotNull { it as? JsonObject } ?: emptyList()
    }
    Scaffold(
        topBar = { TopAppBar(title = { Text("代理聚合") }, navigationIcon = { IconButton(onClick = onBack) { Text("←") } }) }
    ) { pad ->
        if (list.isEmpty()) {
            Box(Modifier.padding(pad).fillMaxSize(), contentAlignment = androidx.compose.ui.Alignment.Center) {
                Text("暂无代理实例")
            }
        } else {
            LazyColumn(
                modifier = Modifier.padding(pad).fillMaxSize(),
                contentPadding = PaddingValues(12.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                item {
                    Button(onClick = { onBind(null) }, modifier = Modifier.fillMaxWidth()) { Text("解绑 / 不归属任何代理") }
                }
                items(list) { p ->
                    ProxyCard(p) { id -> onBind(id) }
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ProxyCard(p: JsonObject, onBind: (String) -> Unit) {
    val id = p.get("instanceId")?.asString ?: ""
    val name = p.get("name")?.asString ?: id
    val port = p.get("proxyPort")?.asInt ?: 0
    val running = p.get("running")?.asBoolean ?: false
    val servers = p.getAsJsonArray("servers")?.mapNotNull { it.asString } ?: emptyList()
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(12.dp)) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text(name, style = MaterialTheme.typography.titleMedium)
                AssistChip(onClick = {}, label = { Text(if (running) "运行中" else "已停止") })
            }
            Text("端口 $port", style = MaterialTheme.typography.bodySmall)
            if (servers.isNotEmpty()) Text("已绑定: ${servers.joinToString()}", style = MaterialTheme.typography.bodySmall)
            Button(onClick = { onBind(id) }, modifier = Modifier.align(androidx.compose.ui.Alignment.End)) { Text("绑定到此代理") }
        }
    }
}
