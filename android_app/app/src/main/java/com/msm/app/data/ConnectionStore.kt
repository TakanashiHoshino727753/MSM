package com.msm.app.data

import android.content.Context
import android.content.SharedPreferences

data class SavedConnection(
    val host: String,
    val port: Int,
    val useHttps: Boolean,
    val token: String
) {
    val baseUrl: String get() = "${if (useHttps) "https" else "http"}://$host:$port"
}

/** 简单的连接配置持久化（SharedPreferences）。 */
class ConnectionStore(context: Context) {

    private val prefs: SharedPreferences =
        context.getSharedPreferences("msm_conn", Context.MODE_PRIVATE)

    fun load(): SavedConnection? {
        val host = prefs.getString("host", null) ?: return null
        val port = prefs.getInt("port", 0)
        if (port == 0) return null
        val useHttps = prefs.getBoolean("https", false)
        val token = prefs.getString("token", "") ?: ""
        return SavedConnection(host, port, useHttps, token)
    }

    fun save(c: SavedConnection) {
        prefs.edit().apply {
            putString("host", c.host)
            putInt("port", c.port)
            putBoolean("https", c.useHttps)
            putString("token", c.token)
            apply()
        }
    }

    fun clear() {
        prefs.edit().clear().apply()
    }
}
