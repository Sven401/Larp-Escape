package de.larpescape.companion.ui.device

import android.content.Context
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.Layout
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.Constraints
import androidx.compose.ui.unit.dp
import de.larpescape.companion.data.device.WakeUiState
import kotlin.math.roundToInt

private val positionLabels = listOf("oben links", "oben rechts", "unten links", "unten rechts")

private class WakeOrderStore(context: Context) {
    private val preferences = context.getSharedPreferences("wake-input-arrangement", Context.MODE_PRIVATE)

    fun load(boxId: String?): List<Int> {
        val key = WakeArrangement.key(boxId) ?: return WakeArrangement.defaultOrder
        val saved = preferences.getString(key, null)
        val order = WakeArrangement.decode(saved)
        // Repair invalid data; no arrangement is ever shared by unidentified boxes.
        if (saved != null && saved != order.joinToString(",")) save(boxId, order)
        return order
    }

    fun save(boxId: String?, order: List<Int>) {
        val key = WakeArrangement.key(boxId) ?: return
        preferences.edit().putString(key, order.joinToString(",")).apply()
    }
}

@Composable
fun WakeControls(state: WakeUiState) {
    val context = LocalContext.current.applicationContext
    val store = remember(context) { WakeOrderStore(context) }
    val boxKey = WakeArrangement.key(state.boxId)
    var order by remember(boxKey) { mutableStateOf(store.load(state.boxId)) }

    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Wake-Inputs · gemeinsamer Test", style = MaterialTheme.typography.titleLarge)
            Text(state.message)
            Text("Kachel lange halten, zur Position an der Box ziehen und loslassen. Die andere Kachel erscheint als Vorschau am freien Platz.",
                style = MaterialTheme.typography.bodySmall)
            WakeGrid(state, order, canArrange = boxKey != null) { source, target ->
                val updated = WakeArrangement.swap(order, source, target)
                if (updated != order) {
                    order = updated
                    store.save(state.boxId, updated)
                }
            }
            Text(if (boxKey == null) "Verbinde eine eingerichtete Box, um ihre Anordnung zu speichern."
                else "Anordnung auf diesem Handy für diese Box gespeichert. Außerhalb der Kacheln loslassen bricht den Tausch ab.",
                style = MaterialTheme.typography.bodySmall)
            Text("Aktiv = HIGH. Zuordnung und Ruhepegel prüfen; Wake 1/B7 ist hardwareseitig noch ungeklärt.",
                style = MaterialTheme.typography.bodySmall)
        }
    }
}

private data class WakeDrag(val source: Int, val start: Offset, val pointer: Offset)

