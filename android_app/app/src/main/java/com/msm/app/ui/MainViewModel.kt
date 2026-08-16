package com.msm.app.ui

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.google.gson.JsonObject
import com.msm.app.data.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody

data class UiState(
    val connected: Boolean = false,
    val connecting: Boolean = false,
    val error: String? = null,
    val system: SystemInfo? = null,
    val servers: List<ServerSummary> = emptyList(),
    val errors: List<ErrorRecord> = emptyList(),
    val selected: ServerDetail? = null,
    val console: String = "",
    val players: List<String> = emptyList()
)

class MainViewModel(app: Application) : AndroidViewModel(app) {

    private val store = ConnectionStore(app)
    private var api: MsmApi? = null

    private val _state = MutableStateFlow(UiState())
    val state: StateFlow<UiState> get() = _state

    init {
        store.load()?.let { connect(it) }
    }

    /** 用已保存/构造的连接信息建立会话。 */
    fun connect(c: SavedConnection) {
        viewModelScope.launch {
            _state.value = _state.value.copy(connecting = true, error = null)
            try {
                val created = withContext(Dispatchers.IO) {
                    ApiFactory.create(c.baseUrl, c.token)
                }
                api = created
                store.save(c)
                // 验证连通性
                val sys = created.system()
                if (!sys.isSuccessful) throw Exception("鉴权失败 (${sys.code()})")
                _state.value = _state.value.copy(
                    connected = true, connecting = false, system = sys.body()
                )
                refreshAll()
            } catch (e: Exception) {
                _state.value = _state.value.copy(
                    connected = false, connecting = false,
                    error = e.message ?: "连接失败"
                )
                api = null
            }
        }
    }

    /** 通过配对码换取 token 后连接。 */
    fun connectWithPair(host: String, port: Int, useHttps: Boolean, code: String) {
        val baseUrl = "${if (useHttps) "https" else "http"}://$host:$port"
        viewModelScope.launch {
            _state.value = _state.value.copy(connecting = true, error = null)
            try {
                val pairApi = withContext(Dispatchers.IO) { ApiFactory.createPair(baseUrl) }
                val body = JsonObject().apply { addProperty("code", code.uppercase()) }
                    .toString().toRequestBody("application/json".toMediaType())
                val resp = pairApi.pair(body)
                if (!resp.isSuccessful || resp.body() == null)
                    throw Exception("配对失败 (${resp.code()})")
                val r = resp.body()!!
                connect(SavedConnection(host, r.port, r.https, r.token))
            } catch (e: Exception) {
                _state.value = _state.value.copy(
                    connecting = false, error = e.message ?: "配对失败"
                )
            }
        }
    }

    /** 解析 msm://token@host:port?t=TOKEN 深链 URI。 */
    fun connectWithUri(uri: String) {
        // 形如 msm://token@192.168.1.5:25580?t=ABC123...
        val noScheme = uri.removePrefix("msm://")
        val useHttps = false
        val (authHost, query) = noScheme.split("?", limit = 2) + listOf("")
        val token = Regex("t=([^&]+)").find("?$query")?.groupValues?.get(1) ?: ""
        val (user, hostPort) = authHost.split("@", limit = 2) + listOf("")
        val (host, portStr) = (hostPort.ifBlank { user }).split(":", limit = 2) + listOf("")
        val port = portStr.toIntOrNull() ?: 25580
        connect(SavedConnection(host, port, useHttps, token))
    }

    fun disconnect() {
        store.clear()
        api = null
        _state.value = UiState()
    }

    fun refreshAll() {
        if (api == null) return
        viewModelScope.launch {
            try {
                val s = api!!.servers()
                val e = api!!.errors()
                _state.value = _state.value.copy(
                    servers = s.body() ?: emptyList(),
                    errors = e.body() ?: emptyList()
                )
            } catch (_: Exception) {
            }
        }
    }

    fun selectServer(name: String) {
        viewModelScope.launch {
            try {
                val d = api?.server(name)?.body()
                val c = api?.console(name)?.body()
                val p = api?.players(name)?.body()
                _state.value = _state.value.copy(
                    selected = d,
                    console = c?.get("text")?.asString ?: "",
                    players = p?.getAsJsonArray("players")
                        ?.mapNotNull { it.asString } ?: emptyList()
                )
            } catch (_: Exception) {
            }
        }
    }

    fun clearSelection() {
        _state.value = _state.value.copy(selected = null, console = "", players = emptyList())
    }

    fun start(name: String) = action { api!!.startServer(name) }
    fun stop(name: String) = action { api!!.stopServer(name) }
    fun sendCommand(name: String, cmd: String) {
        viewModelScope.launch {
            try {
                val body = JsonObject().apply { addProperty("command", cmd) }
                    .toString().toRequestBody("application/json".toMediaType())
                api?.sendCommand(name, body)
                selectServer(name)
            } catch (_: Exception) {
            }
        }
    }

    fun retryError(path: String) = action { api!!.retryError(path) }
    fun stopError(path: String) = action { api!!.stopError(path) }
    fun clearError(path: String) = action { api!!.clearError(path) }

    private fun action(block: suspend () -> retrofit2.Response<JsonObject>) {
        viewModelScope.launch {
            try {
                block()
                refreshAll()
            } catch (_: Exception) {
            }
        }
    }
}
