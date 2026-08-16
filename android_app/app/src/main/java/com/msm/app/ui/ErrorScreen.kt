package com.msm.app.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.msm.app.data.ErrorRecord

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ErrorScreen(
    errors: List<ErrorRecord>,
    onBack: () -> Unit,
    onRetry: (String) -> Unit,
    onStop: (String) -> Unit,
    onClear: (String) -> Unit,
    onRefresh: () -> Unit
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("异常纠错") },
                navigationIcon = { IconButton(onClick = onBack) { Text("←") } },
                actions = { IconButton(onClick = onRefresh) { Text("⟳") } }
            )
        }
    ) { pad ->
        if (errors.isEmpty()) {
            Box(Modifier.padding(pad).fillMaxSize(), contentAlignment = androidx.compose.ui.Alignment.Center) {
                Text("运行正常 ✓")
            }
        } else {
            LazyColumn(
                modifier = Modifier.padding(pad).fillMaxSize(),
                contentPadding = PaddingValues(12.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(errors) { e ->
                    ErrorCard(e, onRetry, onStop, onClear)
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ErrorCard(
    e: ErrorRecord,
    onRetry: (String) -> Unit,
    onStop: (String) -> Unit,
    onClear: (String) -> Unit
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(12.dp)) {
            Row(
                horizontalArrangement = Arrangement.SpaceBetween,
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(e.name, style = MaterialTheme.typography.titleMedium)
                AssistChip(onClick = {}, label = { Text(e.typeLabel) })
            }
            Text("时间: ${e.time}", style = MaterialTheme.typography.bodySmall)
            if (e.retrying)
                Text("重试中… 第 ${e.retryCount}/${e.maxRetries} 次，约 ${e.nextRetryInSec}s 后", style = MaterialTheme.typography.bodySmall)
            if (e.fatal)
                Text("致命错误，已停止自动重试", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error)

            Text(e.logTail, style = MaterialTheme.typography.bodySmall, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace)

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                if (!e.fatal) {
                    Button(onClick = { onRetry(e.path) }) { Text("现在重试") }
                    OutlinedButton(onClick = { onStop(e.path) }) { Text("停止重试") }
                }
                OutlinedButton(onClick = { onClear(e.path) }) { Text("标记已解决") }
            }
        }
    }
}
