package com.msm.app.data

import com.google.gson.JsonObject
import okhttp3.OkHttpClient
import okhttp3.RequestBody
import retrofit2.Response
import retrofit2.http.*
import java.security.SecureRandom
import java.security.cert.X509Certificate
import javax.net.ssl.SSLContext
import javax.net.ssl.X509TrustManager

/** 已鉴权的 MSM WebUI API */
interface MsmApi {

    @GET("/api/state")
    suspend fun state(): Response<JsonObject>

    @GET("/api/system")
    suspend fun system(): Response<SystemInfo>

    @GET("/api/servers")
    suspend fun servers(): Response<List<ServerSummary>>

    @GET("/api/servers/{name}")
    suspend fun server(@Path("name") name: String): Response<ServerDetail>

    @POST("/api/servers/{name}/start")
    suspend fun startServer(@Path("name") name: String): Response<JsonObject>

    @POST("/api/servers/{name}/stop")
    suspend fun stopServer(@Path("name") name: String): Response<JsonObject>

    @POST("/api/servers/{name}/command")
    @Headers("Content-Type: application/json")
    suspend fun sendCommand(
        @Path("name") name: String,
        @Body body: RequestBody
    ): Response<JsonObject>

    @GET("/api/servers/{name}/console")
    suspend fun console(@Path("name") name: String): Response<JsonObject>

    @GET("/api/servers/{name}/players")
    suspend fun players(@Path("name") name: String): Response<JsonObject>

    @GET("/api/errors")
    suspend fun errors(): Response<List<ErrorRecord>>

    @POST("/api/errors/{path}/retry")
    suspend fun retryError(@Path("path") path: String): Response<JsonObject>

    @POST("/api/errors/{path}/stop")
    suspend fun stopError(@Path("path") path: String): Response<JsonObject>

    @POST("/api/errors/{path}/clear")
    suspend fun clearError(@Path("path") path: String): Response<JsonObject>

    @GET("/api/proxies")
    suspend fun proxies(): Response<JsonObject>

    @GET("/api/servers/{name}/watchdog")
    suspend fun watchdog(@Path("name") name: String): Response<JsonObject>

    @POST("/api/servers/{name}/proxy")
    @Headers("Content-Type: application/json")
    suspend fun setProxy(@Path("name") name: String, @Body body: RequestBody): Response<JsonObject>

    @GET("/api/servers/{name}/optimods")
    suspend fun optMods(
        @Path("name") name: String,
        @Query("mc") mc: String,
        @Query("loader") loader: String
    ): Response<JsonObject>

    @POST("/api/servers/{name}/installoptimod")
    @Headers("Content-Type: application/json")
    suspend fun installOptMod(
        @Path("name") name: String,
        @Body body: RequestBody
    ): Response<JsonObject>
}

/** 免令牌的配对接口（手机尚未持有 token 时使用） */
interface MsmPairApi {
    @GET("/api/paircode")
    suspend fun pairCode(): Response<PairInfo>

    @POST("/api/pair")
    @Headers("Content-Type: application/json")
    suspend fun pair(@Body body: RequestBody): Response<PairResult>
}

object ApiFactory {

    /** 信任所有证书（用于自签 HTTPS 的 LAN 管理面板）。仅限本应用内使用。 */
    private fun unsafeOkHttp(): OkHttpClient {
        val trustAll = object : X509TrustManager {
            override fun checkClientTrusted(chain: Array<out X509Certificate>?, authType: String?) {}
            override fun checkServerTrusted(chain: Array<out X509Certificate>?, authType: String?) {}
            override fun getAcceptedIssuers(): Array<X509Certificate> = arrayOf()
        }
        val ssl = SSLContext.getInstance("TLS").apply {
            init(null, arrayOf(trustAll), SecureRandom())
        }
        return OkHttpClient.Builder()
            .hostnameVerifier { _, _ -> true }
            .sslSocketFactory(ssl.socketFactory, trustAll)
            .build()
    }

    fun create(baseUrl: String, token: String): MsmApi {
        val client = unsafeOkHttp().newBuilder()
            .addInterceptor { chain ->
                val req = chain.request().newBuilder()
                    .addHeader("Authorization", "Bearer $token")
                    .build()
                chain.proceed(req)
            }
            .build()
        return retrofit2.Retrofit.Builder()
            .baseUrl(baseUrl)
            .client(client)
            .addConverterFactory(
                retrofit2.converter.gson.GsonConverterFactory.create()
            )
            .build()
            .create(MsmApi::class.java)
    }

    fun createPair(baseUrl: String): MsmPairApi {
        return retrofit2.Retrofit.Builder()
            .baseUrl(baseUrl)
            .client(unsafeOkHttp())
            .addConverterFactory(
                retrofit2.converter.gson.GsonConverterFactory.create()
            )
            .build()
            .create(MsmPairApi::class.java)
    }
}
