package de.larpescape.companion.platform.ble

internal enum class NearGlowCallbackStage { CONNECTION, SERVICES, DESCRIPTOR, WRITE, NOTIFY }
internal enum class NearGlowHandshakeMilestone { HIGH_PRIORITY_REQUESTED, CCCD_ACK, CHALLENGE_COMPLETE, PROOF_QUEUED, AUTH_OK }

/** The production callback boundary. Predicates are evaluated on the serialized queue. */
internal class NearGlowCallbackDispatcher(
    private val post: (() -> Unit) -> Unit,
    private val isCurrent: () -> Boolean,
    private val hasPermission: () -> Boolean,
    private val failed: (NearGlowCallbackStage, Exception) -> Unit,
) {
    fun dispatch(stage: NearGlowCallbackStage, action: () -> Unit) {
        post queued@{
            try {
                if (!isCurrent()) return@queued
                check(hasPermission())
                action()
            } catch (error: Exception) {
                failed(stage, error)
            }
        }
    }
}

/** Only fixed code metadata; never Throwable text, cause, suppressed errors or payloads. */
internal object NearGlowCallbackDiagnostics {
    const val TAG = "NearGlowBle"
    fun line(slot: Int, stage: NearGlowCallbackStage, error: Exception): String {
        val frames = error.stackTrace.take(12).joinToString(" <- ") {
            "${it.className}.${it.methodName}:${it.lineNumber}"
        }
        return "slot=$slot stage=${stage.name} exception=${error.javaClass.name} stack=$frames"
    }
    fun milestone(slot: Int, event: NearGlowHandshakeMilestone, openedAt: Long, eventAt: Long, fragments: Int): String {
        require(slot in 0..1 && eventAt >= openedAt && fragments >= 0)
        return "slot=$slot milestone=${event.name} elapsedMs=${eventAt - openedAt} fragments=$fragments"
    }
    fun writeRejected(slot: Int, sdkStatus: Int, writing: Boolean, queued: Int,
        connectionMatches: Boolean, inputMatches: Boolean): String =
        "slot=$slot event=WRITE_REJECTED sdkStatus=$sdkStatus writing=$writing queued=$queued " +
            "connectionMatches=$connectionMatches inputMatches=$inputMatches"
}
