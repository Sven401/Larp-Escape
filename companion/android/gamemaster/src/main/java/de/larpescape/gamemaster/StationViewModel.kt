package de.larpescape.gamemaster

import android.app.Application
import android.bluetooth.BluetoothManager
import android.content.pm.PackageManager
import android.Manifest
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import de.larpescape.gamemaster.data.StationVault
import de.larpescape.gamemaster.platform.AndroidStationLink
import de.larpescape.gamemaster.station.*
import de.larpescape.protocol.LedAccess
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

data class CatalogState(val loading: Boolean = true, val stations: List<Station> = emptyList(), val error: String? = null)
data class SetupState(val busy: Boolean = false, val imported: Boolean = false, val error: String? = null, val saved: Boolean = false)

class StationViewModel(application: Application) : AndroidViewModel(application) {
    private val handler = Handler(Looper.getMainLooper())
    private val vault = StationVault(application)
    val controller = StationController(AndroidStationLink(application,handler),vault::access,SystemClock::elapsedRealtime,
        StationScheduler { delay, action ->
            val task = Runnable(action); handler.postDelayed(task,delay)
            Cancellation { handler.removeCallbacks(task) }
        })
    private val mutableCatalog = MutableStateFlow(CatalogState())
    val catalog = mutableCatalog.asStateFlow()
    private val mutableSetup = MutableStateFlow(SetupState())
    val setup = mutableSetup.asStateFlow()
    private var preparedAccess: ByteArray? = null
    init { reload() }
    fun reload() {
        viewModelScope.launch {
            mutableCatalog.value = mutableCatalog.value.copy(loading = true,error = null)
            try { mutableCatalog.value = CatalogState(false,withContext(Dispatchers.IO) { vault.stations() }) }
            catch (_: Exception) { mutableCatalog.value = CatalogState(false,error = "Stationsliste kann nicht geöffnet werden. Die verantwortliche Person muss die lokale Einrichtung prüfen.") }
        }
    }
    fun environment() {
        val app = getApplication<Application>()
        val allowed = listOf(Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT).all { app.checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }
        val adapter = app.getSystemService(BluetoothManager::class.java)?.adapter
        controller.environment(allowed,adapter != null,allowed && runCatching { adapter?.isEnabled == true }.getOrDefault(false))
    }
    fun openSetup() { controller.pause(); clearAccess(); mutableSetup.value = SetupState() }
    fun cancelSetup() { clearAccess(); mutableSetup.value = SetupState() }
    fun importAccess(bytes: ByteArray) {
        if (mutableSetup.value.busy) { bytes.fill(0); return }
        try {
            LedAccess.parse(bytes); clearAccess(); preparedAccess = bytes.copyOf()
            mutableSetup.value = SetupState(imported = true)
        } catch (_: Exception) { clearAccess(); mutableSetup.value = mutableSetup.value.copy(imported = false,error = "Diese Datei enthält keinen gültigen Box-Zugang. Die passende Einrichtungsdatei auswählen.") }
        finally { bytes.fill(0) }
    }
    fun importFailed() { mutableSetup.value = mutableSetup.value.copy(error = "Die Zugangsdatei konnte nicht geöffnet werden.") }
    fun save(id: String?, event: String, name: String, address: String) {
        if (mutableSetup.value.busy || mutableCatalog.value.loading || mutableCatalog.value.error != null) return
        mutableSetup.value = mutableSetup.value.copy(busy = true,error = null)
        val credential = preparedAccess?.copyOf()
        viewModelScope.launch {
            try {
                val stations = withContext(Dispatchers.IO) { vault.save(id,event,name,address,credential) }
                mutableCatalog.value = CatalogState(false,stations)
                clearAccess(); controller.stations(); mutableSetup.value = SetupState(saved = true)
            } catch (error: IllegalArgumentException) {
                mutableSetup.value = mutableSetup.value.copy(busy = false,error = error.message ?: "Stationsangaben prüfen.")
            } catch (_: Exception) {
                mutableSetup.value = mutableSetup.value.copy(busy = false,error = "Station konnte nicht gespeichert werden. Angaben, Zugang und lokalen Speicher prüfen lassen.")
            } finally { credential?.fill(0) }
        }
    }
    private fun clearAccess() { preparedAccess?.fill(0); preparedAccess = null }
    override fun onCleared() { controller.close(); clearAccess() }
}
