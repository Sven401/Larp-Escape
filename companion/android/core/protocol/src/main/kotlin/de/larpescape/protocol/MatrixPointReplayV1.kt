package de.larpescape.protocol

/** Replay-only input. Device provenance and scan validity must be verified by a future transport owner. */
sealed interface MatrixReplayEntryV1 {
    val deviceMs: Long

    data class Edge(
        val raw: MatrixCoordinateV1,
        val pressed: Boolean,
        override val deviceMs: Long,
    ) : MatrixReplayEntryV1

    data class ScanFailure(
        val code: MatrixScanFailureCodeV1,
        override val deviceMs: Long,
    ) : MatrixReplayEntryV1
}

enum class MatrixScanFailureCodeV1 { CONFIGURATION, I2C_WRITE, I2C_READ, EVENT_OVERFLOW }

private fun validateMatrixReplayEntriesV1(entries: List<MatrixReplayEntryV1>, unreachable: Boolean) {
    require(entries.size <= 256) { "A point replay must be bounded" }
    require(!unreachable || entries.isEmpty()) {
        "An unreachable observation cannot hide captured device entries"
    }
    require(entries.all { it.deviceMs >= 0 })
    require(entries.zipWithNext().all { (first, second) -> first.deviceMs <= second.deviceMs })
}

data class MatrixPointReplayV1(
    val expectedDisplay: MatrixCoordinateV1,
    val entries: List<MatrixReplayEntryV1>,
    val userMarkedUnreachable: Boolean = false,
) {
    init {
        validateMatrixReplayEntriesV1(entries, userMarkedUnreachable)
    }
}

enum class MatrixPointReplayOutcomeV1 {
    MATCHED_PRESS_AND_RELEASE,
    MISSING,
    PRESS_WITHOUT_RELEASE,
    INVALID_EDGE_SEQUENCE,
    SWAPPED_RAW_POINT,
    ADDITIONAL_RAW_POINT,
    SCAN_ERROR,
    USER_MARKED_UNREACHABLE,
}

data class MatrixPointReplayAssessmentV1(
    val expectedDisplay: MatrixCoordinateV1,
    val expectedRaw: MatrixCoordinateV1,
    val observedRaw: Set<MatrixCoordinateV1>,
    val outcome: MatrixPointReplayOutcomeV1,
)

data class MatrixSweepReplayAssessmentV1(
    val points: List<MatrixPointReplayAssessmentV1>,
    val notAttempted: Set<MatrixCoordinateV1>,
) {
    val matchedSequenceCount: Int
        get() = points.count { it.outcome == MatrixPointReplayOutcomeV1.MATCHED_PRESS_AND_RELEASE }
}

/** Classifies captured events without asserting that the box emitted them or the key was physically observed. */
object MatrixPointReplayV1Evaluator {
    private val allPoints = (0..8).flatMap { row ->
        (0..8).map { column -> MatrixCoordinateV1(row, column) }
    }.toSet()

    fun assess(trial: MatrixPointReplayV1, orientation: MatrixOrientationV1): MatrixPointReplayAssessmentV1 {
        // The data-class list can be backed by a mutable caller list. Recheck
        // at use so it cannot bypass construction-time validation.
        validateMatrixReplayEntriesV1(trial.entries, trial.userMarkedUnreachable)
        val expectedRaw = allPoints.single { orientation.displayCoordinate(it) == trial.expectedDisplay }
        val edges = trial.entries.filterIsInstance<MatrixReplayEntryV1.Edge>()
        val sequences = edges.groupBy { it.raw }.mapValues { (_, rawEdges) -> sequence(rawEdges) }
        val observedRaw = sequences.keys
        val otherEdges = edges.filter { it.raw != expectedRaw }
        val expectedSequence = sequences[expectedRaw] ?: Sequence(false, false, false)
        val outcome = when {
            trial.entries.any { it is MatrixReplayEntryV1.ScanFailure } -> MatrixPointReplayOutcomeV1.SCAN_ERROR
            trial.userMarkedUnreachable -> MatrixPointReplayOutcomeV1.USER_MARKED_UNREACHABLE
            sequences.values.any { it.invalid } -> MatrixPointReplayOutcomeV1.INVALID_EDGE_SEQUENCE
            expectedSequence.complete && otherEdges.isNotEmpty() -> MatrixPointReplayOutcomeV1.ADDITIONAL_RAW_POINT
            expectedSequence.complete && !expectedSequence.down -> MatrixPointReplayOutcomeV1.MATCHED_PRESS_AND_RELEASE
            expectedSequence.down -> MatrixPointReplayOutcomeV1.PRESS_WITHOUT_RELEASE
            otherEdges.isEmpty() -> MatrixPointReplayOutcomeV1.MISSING
            observedRaw.size == 1 && sequences.getValue(observedRaw.single()).let { it.complete && !it.down } ->
                MatrixPointReplayOutcomeV1.SWAPPED_RAW_POINT
            else -> MatrixPointReplayOutcomeV1.ADDITIONAL_RAW_POINT
        }
        return MatrixPointReplayAssessmentV1(trial.expectedDisplay, expectedRaw, observedRaw, outcome)
    }

    fun assessSweep(trials: List<MatrixPointReplayV1>, orientation: MatrixOrientationV1): MatrixSweepReplayAssessmentV1 {
        require(trials.map { it.expectedDisplay }.distinct().size == trials.size) {
            "Each physical target has at most one attempt per sweep; retain retries in separate sweeps"
        }
        val points = trials.map { assess(it, orientation) }
        return MatrixSweepReplayAssessmentV1(points, allPoints - trials.map { it.expectedDisplay }.toSet())
    }

    private data class Sequence(val complete: Boolean, val down: Boolean, val invalid: Boolean)

    private fun sequence(edges: List<MatrixReplayEntryV1.Edge>): Sequence {
        var complete = false
        var down = false
        var invalid = false
        edges.forEach { edge ->
            if (edge.pressed) {
                if (down) invalid = true else down = true
            } else if (!down) {
                invalid = true
            } else {
                down = false
                complete = true
            }
        }
        return Sequence(complete, down, invalid)
    }
}
