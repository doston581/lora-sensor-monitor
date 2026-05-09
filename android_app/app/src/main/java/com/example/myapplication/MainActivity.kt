package com.example.myapplication

import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.TextView
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.google.android.material.card.MaterialCardView
import com.google.gson.Gson
import okhttp3.*
import java.io.IOException
import java.util.Timer
import kotlin.concurrent.timerTask

data class SensorData(val temperature: Double, val humidity: Double)

class MainActivity : AppCompatActivity() {
    private val client = OkHttpClient()
    private val gson = Gson()
    private var timer: Timer? = null
    private val handler = Handler(Looper.getMainLooper())
    private var isOnline = false

    private lateinit var tvStatus: TextView
    private lateinit var tvTemperature: TextView
    private lateinit var tvHumidity: TextView
    private lateinit var cardTemperature: MaterialCardView

    private val timeoutRunnable = Runnable {
        setOfflineState()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_main)
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        tvStatus = findViewById(R.id.tv_status)
        tvTemperature = findViewById(R.id.tv_temperature)
        tvHumidity = findViewById(R.id.tv_humidity)
        cardTemperature = findViewById(R.id.card_temperature)

        setOfflineState()
        startDataFetching()
    }

    private fun startDataFetching() {
        timer = Timer()
        timer?.schedule(timerTask {
            fetchData()
        }, 0, 2000)
    }

    private fun fetchData() {
        val request = Request.Builder()
            .url("http://10.59.9.94:5000/api/latest_data")
            .build()

        client.newCall(request).enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) {
                // Failure is handled by timeout logic
                e.printStackTrace()
            }

            override fun onResponse(call: Call, response: Response) {
                response.body?.string()?.let { responseBody ->
                    try {
                        val data = gson.fromJson(responseBody, SensorData::class.java)
                        runOnUiThread {
                            setOnlineState(data)
                        }
                    } catch (e: Exception) {
                        e.printStackTrace()
                    }
                }
            }
        })
    }

    private fun setOnlineState(data: SensorData) {
        isOnline = true
        tvStatus.text = "设备状态：在线 (数据刷新中)"
        tvStatus.setTextColor(Color.parseColor("#4CAF50"))

        tvTemperature.text = "${data.temperature} °C"
        tvHumidity.text = "${data.humidity} %"

        if (data.temperature > 30.0) {
            cardTemperature.setCardBackgroundColor(Color.parseColor("#FFCDD2")) // Light red alert
            tvTemperature.setTextColor(Color.parseColor("#D32F2F")) // Dark red text
        } else {
            cardTemperature.setCardBackgroundColor(Color.parseColor("#FFFFFF"))
            tvTemperature.setTextColor(Color.parseColor("#333333"))
        }

        handler.removeCallbacks(timeoutRunnable)
        handler.postDelayed(timeoutRunnable, 5000)
    }

    private fun setOfflineState() {
        isOnline = false
        tvStatus.text = "设备状态：离线"
        tvStatus.setTextColor(Color.parseColor("#F44336")) // Red

        tvTemperature.text = "--"
        tvHumidity.text = "--"

        cardTemperature.setCardBackgroundColor(Color.parseColor("#FFFFFF"))
        tvTemperature.setTextColor(Color.parseColor("#333333"))
    }

    override fun onDestroy() {
        super.onDestroy()
        timer?.cancel()
        handler.removeCallbacks(timeoutRunnable)
    }
}