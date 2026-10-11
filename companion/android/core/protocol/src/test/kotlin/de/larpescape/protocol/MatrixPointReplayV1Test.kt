package de.larpescape.protocol

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class MatrixPointReplayV1Test {
    private val identity = MatrixOrientationV1(MatrixRotationV1.DEG_0, mirrorHorizontally = false)

    @Test
    fun all81TargetsNeedBothRawEdgesToCountAsCompleteSequences() {
        val trials = (0..8).flatMap { row ->
            (0..8).map { column ->
                val point = MatrixCoordinateV1(row, column)
                MatrixPointReplayV1(point, listOf(edge(point, true, 10), edge(point, false, 11)))
            }
        }
        val sweep = MatrixPointReplayV1Evaluator.assessSweep(trials, identity)
        assertEquals(81, sweep.points.size)
        assertEquals(81, sweep.matchedSequenceCount)
        assertTrue(sweep.notAttempted.isEmpty())

        val missingRelease = trials.toMutableList().apply {
            this[80] = MatrixPointReplayV1(MatrixCoordinateV1(8, 8), listOf(edge(MatrixCoordinateV1(8, 8), true, 10)))
        }
        val incomplete = MatrixPointReplayV1Evaluator.assessSweep(missingRelease, identity)
        assertEquals(80, incomplete.matchedSequenceCount)
        assertEquals(MatrixPointReplayOutcomeV1.PRESS_WITHOUT_RELEASE, incomplete.points.last().outcome)
    }

    @Test
    fun orientationMapsPhysicalTargetBackToUnchangedRawCoordinate() {
        val orientation = MatrixOrientationV1(MatrixRotationV1.DEG_90, mirrorHorizontally = true)
        val raw = MatrixCoordinateV1(2, 7)
        val display = orientation.displayCoordinate(raw)
        val result = MatrixPointReplayV1Evaluator.assess(
            MatrixPointReplayV1(display, listOf(edge(raw, true, 1), edge(raw, false, 2))), orientation,
        )
        assertEquals(raw, result.expectedRaw)
        assertEquals(MatrixPointReplayOutcomeV1.MATCHED_PRESS_AND_RELEASE, result.outcome)
    }

    @Test
    fun wrongAdditionalMissingAndUnreachableRemainDistinct() {
        val expected = MatrixCoordinateV1(1, 2)
        val other = MatrixCoordinateV1(3, 4)
        fun result(entries: List<MatrixReplayEntryV1>) = MatrixPointReplayV1Evaluator.assess(
            MatrixPointReplayV1(expected, entries), identity,
        ).outcome

        assertEquals(MatrixPointReplayOutcomeV1.MISSING, result(emptyList()))
        assertEquals(MatrixPointReplayOutcomeV1.SWAPPED_RAW_POINT,
            result(listOf(edge(other, true, 1), edge(other, false, 2))))
        assertEquals(MatrixPointReplayOutcomeV1.ADDITIONAL_RAW_POINT,
            result(listOf(edge(expected, true, 1), edge(other, true, 2), edge(other, false, 3), edge(expected, false, 4))))
        assertEquals(MatrixPointReplayOutcomeV1.USER_MARKED_UNREACHABLE,
            MatrixPointReplayV1Evaluator.assess(MatrixPointReplayV1(expected, emptyList(), true), identity).outcome)
    }

    @Test
    fun scanFailureCannotBeInterpretedAsAnUnpressedMatrix() {
        val point = MatrixCoordinateV1(4, 4)
        val result = MatrixPointReplayV1Evaluator.assess(
            MatrixPointReplayV1(point, listOf(MatrixReplayEntryV1.ScanFailure(MatrixScanFailureCodeV1.I2C_READ, 5))),
            identity,
        )
        assertEquals(MatrixPointReplayOutcomeV1.SCAN_ERROR, result.outcome)
        assertTrue(result.observedRaw.isEmpty())
    }

    @Test
    fun malformedEdgeOrderAndDuplicateTargetAreRejectedOrUnmatched() {
        val point = MatrixCoordinateV1(2, 2)
        val releaseFirst = MatrixPointReplayV1Evaluator.assess(
            MatrixPointReplayV1(point, listOf(edge(point, false, 1), edge(point, true, 2))), identity,
        )
        assertEquals(MatrixPointReplayOutcomeV1.INVALID_EDGE_SEQUENCE, releaseFirst.outcome)
        val other = MatrixCoordinateV1(3, 3)
        val malformedOther = MatrixPointReplayV1Evaluator.assess(
            MatrixPointReplayV1(point, listOf(edge(point, true, 1), edge(other, false, 2), edge(point, false, 3))),
            identity,
        )
        assertEquals(MatrixPointReplayOutcomeV1.INVALID_EDGE_SEQUENCE, malformedOther.outcome)
        rejects { MatrixPointReplayV1(point, listOf(edge(point, true, 2), edge(point, false, 1))) }
        rejects { MatrixPointReplayV1(point, listOf(edge(point, true, 1)), userMarkedUnreachable = true) }
        rejects { MatrixPointReplayV1(point, List(257) { edge(point, true, it.toLong()) }) }
        val trial = MatrixPointReplayV1(point, emptyList())
        rejects { MatrixPointReplayV1Evaluator.assessSweep(listOf(trial, trial), identity) }

        val mutableEntries = mutableListOf<MatrixReplayEntryV1>(edge(point, true, 1))
        val mutableTrial = MatrixPointReplayV1(point, mutableEntries)
        mutableEntries.add(edge(point, false, 0))
        rejects { MatrixPointReplayV1Evaluator.assess(mutableTrial, identity) }
    }

    @Test
    fun sweepKeepsUntestedCoordinatesSeparateFromAttemptFailures() {
        val point = MatrixCoordinateV1(0, 0)
        val sweep = MatrixPointReplayV1Evaluator.assessSweep(listOf(MatrixPointReplayV1(point, emptyList())), identity)
        assertEquals(MatrixPointReplayOutcomeV1.MISSING, sweep.points.single().outcome)
        assertEquals(80, sweep.notAttempted.size)
        assertTrue(point !in sweep.notAttempted)
    }

    private fun edge(raw: MatrixCoordinateV1, pressed: Boolean, ms: Long) =
        MatrixReplayEntryV1.Edge(raw, pressed, ms)

    private fun rejects(block: () -> Unit) {
        var rejected = false
        try { block() } catch (_: IllegalArgumentException) { rejected = true }
        assertTrue(rejected)
    }
}
