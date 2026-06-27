package com.example.tugasakhir

import android.app.Notification
import android.app.Service
import android.content.Intent
import android.os.IBinder
import com.google.firebase.database.*
import android.content.pm.ServiceInfo
import android.os.Build
import java.text.SimpleDateFormat
import java.util.*

class FirebaseMonitorService : Service() {

    private lateinit var database: DatabaseReference

    override fun onCreate() {
        super.onCreate()

        NotificationHelper.createChannel(this)

        val notification: Notification =
            NotificationHelper.buildForegroundNotification(this)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {

            startForeground(
                1,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC
            )

        } else {

            startForeground(
                1,
                notification
            )
        }

        monitorFirebase()
    }

    private fun monitorFirebase() {

        database = FirebaseDatabase
            .getInstance()
            .getReference("servo/status")

        database.addValueEventListener(object : ValueEventListener {

            override fun onDataChange(snapshot: DataSnapshot) {

                val status = snapshot.getValue(String::class.java)

                if (status == "MENYALA") {

                    NotificationHelper.showNotification(
                        this@FirebaseMonitorService,
                        "Pemberian Pakan",
                        "Servo sedang memberikan pakan"
                    )

                    saveNotification(
                        "Pemberian Pakan",
                        "Servo sedang memberikan pakan"
                    )
                }

                if (status == "MATI") {

                    NotificationHelper.showNotification(
                        this@FirebaseMonitorService,
                        "Pemberian Pakan Selesai",
                        "Servo telah selesai memberikan pakan"
                    )

                    saveNotification(
                        "Pemberian Pakan Selesai",
                        "Servo telah selesai memberikan pakan"
                    )
                }
            }

            override fun onCancelled(error: DatabaseError) {

            }
        })
    }

    private fun saveNotification(
        title: String,
        message: String
    ) {

        val database =
            FirebaseDatabase.getInstance()
                .getReference("notifications")

        val currentTime =
            SimpleDateFormat(
                "dd-MM-yyyy HH:mm:ss",
                Locale.getDefault()
            ).format(Date())

        val data = HashMap<String, String>()

        data["title"] = title
        data["message"] = message
        data["time"] = currentTime

        database.push().setValue(data)
    }

    override fun onBind(intent: Intent?): IBinder? {
        return null
    }
}