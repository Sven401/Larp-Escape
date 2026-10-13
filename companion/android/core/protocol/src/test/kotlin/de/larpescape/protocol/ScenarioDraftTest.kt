package de.larpescape.protocol

import org.junit.Assert.*
import org.junit.Test

class ScenarioDraftTest {
    @Test fun importsViteSerializedDraft() {
        val bytes = requireNotNull(javaClass.getResourceAsStream("/vite-scenario-v1.json")) { "Vite fixture fehlt" }
            .use { it.readBytes() }
        val draft = ScenarioDraftCodec.parse(bytes)
        assertEquals("vite-transfer-2026-10-10", draft.id)
        assertEquals("Vite-Transferprobe", draft.title)
        assertEquals(listOf("vite-scene-2", "vite-scene-1"), draft.scenes.map { it.id })
        assertEquals(listOf("Tor finden", "Tor öffnen"), draft.scenes.map { it.goal })
        assertEquals(draft, ScenarioDraftCodec.parse(ScenarioDraftCodec.serialize(draft)))
    }

    @Test fun viteV1RoundTripAndOrder() {
        val draft = ScenarioDraft("draft-1", "Die Gruft", listOf(
            ScenarioScene("second", "Eingang", "Tür finden", "Symbol an der Wand"),
            ScenarioScene("first", "Finale", "Tor öffnen", "Rune am Rahmen")))
        assertEquals(draft, ScenarioDraftCodec.parse(ScenarioDraftCodec.serialize(draft)))
        assertEquals(listOf("second", "first"), ScenarioDraftCodec.preview(draft).scenes.map { it.id })
    }

    @Test fun incompleteEditingAndPreview() {
        val draft = ScenarioDraft("draft", "", listOf(ScenarioScene("scene", "", "  ", "  Ein Hinweis.  ")))
        ScenarioDraftCodec.validate(draft)
        val preview = ScenarioDraftCodec.preview(draft)
        assertEquals("Unbenanntes Szenario", preview.title)
        assertEquals(3, preview.noteCount)
        assertEquals(listOf("Titel", "Ziel"), preview.scenes.single().missing)
        assertEquals("Ein Hinweis.", preview.scenes.single().clue)
        assertFails { ScenarioDraftCodec.serialize(draft) }
    }

    @Test fun rejectsBrokenAndOversizedImports() {
        assertFails { ScenarioDraftCodec.parse(ByteArray(ScenarioDraftCodec.MAX_BYTES + 1)) }
        assertFails { ScenarioDraftCodec.parse("x".toByteArray()) }
        assertFails { ScenarioDraftCodec.parse("""{"version":2,"id":"x","title":"X","scenes":[]}""".toByteArray()) }
        assertFails { ScenarioDraftCodec.validate(ScenarioDraft("d", scenes = listOf(ScenarioScene("same"), ScenarioScene("same")))) }
        assertFails { ScenarioDraftCodec.validate(ScenarioDraft("d", scenes = listOf(ScenarioScene("s", goal = "x".repeat(601))))) }
    }

    private fun assertFails(block: () -> Unit) {
        try { block(); fail("Expected invalid draft") } catch (_: IllegalArgumentException) { }
    }
}
