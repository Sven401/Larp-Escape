package de.larpescape.companion.ui.device

/** Input identities never change when their display positions are swapped. */
internal object WakeArrangement {
    val defaultOrder = listOf(1, 2, 3, 4)

    fun decode(saved: String?): List<Int> {
        val parts = saved?.split(',') ?: return defaultOrder
        val values = parts.mapNotNull { it.toIntOrNull() }
        return if (parts.size == 4 && values.size == 4 && values.toSet() == defaultOrder.toSet()) values else defaultOrder
    }

    fun swap(order: List<Int>, source: Int, target: Int?): List<Int> {
        if (source !in order.indices || target == null || target !in order.indices || source == target) return order
        return order.toMutableList().also { it[source] = order[target]; it[target] = order[source] }
    }

    /** A preview is derived without mutating the persisted arrangement. */
    fun preview(order: List<Int>, source: Int?, target: Int?): List<Int> =
        if (source == null) order else swap(order, source, target)

    fun key(boxId: String?): String? = boxId?.takeIf { it.isNotBlank() }?.let { "box:$it" }
}

/** Pixel geometry shared by pointer hit testing and the 2 × 2 layout. Gaps are not drop targets. */
internal data class WakeGridGeometry(val width: Int, val height: Int, val gap: Int) {
    val cellWidth: Int get() = ((width - gap) / 2).coerceAtLeast(0)
    val cellHeight: Int get() = ((height - gap) / 2).coerceAtLeast(0)
    fun left(slot: Int): Int = (slot % 2) * (cellWidth + gap)
    fun top(slot: Int): Int = (slot / 2) * (cellHeight + gap)

    fun slotAt(x: Float, y: Float): Int? {
        if (!x.isFinite() || !y.isFinite() || cellWidth <= 0 || cellHeight <= 0) return null
        return (0..3).firstOrNull { slot ->
            x >= left(slot) && x < left(slot) + cellWidth &&
                y >= top(slot) && y < top(slot) + cellHeight
        }
    }
}
