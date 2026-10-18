package de.larpescape.protocol

import com.google.gson.JsonParser
import org.junit.Assert.*
import org.junit.Test
import java.io.File
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

class EspNowProtocolTest {
    private val fixture = JsonParser.parseString(File(System.getProperty("ledFixtures"), "wire-v2.json").readText()).asJsonObject
    private fun bytes(name: String) = fixture[name].asString.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private fun sign(body: ByteArray, domain: String) = Mac.getInstance("HmacSHA256").run {
        init(SecretKeySpec(bytes("secret"),"HmacSHA256")); update("LARP-LED/2/$domain".toByteArray(Charsets.US_ASCII)); body + doFinal(body)
    }
    private fun client(): LedProtocol {
        val access = LedAccess.parse("{\"schemaVersion\":1,\"deviceId\":\"${fixture["device"].asString}\",\"secret\":\"${fixture["secret"].asString}\"}".toByteArray())
        return LedProtocol(access,bytes("client")).also { it.proof(bytes("challenge")) }
    }
    private fun grant(flags: Int = 6) = sign(bytes("granted").copyOf(59).also { it[38]=flags.toByte() },"status")
    private val peer = byteArrayOf(2,3,4,5,6,7)
    private fun body(): ByteArray = ByteArray(76).also {
        bytes("result_start").copyOf(42).copyInto(it)
        it[1]=0x87.toByte(); it[38]=0; it[42]=1; it[43]=1; it[44]=1; it[45]=1
        peer.copyInto(it,46); (52..67).forEach { n -> it[n]=7 }; it[68]=3; it[72]=77
    }
    private fun rejects(action: () -> Unit) {
        try { action(); fail("Expected rejection") } catch (_: IllegalArgumentException) {} catch (_: IllegalStateException) {}
    }
    @Test fun capabilityIsExplicitAndCommandsHaveIndependentDomainAndTarget() {
        val p=client(); val status=p.granted(grant())
        assertEquals(true,status.espNowSupported); assertEquals(true,status.espNowReady)
        val expected=bytes("command_start_twinkle").copyOf(38).also { it[1]=6; it[31]=1; peer.copyInto(it,32) }
        assertArrayEquals(sign(expected,"espnow-command"),p.espNowCommand(status,peer))
        for (target in listOf(ByteArray(6),byteArrayOf(1,2,3,4,5,6),ByteArray(5))) rejects { p.espNowCommand(status,target) }
        rejects { p.espNowCommand(status.copy(espNowReady=false),peer) }
        val old=client(); val unsupported=old.granted(bytes("granted"))
        assertEquals(false,unsupported.espNowSupported); rejects { old.espNowCommand(unsupported,peer) }
        for (flags in listOf(4,5,8,255)) rejects { client().granted(grant(flags)) }
    }
    @Test fun receptionIsBoundToSessionAndEveryAuthenticatedByte() {
        val p=client(); p.granted(grant())
        val wire=sign(body(),"espnow-result"); val r=p.espNowResult(wire)
        assertTrue(r.accepted); assertTrue(r.received); assertEquals(1,r.link)
        assertEquals(3L,r.receiverCount); assertEquals(77L,r.receiverBoot); assertArrayEquals(peer,r.peer)
        for (i in wire.indices) rejects { p.espNowResult(wire.copyOf().also { it[i]=(it[i].toInt() xor 1).toByte() }) }
        for (i in listOf(0,1,18,42,44,45,68)) rejects {
            p.espNowResult(sign(body().also { it[i]=0 },"espnow-result"))
        }
        rejects { p.espNowResult(sign(body().also { it[43]=3 },"espnow-result")) }
        rejects { p.espNowResult(sign(body().also { it[46]=1 },"espnow-result")) }
        rejects { p.espNowResult(sign(body().also { (52..67).forEach { n -> it[n]=0 } },"espnow-result")) }
        rejects { p.espNowResult(sign(body().also { (2..17).forEach { n -> it[n]=0 } },"espnow-result")) }
        rejects { p.audioResult(wire) }; rejects { p.espNowResult(sign(body(),"audio-result")) }
    }
    @Test fun localLinkAndReceptionFailuresRemainDifferent() {
        val p=client(); p.granted(grant())
        val timeout=body().also { it[38]=13; it[44]=0; it[68]=0; it[72]=0 }
        val r=p.espNowResult(sign(timeout,"espnow-result"))
        assertTrue(r.accepted); assertEquals(1,r.link); assertFalse(r.received)
        timeout[38]=12; timeout[43]=2
        assertEquals(12,p.espNowResult(sign(timeout,"espnow-result")).led.outcome)
        timeout[38]=10; timeout[42]=0; timeout[43]=0
        assertFalse(p.espNowResult(sign(timeout,"espnow-result")).accepted)
        rejects { p.espNowResult(sign(timeout.also { it[44]=1 },"espnow-result")) }
    }
    @Test fun peerInputRequiresSixUnicastOctetsAndSelectionDoesNotSend() {
        assertArrayEquals(peer,espNowPeer("02:03:04:05:06:07"))
        for (text in listOf("", "0001", "FF:FF:FF:FF:FF:FF", "00:00:00:00:00:00", "02:03:04:05:06", "02:03:04:05:06:GG")) assertNull(espNowPeer(text))
    }
}
