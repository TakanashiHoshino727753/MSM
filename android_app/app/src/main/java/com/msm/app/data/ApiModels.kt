package com.msm.app.data

import com.google.gson.JsonObject
import com.google.gson.annotations.SerializedName

/** 服务器摘要（GET /api/servers 列表项） */
data class ServerSummary(
    @SerializedName("name") val name: String = "",
    @SerializedName("type") val type: String = "",
    @SerializedName("running") val running: Boolean = false,
    @SerializedName("version") val version: String = "",
    @SerializedName("port") val port: Int = 0,
    @SerializedName("players") val players: Int = 0,
    @SerializedName("maxPlayers") val maxPlayers: Int = 0,
    @SerializedName("mcVersion") val mcVersion: String = "",
    @SerializedName("loader") val loader: String = "",
    @SerializedName("path") val path: String = "",
    @SerializedName("hasError") val hasError: Boolean = false
)

/** 服务器详情（GET /api/servers/{name}） */
data class ServerDetail(
    @SerializedName("name") val name: String = "",
    @SerializedName("type") val type: String = "",
    @SerializedName("running") val running: Boolean = false,
    @SerializedName("version") val version: String = "",
    @SerializedName("mcVersion") val mcVersion: String = "",
    @SerializedName("loader") val loader: String = "",
    @SerializedName("port") val port: Int = 0,
    @SerializedName("path") val path: String = "",
    @SerializedName("players") val players: Int = 0,
    @SerializedName("maxPlayers") val maxPlayers: Int = 0,
    @SerializedName("eulaAccepted") val eulaAccepted: Boolean = false,
    @SerializedName("console") val console: String = "",
    @SerializedName("playerList") val playerList: List<String> = emptyList(),
    @SerializedName("properties") val properties: JsonObject = JsonObject(),
    @SerializedName("mods") val mods: JsonObject = JsonObject(),
    @SerializedName("javaInfo") val javaInfo: String = "",
    @SerializedName("proxy") val proxy: JsonObject? = null,
    @SerializedName("watchdog") val watchdog: JsonObject? = null
)

/** 异常记录（GET /api/errors） */
data class ErrorRecord(
    @SerializedName("name") val name: String = "",
    @SerializedName("path") val path: String = "",
    @SerializedName("type") val type: String = "",
    @SerializedName("typeLabel") val typeLabel: String = "",
    @SerializedName("time") val time: String = "",
    @SerializedName("logTail") val logTail: String = "",
    @SerializedName("retrying") val retrying: Boolean = false,
    @SerializedName("retryCount") val retryCount: Int = 0,
    @SerializedName("maxRetries") val maxRetries: Int = 0,
    @SerializedName("nextRetryInSec") val nextRetryInSec: Int = 0,
    @SerializedName("fatal") val fatal: Boolean = false,
    @SerializedName("autoRestart") val autoRestart: Boolean = false
)

/** 配对码信息（GET /api/paircode） */
data class PairInfo(
    @SerializedName("code") val code: String = "",
    @SerializedName("uri") val uri: String = "",
    @SerializedName("remainSec") val remainSec: Int = 0,
    @SerializedName("used") val used: Boolean = false
)

/** 配对换取到的令牌（POST /api/pair） */
data class PairResult(
    @SerializedName("token") val token: String = "",
    @SerializedName("port") val port: Int = 0,
    @SerializedName("https") val https: Boolean = false
)

/** 系统信息（GET /api/system） */
data class SystemInfo(
    @SerializedName("hostname") val hostname: String = "",
    @SerializedName("os") val os: String = "",
    @SerializedName("arch") val arch: String = "",
    @SerializedName("cpuModel") val cpuModel: String = "",
    @SerializedName("cpuCores") val cpuCores: Int = 0,
    @SerializedName("totalMemMB") val totalMemMB: Long = 0,
    @SerializedName("freeMemMB") val freeMemMB: Long = 0,
    @SerializedName("javaAvailable") val javaAvailable: Boolean = false,
    @SerializedName("proxyCount") val proxyCount: Int = 0,
    @SerializedName("botLinked") val botLinked: Boolean = false
)
