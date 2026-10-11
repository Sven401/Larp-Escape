package de.larpescape.protocol

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class MatrixOrientationV1Test {
    @Test
    fun allEightTransformsKeepEveryRawPointInRangeAndDistinct() {
        val rawPoints = (0..8).flatMap { row -> (0..8).map { column -> MatrixCoordinateV1(row, column) } }
        MatrixOrientationV1.all.forEach { orientation ->
            val displayed = rawPoints.map(orientation::displayCoordinate)
            assertEquals(81, displayed.distinct().size)
            assertTrue(displayed.all { it.row in 0..8 && it.column in 0..8 })
        }
    }

    @Test
    fun suggestionIgnoresUnconfirmedPointsAndNeedsMultipleConfirmedReferences() {
        val result = MatrixOrientationResolverV1.suggest(
            listOf(
                reference(0, 0, 0, 8, true),
                reference(2, 1, 1, 6, false),
            ),
        )
        assertEquals(MatrixOrientationSuggestionStateV1.INSUFFICIENT_CONFIRMED_REFERENCES, result.state)
        assertEquals(1, result.confirmedReferenceCount)
        assertTrue(result.candidates.isEmpty())
    }

    @Test
    fun asymmetricConfirmedPointsProduceUniqueSuggestionWithoutChangingRawCoordinates() {
        val rawA = MatrixCoordinateV1(0, 1)
        val rawB = MatrixCoordinateV1(2, 7)
        val expected = MatrixOrientationV1(MatrixRotationV1.DEG_90, mirrorHorizontally = true)
        val result = MatrixOrientationResolverV1.suggest(
            listOf(
                MatrixOrientationReferenceV1(rawA, expected.displayCoordinate(rawA), true),
                MatrixOrientationReferenceV1(rawB, expected.displayCoordinate(rawB), true),
            ),
        )
        assertEquals(MatrixOrientationSuggestionStateV1.UNIQUE, result.state)
        assertEquals(listOf(expected), result.candidates)
        assertEquals(MatrixCoordinateV1(0, 1), rawA)
        assertEquals(MatrixCoordinateV1(2, 7), rawB)
    }

    @Test
    fun orientationCannotBeStoredWithoutExplicitConfirmationOrMatchingCandidate() {
        val expected = MatrixOrientationV1(MatrixRotationV1.DEG_270, mirrorHorizontally = false)
        val references = listOf(MatrixCoordinateV1(0, 1), MatrixCoordinateV1(3, 8)).map {
            MatrixOrientationReferenceV1(it, expected.displayCoordinate(it), true)
        }
        assertNull(MatrixOrientationResolverV1.confirmForStorage(references, expected, userConfirmed = false))
        assertNull(
            MatrixOrientationResolverV1.confirmForStorage(
                references,
                MatrixOrientationV1(MatrixRotationV1.DEG_0, mirrorHorizontally = false),
                userConfirmed = true,
            ),
        )
        assertEquals(
            ConfirmedMatrixOrientationV1(expected, 2),
            MatrixOrientationResolverV1.confirmForStorage(references, expected, userConfirmed = true),
        )
    }

    @Test
    fun fabricatedSuggestionCannotBypassConfirmedReferenceCheck() {
        val expected = MatrixOrientationV1(MatrixRotationV1.DEG_90, mirrorHorizontally = true)
        val unconfirmed = listOf(
            MatrixOrientationReferenceV1(MatrixCoordinateV1(0, 1), expected.displayCoordinate(MatrixCoordinateV1(0, 1)), false),
            MatrixOrientationReferenceV1(MatrixCoordinateV1(3, 7), expected.displayCoordinate(MatrixCoordinateV1(3, 7)), false),
        )
        assertNull(MatrixOrientationResolverV1.confirmForStorage(unconfirmed, expected, userConfirmed = true))
    }

    @Test
    fun contradictoryReferencesHaveNoCandidate() {
        val result = MatrixOrientationResolverV1.suggest(
            listOf(reference(0, 0, 0, 0, true), reference(0, 1, 8, 8, true)),
        )
        assertEquals(MatrixOrientationSuggestionStateV1.NO_MATCH, result.state)
        assertTrue(result.candidates.isEmpty())
    }

    private fun reference(
        rawRow: Int,
        rawColumn: Int,
        displayRow: Int,
        displayColumn: Int,
        confirmed: Boolean,
    ) = MatrixOrientationReferenceV1(
        MatrixCoordinateV1(rawRow, rawColumn),
        MatrixCoordinateV1(displayRow, displayColumn),
        confirmed,
    )
}
