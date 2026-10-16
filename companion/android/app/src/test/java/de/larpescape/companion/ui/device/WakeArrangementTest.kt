package de.larpescape.companion.ui.device

import org.junit.Assert.*
import org.junit.Test

class WakeArrangementTest {
    @Test fun allValidStoredOrdersRoundTripWithoutChangingInputIdentities() {
        permutations(listOf(1, 2, 3, 4)).forEach { order ->
            assertEquals(order, WakeArrangement.decode(order.joinToString(",")))
        }
    }

    @Test fun corruptOrIncompleteStorageFallsBackToDefault() {
        listOf(null, "", "1,2,3", "1,2,3,4,5", "1,1,3,4", "0,2,3,4", "2,3,4,5",
            "1,2,no,3,4", "1,2,3,4,", "1,2,3,no").forEach { saved ->
            assertEquals("Corrupt value: $saved", listOf(1, 2, 3, 4), WakeArrangement.decode(saved))
        }
    }

    @Test fun everySwapMovesOnlyTheTwoInputsAndIsReversible() {
        permutations(listOf(1, 2, 3, 4)).forEach { order ->
            for (source in 0..3) for (target in 0..3) {
                val swapped = WakeArrangement.swap(order, source, target)
                assertEquals(order[source], swapped[target])
                assertEquals(order[target], swapped[source])
                for (untouched in (0..3).filter { it != source && it != target }) {
                    assertEquals(order[untouched], swapped[untouched])
                }
                assertEquals(order, WakeArrangement.swap(swapped, source, target))
                assertEquals(setOf(1, 2, 3, 4), swapped.toSet())
            }
        }
    }

    @Test fun previewShowsDisplacedInputAtOriginWithoutCommitting() {
        val committed = listOf(4, 2, 1, 3)
        assertEquals(listOf(3, 2, 1, 4), WakeArrangement.preview(committed, 0, 3))
        assertEquals(listOf(4, 2, 1, 3), committed)
        // Moving across several targets always previews against the committed order.
        assertEquals(listOf(2, 4, 1, 3), WakeArrangement.preview(committed, 0, 1))
        assertEquals(committed, WakeArrangement.preview(committed, 0, null))
        assertEquals(committed, WakeArrangement.preview(committed, null, 3))
    }

    @Test fun cancelledOrInvalidDropLeavesStoredOrderUntouched() {
        val order = listOf(4, 1, 3, 2)
        assertSame(order, WakeArrangement.swap(order, 0, null))
        assertSame(order, WakeArrangement.swap(order, 2, 2))
        assertSame(order, WakeArrangement.swap(order, -1, 2))
        assertSame(order, WakeArrangement.swap(order, 2, 4))
    }

    @Test fun layoutKeysArePerBoxAndNeverUseMissingIdentity() {
        assertNull(WakeArrangement.key(null))
        assertNull(WakeArrangement.key(""))
        assertNull(WakeArrangement.key("  "))
        assertNotEquals(WakeArrangement.key("box-a"), WakeArrangement.key("box-b"))
        assertEquals(WakeArrangement.key("box-a"), WakeArrangement.key("box-a"))
    }

    @Test fun hitTestingMatchesPhysicalTwoByTwoSlots() {
        val geometry = WakeGridGeometry(width = 212, height = 312, gap = 12)
        assertEquals(100, geometry.cellWidth)
        assertEquals(150, geometry.cellHeight)
        assertEquals(0, geometry.slotAt(1f, 1f))
        assertEquals(1, geometry.slotAt(112f, 1f))
        assertEquals(2, geometry.slotAt(1f, 162f))
        assertEquals(3, geometry.slotAt(211f, 311f))
        assertNull(geometry.slotAt(100f, 1f))
        assertNull(geometry.slotAt(105f, 170f))
        assertNull(geometry.slotAt(1f, 155f))
        assertNull(geometry.slotAt(-1f, 1f))
        assertNull(geometry.slotAt(212f, 311f))
        assertNull(geometry.slotAt(211f, 312f))
        assertNull(geometry.slotAt(Float.NaN, 1f))
        assertNull(geometry.slotAt(1f, Float.POSITIVE_INFINITY))
        assertNull(WakeGridGeometry(0, 0, 12).slotAt(0f, 0f))
    }

    private fun permutations(values: List<Int>): List<List<Int>> =
        if (values.isEmpty()) listOf(emptyList())
        else values.flatMap { value -> permutations(values - value).map { listOf(value) + it } }
}
