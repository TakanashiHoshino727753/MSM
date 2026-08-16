package com.msm.app.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.google.gson.JsonObject

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun WatchdogScreen(
    watchdog: JsonObject?,
    onBack: () -> Unit,
    onRefresh: () -> Unit
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("看门狗状态") },
                navigationIcon = { IconButton(onClick = onBack) { Text("←") } },
                actions = { IconButton(onClick = onRefresh) { Text("⟳") } }
            )
        }
    ) { pad ->
        if (watchdog == null) {
            Box(Modifier.padding(pad).fillMaxSize(), contentAlignment = androidx.compose.ui.Alignment.Center) { Text("无数据") }
        } else {
            val entries = watchdog.entrySet().toList()
            LazyColumn(
                modifier = Modifier.padding(pad).fillMaxSize(),
                contentPadding = PaddingValues(12.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                items(entries) { (k, v) ->
                    Card(modifier = Modifier.fillMaxWidth()) {
                        Row(
                            Modifier.padding(12.dp).fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween
                        ) {
                            Text(k, style = MaterialTheme.typography.bodyMedium)
                            Text(v.toString(), style = MaterialTheme.typography.bodySmall)
                        }
                    }
                }
            }
        }
    }
}
