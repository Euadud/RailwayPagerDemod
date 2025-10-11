package com.example.railwaypagerdemod

import android.graphics.Color
import android.media.RingtoneManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.method.ScrollingMovementMethod
import android.view.View
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import com.google.android.material.button.MaterialButton
import com.google.android.material.textfield.TextInputEditText
import com.google.android.material.textfield.TextInputLayout
import com.google.android.material.textview.MaterialTextView

class ParsedMessage(
    val vehicleId: String,
    val routeCandidatesBytes: Array<ByteArray>,
    val latitude: String,
    val longitude: String,
    val trainNo: String,
    val speed: String,
    val mileage: String
) {
    val routeCandidates: Array<String>
        get() = routeCandidatesBytes.map { bytes ->
            try { String(bytes, charset("GB2312")) } catch (e: Exception) { "(无法解码)" }
        }.toTypedArray()
}

class MainActivity : AppCompatActivity() {

    private val handler = Handler(Looper.getMainLooper())
    private lateinit var outputText: MaterialTextView
    private lateinit var scrollView: ScrollView

    // 顶部信息 Label
    private lateinit var vehicleIdLabel: TextView
    private lateinit var routeLabel: TextView
    private lateinit var latitudeLabel: TextView
    private lateinit var longitudeLabel: TextView
    private lateinit var routeCandidatesLayout: LinearLayout

    // 地址 1234000 信息区
    private lateinit var trainNoLabel: TextView
    private lateinit var speedLabel: TextView
    private lateinit var mileageLabel: TextView

    // 记录上次消息，避免重复响铃
    private var lastLog = ""

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        supportActionBar?.hide()

        val rootLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.parseColor("#3f83e9"))
            setPadding(24, 24, 24, 24)
        }

        ViewCompat.setOnApplyWindowInsetsListener(rootLayout) { v, insets ->
            val statusBarHeight = insets.getInsets(WindowInsetsCompat.Type.statusBars()).top
            v.updatePadding(top = statusBarHeight + 16)
            insets
        }

        // 顶部信息区
        vehicleIdLabel = createInfoLabel("车号: -")
        routeLabel = createInfoLabel("线路: -", bold = true)
        latitudeLabel = createInfoLabel("纬度: -")
        longitudeLabel = createInfoLabel("经度: -")
        routeCandidatesLayout = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }

        trainNoLabel = createInfoLabel("车次号: -")
        speedLabel = createInfoLabel("速度: -")
        mileageLabel = createInfoLabel("公里标: -")

        rootLayout.addView(vehicleIdLabel)
        rootLayout.addView(routeLabel)
        rootLayout.addView(routeCandidatesLayout)
        rootLayout.addView(latitudeLabel)
        rootLayout.addView(longitudeLabel)
        rootLayout.addView(trainNoLabel)
        rootLayout.addView(speedLabel)
        rootLayout.addView(mileageLabel)

        // 输入框
        val hostEdit = TextInputEditText(this).apply {
            hint = "主机名"
            setText("127.0.0.1")
        }
        val portEdit = TextInputEditText(this).apply {
            hint = "端口号"
            setText("14423")
        }
        rootLayout.addView(TextInputLayout(this).apply { addView(hostEdit) })
        rootLayout.addView(TextInputLayout(this).apply { addView(portEdit) })

        // 输出文本框
        outputText = MaterialTextView(this).apply {
            textSize = 14f
            setBackgroundColor(Color.WHITE)
            setTextColor(Color.BLACK)
            movementMethod = ScrollingMovementMethod.getInstance()
            setPadding(16, 20, 16, 20)
        }
        scrollView = ScrollView(this).apply {
            addView(outputText)
            layoutParams = LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f)
        }
        rootLayout.addView(scrollView)

        // 连接按钮
        val button = MaterialButton(this).apply {
            text = "连接"
            setOnClickListener {
                val host = hostEdit.text.toString().trim()
                val port = portEdit.text.toString().trim()
                if (host.isEmpty() || port.isEmpty()) {
                    Toast.makeText(context, "请输入主机名和端口号", Toast.LENGTH_SHORT).show()
                    return@setOnClickListener
                }
                startClientAsync(host, port)
                startPolling()
            }
        }
        rootLayout.addView(button)

        setContentView(rootLayout)
    }

    private fun createInfoLabel(text: String, bold: Boolean = false): TextView {
        return TextView(this).apply {
            this.text = text
            textSize = if (bold) 18f else 16f
            setTextColor(Color.WHITE)
            setPadding(8, 4, 8, 4)
        }
    }

    private fun startPolling() {
        handler.post(object : Runnable {
            override fun run() {
                val logs = pollMessages()
                if (logs.isNotEmpty() && logs != lastLog) {
                    lastLog = logs
                    outputText.append(logs + "\n")
                    scrollView.post { scrollView.fullScroll(View.FOCUS_DOWN) }
                    parseMsgAndUpdateUI(logs)
                    playNotificationSound() // 🔔 新消息时响铃
                }
                handler.postDelayed(this, 200)
            }
        })
    }

    private fun playNotificationSound() {
        try {
            val notificationUri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION)
            val ringtone = RingtoneManager.getRingtone(applicationContext, notificationUri)
            ringtone?.play()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun parseMsgAndUpdateUI(log: String) {
        val regex = "\\[MSG\\]\\s*(.+)".toRegex()
        regex.findAll(log).forEach { match ->
            val msg = match.groupValues[1]
            if (msg.length > 10) {
                val parsed = decodeMessageNative(msg)

                // 如果有经纬度 → 1234002
                if (parsed.latitude != "0" || parsed.longitude != "0") {
                    vehicleIdLabel.text = "车号: ${parsed.vehicleId}"
                    latitudeLabel.text = "纬度: ${parsed.latitude}"
                    longitudeLabel.text = "经度: ${parsed.longitude}"

                    routeLabel.text = "线路: ${parsed.routeCandidates.firstOrNull() ?: "(无法解码)"}"
                    routeCandidatesLayout.removeAllViews()
                    if (parsed.routeCandidates.size > 1) {
                        parsed.routeCandidates.drop(1).forEachIndexed { idx, route ->
                            val tv = TextView(this).apply {
                                text = "候选${idx + 2}: $route"
                                setTextColor(Color.WHITE)
                                textSize = 14f
                                setPadding(16, 2, 0, 2)
                            }
                            routeCandidatesLayout.addView(tv)
                        }
                    }
                }

                // 如果是车次/速度/公里标 → 1234000
                if (parsed.trainNo.isNotEmpty()) {
                    trainNoLabel.text = "车次号: ${parsed.trainNo}"
                    speedLabel.text = "速度: ${parsed.speed} km/h"
                    mileageLabel.text = "公里标: ${parsed.mileage} km"
                }
            }
        }
    }

    override fun onDestroy() {
        nativeStopClient()
        super.onDestroy()
    }

    external fun startClientAsync(host: String, port: String)
    external fun pollMessages(): String
    external fun nativeStopClient()
    external fun decodeMessageNative(msg: String): ParsedMessage

    companion object {
        init { System.loadLibrary("railwaypagerdemod") }
    }
}
