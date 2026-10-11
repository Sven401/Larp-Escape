package de.larpescape.protocol

import java.util.UUID

/** Reviewed firmware GATT mapping; this identifies transport only, never an authenticated role. */
data class DiagnosticGattRouteV1(
    val service: UUID,
    val controlRequest: UUID,
    val controlReply: UUID,
    val stepRequest: UUID,
    val stepReply: UUID,
)

object DiagnosticGattFirmwareV1 {
    val route = DiagnosticGattRouteV1(
        UUID.fromString("407e6a1d-1cc2-43fd-856b-4aff85a20010"),
        UUID.fromString("407e6a1d-1cc2-43fd-856b-4aff85a20011"),
        UUID.fromString("407e6a1d-1cc2-43fd-856b-4aff85a20012"),
        UUID.fromString("407e6a1d-1cc2-43fd-856b-4aff85a20013"),
        UUID.fromString("407e6a1d-1cc2-43fd-856b-4aff85a20014"),
    )
    const val REQUIRED_MTU = 131
}

/** Facts observed on one live connection after service discovery and subscription. */
data class DiagnosticGattObservationV1(
    val service: UUID,
    val writableWithResponse: Set<UUID>,
    val subscribedNotifications: Set<UUID>,
    val effectiveAttValueBytes: Int,
)

enum class DiagnosticGattBlockV1 {
    MISSING_REVIEWED_ROUTE, MISSING_CONNECTION_OBSERVATION, WRONG_SERVICE, MISSING_WRITE,
    MISSING_REPLY_SUBSCRIPTION, INSUFFICIENT_ATT_VALUE,
}

/** Preflight only: passing it grants no role, device authority, or physical result. */
object DiagnosticGattPreflightV1 {
    const val MIN_ATT_VALUE_BYTES = DiagnosticStepV1.FRAME_BYTES

    fun blocks(route: DiagnosticGattRouteV1?, observed: DiagnosticGattObservationV1?): Set<DiagnosticGattBlockV1> {
        if (route == null) return setOf(DiagnosticGattBlockV1.MISSING_REVIEWED_ROUTE)
        if (observed == null) return setOf(DiagnosticGattBlockV1.MISSING_CONNECTION_OBSERVATION)
        val blocks = mutableSetOf<DiagnosticGattBlockV1>()
        if (route.service != observed.service) blocks += DiagnosticGattBlockV1.WRONG_SERVICE
        if (!observed.writableWithResponse.containsAll(setOf(route.controlRequest, route.stepRequest)))
            blocks += DiagnosticGattBlockV1.MISSING_WRITE
        if (!observed.subscribedNotifications.containsAll(setOf(route.controlReply, route.stepReply)))
            blocks += DiagnosticGattBlockV1.MISSING_REPLY_SUBSCRIPTION
        if (observed.effectiveAttValueBytes < MIN_ATT_VALUE_BYTES)
            blocks += DiagnosticGattBlockV1.INSUFFICIENT_ATT_VALUE
        return blocks
    }
}
