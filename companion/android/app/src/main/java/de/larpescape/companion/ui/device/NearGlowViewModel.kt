package de.larpescape.companion.ui.device

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import de.larpescape.companion.platform.ble.AndroidNearGlowRepository

class NearGlowViewModel(application: Application) : AndroidViewModel(application) {
    private val repository = AndroidNearGlowRepository(application)
    val state = repository.state
    fun foregroundStarted(granted: Boolean) = repository.foregroundStarted(granted)
    fun foregroundEnded() = repository.foregroundEnded()
    fun permissionsChanged(granted: Boolean) = repository.permissionsChanged(granted)
    fun importAccess(slot: Int, bytes: ByteArray) = repository.importAccess(slot, bytes)
    fun connect(slot: Int) = repository.connect(slot)
    fun disconnect(slot: Int) = repository.disconnect(slot)
    fun edit(point: Int, rssi: String, percent: String) = repository.edit(point, rssi, percent)
    fun applyBoth() = repository.applyBoth()
    override fun onCleared() { repository.foregroundEnded() }
}
