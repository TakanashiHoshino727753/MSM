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
fun OptModScreen(
    mcVersion: String,
    loader: String,
    optMods: JsonObject?,
    loading: Boolean,
    onBack: () -> Unit,
    onRefresh: () -> Unit,
    onInstall: (modId: String, version: String) -> Unit
) {
    val mods = remember(optMods) {
        optMods?.getAsJsonArray("mods")?.mapNotNull { it as? JsonObject } ?: emptyList()
    }
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("优化模组") },
                navigationIcon = { IconButton(onClick = onBack) { Text("←") } },
                actions = { IconButton(onClick = onRefresh) { Text("⟳") } }
            )
        }
    ) { pad ->
        Column(Modifier.padding(pad).fillMaxSize()) {
            Text(
                "MC $mcVersion · $loader",
                style = MaterialTheme.typography.bodySmall,
                modifier = Modifier.padding(12.dp, 8.dp, 12.dp, 0.dp)
            )
            if (loading) {
                LinearProgressIndicator(Modifier.fillMaxWidth().padding(12.dp))
                Text("检索中…", style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(12.dp))
            }
            if (mods.isEmpty() && !loading) {
                Box(Modifier.fillMaxSize(), contentAlignment = androidx.compose.ui.Alignment.Center) { Text("暂无优化模组") }
            } else {
                LazyColumn(
                    modifier = Modifier.fillMaxSize().weight(1f),
                    contentPadding = PaddingValues(12.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    items(mods) { m -> OptModCard(m, onInstall) }
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun OptModCard(m: JsonObject, onInstall: (String, String) -> Unit) {
    val id = m.get("id")?.asString ?: m.get("modId")?.asString ?: ""
    val name = m.get("name")?.asString ?: id
    val version = m.get("version")?.asString ?: m.get("latestVersion")?.asString ?: "latest"
    val desc = m.get("description")?.asString ?: ""
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(12.dp)) {
            Text(name, style = MaterialTheme.typography.titleMedium)
            if (desc.isNotBlank()) Text(desc, style = MaterialTheme.typography.bodySmall)
            Text("版本 $version", style = MaterialTheme.typography.bodySmall)
            Button(
                onClick = { onInstall(id, version) },
                modifier = Modifier.align(androidx.compose.ui.Alignment.End)
            ) { Text("安装") }
        }
    }
}
