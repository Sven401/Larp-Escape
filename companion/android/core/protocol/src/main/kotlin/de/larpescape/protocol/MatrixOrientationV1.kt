package de.larpescape.protocol

private const val MATRIX_EDGE_V1 = 9

data class MatrixCoordinateV1(val row: Int, val column: Int) {
    init {
        require(row in 0 until MATRIX_EDGE_V1)
        require(column in 0 until MATRIX_EDGE_V1)
    }
}

enum class MatrixRotationV1(val clockwiseQuarterTurns: Int) {
    DEG_0(0),
    DEG_90(1),
    DEG_180(2),
    DEG_270(3),
}

/** Display-only transform. Raw firmware coordinates remain unchanged. */
data class MatrixOrientationV1(
    val rotation: MatrixRotationV1,
    val mirrorHorizontally: Boolean,
) {
    fun displayCoordinate(raw: MatrixCoordinateV1): MatrixCoordinateV1 {
        var row = raw.row
        var column = if (mirrorHorizontally) MATRIX_EDGE_V1 - 1 - raw.column else raw.column
        repeat(rotation.clockwiseQuarterTurns) {
            val previousRow = row
            row = column
            column = MATRIX_EDGE_V1 - 1 - previousRow
        }
        return MatrixCoordinateV1(row, column)
    }

    companion object {
        val all = MatrixRotationV1.entries.flatMap { rotation ->
            listOf(false, true).map { mirrored -> MatrixOrientationV1(rotation, mirrored) }
        }
    }
}

data class MatrixOrientationReferenceV1(
    val raw: MatrixCoordinateV1,
    val observedDisplay: MatrixCoordinateV1,
    val userConfirmed: Boolean,
)

enum class MatrixOrientationSuggestionStateV1 {
    INSUFFICIENT_CONFIRMED_REFERENCES,
    NO_MATCH,
    AMBIGUOUS,
    UNIQUE,
}

data class MatrixOrientationSuggestionV1(
    val state: MatrixOrientationSuggestionStateV1,
    val confirmedReferenceCount: Int,
    val candidates: List<MatrixOrientationV1>,
)

data class ConfirmedMatrixOrientationV1(
    val orientation: MatrixOrientationV1,
    val confirmedReferenceCount: Int,
)

object MatrixOrientationResolverV1 {
    fun suggest(references: List<MatrixOrientationReferenceV1>): MatrixOrientationSuggestionV1 {
        val confirmed = references.filter { it.userConfirmed }.distinctBy { it.raw to it.observedDisplay }
        if (confirmed.size < 2) {
            return MatrixOrientationSuggestionV1(
                MatrixOrientationSuggestionStateV1.INSUFFICIENT_CONFIRMED_REFERENCES,
                confirmed.size,
                emptyList(),
            )
        }
        val candidates = MatrixOrientationV1.all.filter { orientation ->
            confirmed.all { orientation.displayCoordinate(it.raw) == it.observedDisplay }
        }
        val state = when (candidates.size) {
            0 -> MatrixOrientationSuggestionStateV1.NO_MATCH
            1 -> MatrixOrientationSuggestionStateV1.UNIQUE
            else -> MatrixOrientationSuggestionStateV1.AMBIGUOUS
        }
        return MatrixOrientationSuggestionV1(state, confirmed.size, candidates)
    }

    /** Returns a persistable value only after an explicit caller confirmation. */
    fun confirmForStorage(
        references: List<MatrixOrientationReferenceV1>,
        userSelected: MatrixOrientationV1,
        userConfirmed: Boolean,
    ): ConfirmedMatrixOrientationV1? {
        if (!userConfirmed) return null
        val suggestion = suggest(references)
        if (suggestion.state != MatrixOrientationSuggestionStateV1.UNIQUE ||
            suggestion.candidates.single() != userSelected) return null
        return ConfirmedMatrixOrientationV1(userSelected, suggestion.confirmedReferenceCount)
    }
}
