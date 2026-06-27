package com.example.tugasakhir

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.google.firebase.database.*

class LogNotificationFragment : Fragment() {

    private lateinit var database: DatabaseReference

    private var lastStatus: String? = null

    private val notificationList =
        ArrayList<NotificationModel>()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {

        return inflater.inflate(
            R.layout.fragment_log_notification,
            container,
            false
        )
    }

    override fun onViewCreated(
        view: View,
        savedInstanceState: Bundle?
    ) {
        super.onViewCreated(view, savedInstanceState)

        NotificationHelper.createChannel(
            requireContext()
        )

        database =
            FirebaseDatabase.getInstance()
                .getReference("notifications")

        database.addValueEventListener(
            object : ValueEventListener {

                override fun onDataChange(
                    snapshot: DataSnapshot
                ) {

                    notificationList.clear()

                    for (data in snapshot.children) {

                        val item =
                            data.getValue(
                                NotificationModel::class.java
                            )

                        item?.let {
                            notificationList.add(it)
                        }
                    }

                    notificationList.reverse()

//                    adapter.notifyDataSetChanged()
                }

                override fun onCancelled(
                    error: DatabaseError
                ) {}
            }
        )

        database = FirebaseDatabase
            .getInstance()
            .getReference("servo/status")

        database.addValueEventListener(
            object : ValueEventListener {

                override fun onDataChange(
                    snapshot: DataSnapshot
                ) {

                    val status =
                        snapshot.getValue(String::class.java)

                    if (status == null) return

                    if (status == lastStatus) return

                    lastStatus = status

                    when (status) {

                        "MENYALA" -> {

                            NotificationHelper.showNotification(
                                requireContext(),
                                "Servo Menyala",
                                "Pemberian pakan sedang berlangsung"
                            )
                        }

                        "MATI" -> {

                            NotificationHelper.showNotification(
                                requireContext(),
                                "Servo Mati",
                                "Pemberian pakan selesai"
                            )
                        }
                    }
                }

                override fun onCancelled(
                    error: DatabaseError
                ) {
                }
            }
        )
    }
}