@Composable
private fun WakeGrid(state: WakeUiState, order: List<Int>, canArrange: Boolean, swap: (Int, Int) -> Unit) {
    val density = LocalDensity.current
    val configuration = LocalConfiguration.current
    val haptics = LocalHapticFeedback.current
    val gap = with(density) { 12.dp.roundToPx() }
    val minimumHeight = with(density) { 136.dp.roundToPx() }
    // A drag is transient, including across rotation, resize, box switches and font changes.
    val layoutIdentity = listOf(configuration.orientation, configuration.screenWidthDp,
        configuration.screenHeightDp, density.fontScale)
    var drag by remember(state.boxId, layoutIdentity) { mutableStateOf<WakeDrag?>(null) }
    var geometry by remember { mutableStateOf(WakeGridGeometry(0, 0, gap)) }
    val currentOrder by rememberUpdatedState(order)
    val currentSwap by rememberUpdatedState(swap)
    val target = drag?.let { geometry.slotAt(it.pointer.x, it.pointer.y) }
    val preview = WakeArrangement.preview(order, drag?.source, target)

    Layout(
        modifier = Modifier.fillMaxWidth()
            .onSizeChanged {
                val updated = WakeGridGeometry(it.width, it.height, gap)
                if (updated != geometry) { drag = null; geometry = updated }
            }
            .pointerInput(state.boxId, canArrange, layoutIdentity) {
                if (!canArrange) return@pointerInput
                detectDragGesturesAfterLongPress(
                    onDragStart = { point ->
                        geometry.slotAt(point.x, point.y)?.let { source ->
                            drag = WakeDrag(source, point, point)
                            haptics.performHapticFeedback(HapticFeedbackType.LongPress)
                        }
                    },
                    onDrag = { change, amount ->
                        drag?.let { drag = it.copy(pointer = it.pointer + amount); change.consume() }
                    },
                    onDragEnd = {
                        drag?.let { ended ->
                            geometry.slotAt(ended.pointer.x, ended.pointer.y)?.let { destination ->
                                if (destination != ended.source) currentSwap(ended.source, destination)
                            }
                        }
                        drag = null
                    },
                    onDragCancel = { drag = null },
                )
            },
        content = {
            repeat(4) { slot ->
                val lifted = drag
                val empty = lifted != null && (slot == target || (target == null && slot == lifted.source))
                if (empty) {
                    WakeDropSlot(if (target == null) "Freier Platz" else "Hier ablegen")
                } else {
                    val input = preview[slot]
                    val isPreview = lifted != null && slot == lifted.source && target != null && target != slot
                    val actions = if (canArrange && lifted == null) (0..3).filter { it != slot }.map { destination ->
                        CustomAccessibilityAction("Mit ${positionLabels[destination]} tauschen") {
                            // Resolve the stable input identity at action time, after any prior swap.
                            val source = currentOrder.indexOf(input)
                            if (source >= 0) currentSwap(source, destination)
                            source >= 0
                        }
                    } else emptyList()
                    WakeTile(input, state.active(input), positionLabels[slot], preview = isPreview,
                        modifier = Modifier.semantics(mergeDescendants = true) {
                            contentDescription = "Wake-Input $input, ${positionLabels[slot]}"
                            stateDescription = wakeStateLabel(state.active(input))
                            customActions = actions
                        })
                }
            }
            // This fifth child is drawn above the grid at the original grip offset.
            drag?.let {
                val input = order[it.source]
                WakeTile(input, state.active(input), "Angehoben", lifted = true,
                    modifier = Modifier.clearAndSetSemantics { })
            } ?: Box(Modifier)
            // This unplaced sizing child reserves every label variant. Live changes and preview
            // labels therefore cannot resize the grid or interrupt the current gesture.
            WakeTileContent((1..4).map { "Input $it" }, listOf("Aktiv", "Inaktiv", "Unbekannt"),
                listOf("HIGH", "LOW", "Kein aktueller Pegel"), positionLabels + listOf("Vorschau", "Angehoben"))
        },
    ) { children, constraints ->
        val cellWidth = ((constraints.maxWidth - gap) / 2).coerceAtLeast(1)
        // Intrinsic sizing keeps both rows aligned while allowing large system text to wrap.
        val cellHeight = children[5].maxIntrinsicHeight(cellWidth).coerceAtLeast(minimumHeight)
        val measured = children.map { it.measure(Constraints.fixed(cellWidth, cellHeight)) }
        val gridHeight = 2 * cellHeight + gap
        layout(constraints.maxWidth, gridHeight) {
            repeat(4) { slot -> measured[slot].place((slot % 2) * (cellWidth + gap), (slot / 2) * (cellHeight + gap)) }
            drag?.let {
                val movement = it.pointer - it.start
                measured[4].place((it.source % 2) * (cellWidth + gap) + movement.x.roundToInt(),
                    (it.source / 2) * (cellHeight + gap) + movement.y.roundToInt(), zIndex = 1f)
            }
        }
    }
}

private fun wakeStateLabel(active: Boolean?): String = when (active) {
    true -> "Aktiv"
    false -> "Inaktiv"
    null -> "Unbekannt"
}

@Composable
private fun WakeTile(input: Int, active: Boolean?, position: String, modifier: Modifier = Modifier,
                     preview: Boolean = false, lifted: Boolean = false) {
    val colors = MaterialTheme.colorScheme
    val background = when (active) {
        true -> colors.primaryContainer
        false -> colors.surfaceContainerLow
        null -> colors.surfaceContainerHighest
    }
    val foreground = if (active == true) colors.onPrimaryContainer else colors.onSurface
    Surface(modifier = modifier, shape = RoundedCornerShape(16.dp), color = background,
        contentColor = foreground, shadowElevation = if (lifted) 12.dp else 0.dp,
        border = BorderStroke(if (preview || lifted || active == true) 3.dp else 1.dp,
            if (preview || lifted) colors.primary else if (active == true) colors.primary else colors.outline)) {
        WakeTileContent(listOf("Input $input"), listOf(wakeStateLabel(active)),
            listOf(when (active) { true -> "HIGH"; false -> "LOW"; null -> "Kein aktueller Pegel" }),
            listOf(if (preview) "Vorschau" else position))
    }
}

@Composable
private fun WakeTileContent(inputs: List<String>, states: List<String>, levels: List<String>, labels: List<String>) {
    Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        WakeTextLayer(inputs, MaterialTheme.typography.titleLarge)
        WakeTextLayer(states, MaterialTheme.typography.titleMedium)
        WakeTextLayer(levels, MaterialTheme.typography.bodySmall)
        WakeTextLayer(labels, MaterialTheme.typography.labelMedium)
    }
}

/** One visible label, or overlapping sizing alternatives in the unplaced measurement child. */
@Composable
private fun WakeTextLayer(labels: List<String>, style: TextStyle) {
    Box { labels.forEach { Text(it, style = style) } }
}

@Composable
private fun WakeDropSlot(label: String) {
    Surface(Modifier.clearAndSetSemantics { }, shape = RoundedCornerShape(16.dp),
        color = MaterialTheme.colorScheme.surfaceContainerLowest,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline)) {
        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.Center) {
            Text(label, style = MaterialTheme.typography.bodyMedium)
        }
    }
}
