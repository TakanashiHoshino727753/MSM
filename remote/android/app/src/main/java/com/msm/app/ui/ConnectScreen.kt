package com.msm.app.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConnectScreen(
    connecting: Boolean,
    error: String?,
    onConnect: (host: String, port: Int, https: Boolean, token: String) -> Unit,
    onPair: (host: String, port: Int, https: Boolean, code: String) -> Unit,
    onScan: () -> Unit,
    onUri: (uri: String) -> Unit
) {
    var host by remember { mutableStateOf("") }
    var port by remember { mutableStateOf("25580") }
    var https by remember { mutableStateOf(false) }
    var token by remember { mutableStateOf("") }
    var code by remember { mutableStateOf("") }
    var uri by remember { mutableStateOf("") }
    var mode by remember { mutableStateOf(0) } // 0 令牌, 1 配对码, 2 URI

    Scaffold(
        topBar = { TopAppBar(title = { Text("连接 MSM 服务器") }) }
    ) { pad ->
        Column(
            modifier = Modifier
                .padding(pad)
                .padding(16.dp)
                .fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            TabRow(selectedTabIndex = mode) {
                Tab(selected = mode == 0, onClick = { mode = 0 }) { Text("令牌", Modifier.padding(8.dp)) }
                Tab(selected = mode == 1, onClick = { mode = 1 }) { Text("配对码", Modifier.padding(8.dp)) }
                Tab(selected = mode == 2, onClick = { mode = 2 }) { Text("URI", Modifier.padding(8.dp)) }
            }

            OutlinedTextField(
                value = host, onValueChange = { host = it },
                label = { Text("主机 / IP") }, modifier = Modifier.fillMaxWidth()
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    value = port, onValueChange = { port = it },
                    label = { Text("端口") }, keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    modifier = Modifier.weight(1f)
                )
                Row(verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
                    Checkbox(checked = https, onCheckedChange = { https = it })
                    Text("HTTPS")
                }
            }

            if (mode == 0) {
                OutlinedTextField(
                    value = token, onValueChange = { token = it },
                    label = { Text("访问令牌 (webuiToken)") }, modifier = Modifier.fillMaxWidth()
                )
                Button(
                    onClick = { onConnect(host, port.toIntOrNull() ?: 25580, https, token) },
                    enabled = !connecting && host.isNotBlank(),
                    modifier = Modifier.fillMaxWidth()
                ) { Text("连接") }
            }

            if (mode == 1) {
                OutlinedTextField(
                    value = code, onValueChange = { code = it },
                    label = { Text("配对码 (如 ABC-1234)") }, modifier = Modifier.fillMaxWidth()
                )
                Button(
                    onClick = {
                        onScan()
                    },
                    modifier = Modifier.fillMaxWidth()
                ) { Text("📷 扫码获取") }
                Button(
                    onClick = { onPair(host, port.toIntOrNull() ?: 25580, https, code) },
                    enabled = !connecting && host.isNotBlank() && code.isNotBlank(),
                    modifier = Modifier.fillMaxWidth()
                ) { Text("用配对码连接") }
                Text("提示：配对码在桌面端「移动端配对」页面获取，10 分钟内有效。", style = MaterialTheme.typography.bodySmall)
            }

            if (mode == 2) {
                OutlinedTextField(
                    value = uri, onValueChange = { uri = it },
                    label = { Text("msm://token@host:port?t=...") }, modifier = Modifier.fillMaxWidth()
                )
                Button(
                    onClick = { onUri(uri) },
                    enabled = !connecting && uri.startsWith("msm://"),
                    modifier = Modifier.fillMaxWidth()
                ) { Text("用 URI 连接") }
                Button(onClick = { onScan() }, modifier = Modifier.fillMaxWidth()) { Text("📷 扫码") }
            }

            if (connecting) CircularProgressIndicator()
            error?.let {
                Text(it, color = MaterialTheme.colorScheme.error)
            }
        }
    }
}
