package de.larpescape.companion.data.device

import de.larpescape.protocol.*
import org.junit.Assert.*
import org.junit.Test
import java.nio.file.Files

class RssiJournalTest {
    @Test fun everyResultIncludingFailureAndLateReplySurvivesRestart() {
        val dir=Files.createTempDirectory("rssi-journal").toFile()
        try {
            var time=1000L
            val j=RssiJournal(dir,{time},{time-1000})
            j.begin(byteArrayOf(2,3,4,5,6,7),true)
            fun result(id:Long,ok:Boolean)=EspNowResult(LedResult(id,if(ok)0 else 13,
                LedStatus(2,id+1,123,4,1,1,0)),true,1,ok,byteArrayOf(2,3,4,5,6,7),ByteArray(16){1},
                if(ok)id else 0,if(ok)7 else 0,if(ok)-60 else null,if(ok)-62 else null,if(ok)64 else null)
            repeat(200) { n -> time++; j.event("request",n+1L); j.sample(result(n+1L,n%3!=0)) }
            j.event("stop_requested"); j.sample(result(201,true)); j.close()
            val rows=j.latest!!.readLines()
            assertEquals(201L,j.samples)
            assertEquals(201,rows.count{it.startsWith("sample,")})
            assertTrue(rows.all{it.split(',').size==19})
            assertTrue(rows.last().startsWith("disconnect,"))
            val reopened=RssiJournal(dir,{time},{time-1000})
            assertEquals(201L,reopened.samples)
            val first=reopened.latest!!
            reopened.begin(byteArrayOf(2,3,4,5,6,8),false)
            assertEquals(0L,reopened.samples); assertNotEquals(first,reopened.latest)
            assertEquals(201,first.readLines().count{it.startsWith("sample,")})
        } finally { dir.deleteRecursively() }
    }
    @Test fun storageFailureDoesNotCountAnUnsavedSample() {
        val dir=Files.createTempDirectory("rssi-broken").toFile()
        try {
            val j=RssiJournal(dir,{1},{2}); j.begin(byteArrayOf(2,3,4,5,6,7),false)
            val file=j.latest!!; assertTrue(file.delete()); assertTrue(file.mkdir())
            try { j.event("request",1); fail("must stop rather than silently drop") } catch (_:IllegalStateException) {}
            assertEquals(0L,j.samples)
        } finally { dir.deleteRecursively() }
    }
}
