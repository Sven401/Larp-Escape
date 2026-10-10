package de.larpescape.protocol

import com.google.gson.*
import com.google.gson.stream.JsonReader
import com.google.gson.stream.JsonToken
import java.io.StringReader
import java.nio.ByteBuffer
import java.nio.charset.CodingErrorAction
import java.security.MessageDigest

/** Bounded integer-only JSON and the contract's Python ensure_ascii canonical profile. */
internal object DiagnosticJsonV1 {
    const val MAX_BYTES = 1024 * 1024
    private const val MAX_NODES = 50000
    fun parse(bytes: ByteArray): JsonElement = try { parseStrict(bytes) } catch (error: java.io.IOException) {
        throw IllegalArgumentException("Invalid diagnostic JSON or UTF-8", error)
    }
    private fun parseStrict(bytes: ByteArray): JsonElement {
        require(bytes.size <= MAX_BYTES) { "Diagnostic document too large" }
        val text = Charsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
            .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes)).toString()
        val reader = JsonReader(StringReader(text)).apply { isLenient = false }
        var nodes = 0
        fun read(depth: Int): JsonElement {
            require(depth <= 32 && ++nodes <= MAX_NODES) { "Diagnostic JSON budget exceeded" }
            return when (reader.peek()) {
                JsonToken.BEGIN_OBJECT -> JsonObject().apply {
                    reader.beginObject()
                    while (reader.hasNext()) {
                        val key = reader.nextName()
                        require(key.all { it.code < 128 } && !has(key)) { "Non-ASCII or duplicate key" }
                        add(key, read(depth + 1))
                    }
                    reader.endObject()
                }
                JsonToken.BEGIN_ARRAY -> JsonArray().apply {
                    reader.beginArray(); while (reader.hasNext()) add(read(depth + 1)); reader.endArray()
                }
                JsonToken.STRING -> JsonPrimitive(reader.nextString().also(::checkString))
                JsonToken.NUMBER -> {
                    val raw = reader.nextString()
                    require(Regex("-?(0|[1-9][0-9]*)").matches(raw)) { "Integer required" }
                    val number = raw.toLong()
                    require(number in -9007199254740991L..9007199254740991L)
                    JsonPrimitive(number)
                }
                JsonToken.BOOLEAN -> JsonPrimitive(reader.nextBoolean())
                JsonToken.NULL -> { reader.nextNull(); JsonNull.INSTANCE }
                else -> throw IllegalArgumentException("Invalid diagnostic JSON")
            }
        }
        val value = read(0)
        require(reader.peek() == JsonToken.END_DOCUMENT) { "Trailing JSON" }
        return value
    }

    private fun checkString(text: String) {
        var i = 0
        while (i < text.length) {
            val ch = text[i++]
            if (ch.isHighSurrogate()) require(i < text.length && text[i++].isLowSurrogate()) { "Unpaired surrogate" }
            else require(!ch.isLowSurrogate()) { "Unpaired surrogate" }
        }
    }

    fun canonical(value: JsonElement): ByteArray {
        val out = StringBuilder()
        fun string(text: String) {
            checkString(text)
            out.append('"')
            for (ch in text) when (ch) {
                '"' -> out.append("\\\"")
                '\\' -> out.append("\\\\")
                '\b' -> out.append("\\b")
                '\u000C' -> out.append("\\f")
                '\n' -> out.append("\\n")
                '\r' -> out.append("\\r")
                '\t' -> out.append("\\t")
                else -> if (ch.code < 32 || ch.code >= 127) out.append("\\u" + ch.code.toString(16).padStart(4, '0')) else out.append(ch)
            }
            out.append('"')
        }
        fun write(item: JsonElement) {
            when {
                item.isJsonNull -> out.append("null")
                item.isJsonObject -> {
                    out.append('{')
                    item.asJsonObject.keySet().sorted().forEachIndexed { i, key ->
                        require(key.all { it.code < 128 })
                        if (i > 0) out.append(','); string(key); out.append(':'); write(item.asJsonObject[key])
                    }
                    out.append('}')
                }
                item.isJsonArray -> {
                    out.append('['); item.asJsonArray.forEachIndexed { i, child -> if (i > 0) out.append(','); write(child) }; out.append(']')
                }
                item.asJsonPrimitive.isString -> string(item.asString)
                item.asJsonPrimitive.isBoolean -> out.append(item.asBoolean)
                else -> {
                    require(Regex("-?(0|[1-9][0-9]*)").matches(item.asString))
                    require(item.asLong in -9007199254740991L..9007199254740991L)
                    out.append(item.asLong)
                }
            }
        }
        write(value)
        return out.toString().toByteArray(Charsets.US_ASCII).also { require(it.size <= MAX_BYTES) }
    }
    fun sha256(bytes: ByteArray): String = MessageDigest.getInstance("SHA-256").digest(bytes).joinToString("") { "%02x".format(it) }
}

/** Only the vocabulary used by the pinned v1 schema; unsupported keywords fail closed. */
internal object DiagnosticSchemaV1 {
    private val root = DiagnosticJsonV1.parse(checkNotNull(javaClass.getResourceAsStream("/diagnostic-v1.schema.json")).use { it.readBytes() }).asJsonObject
    private val keywords = setOf("$" + "schema", "$" + "defs", "$" + "ref", "title", "type", "const", "enum", "not", "allOf", "anyOf", "oneOf", "if", "then", "else", "properties", "required", "additionalProperties", "items", "prefixItems", "minItems", "maxItems", "maxLength", "pattern", "minimum", "maximum")
    fun validate(value: JsonElement, definition: String? = null) = validate(value, if (definition == null) root else root.getAsJsonObject("$" + "defs").getAsJsonObject(definition))
    private fun accepts(value: JsonElement, schema: JsonElement): Boolean = try { validate(value, schema); true } catch (_: IllegalArgumentException) { false }
    private fun validate(value: JsonElement, schema: JsonElement) {
        if (schema.isJsonPrimitive) { require(schema.asBoolean); return }
        val s = schema.asJsonObject
        require(s.keySet().all { it in keywords }) { "Unsupported diagnostic schema keyword" }
        s["$" + "ref"]?.let {
            val ref = it.asString
            require(ref.startsWith("#/$" + "defs/"))
            validate(value, root.getAsJsonObject("$" + "defs")[ref.substringAfterLast('/')])
        }
        s["type"]?.let { type -> require(when (type.asString) {
            "object" -> value.isJsonObject; "array" -> value.isJsonArray; "null" -> value.isJsonNull
            "string" -> value.isJsonPrimitive && value.asJsonPrimitive.isString
            "boolean" -> value.isJsonPrimitive && value.asJsonPrimitive.isBoolean
            "integer" -> value.isJsonPrimitive && value.asJsonPrimitive.isNumber && Regex("-?(0|[1-9][0-9]*)").matches(value.asString)
            else -> false
        }) { "Diagnostic schema type mismatch" } }
        s["const"]?.let { require(value == it) { "Diagnostic constant mismatch" } }
        s["enum"]?.let { require(it.asJsonArray.any { option -> value == option }) }
        s["not"]?.let { require(!accepts(value, it)) }
        s["allOf"]?.asJsonArray?.forEach { validate(value, it) }
        s["anyOf"]?.let { require(it.asJsonArray.any { option -> accepts(value, option) }) }
        s["oneOf"]?.let { require(it.asJsonArray.count { option -> accepts(value, option) } == 1) }
        s["if"]?.let { condition -> s[if (accepts(value, condition)) "then" else "else"]?.let { validate(value, it) } }
        if (value.isJsonObject) {
            val obj = value.asJsonObject
            s["required"]?.asJsonArray?.forEach { require(obj.has(it.asString)) { "Missing ${it.asString}" } }
            val properties = s["properties"]?.asJsonObject
            if (s["additionalProperties"]?.asBoolean == false) require(obj.keySet().all { properties?.has(it) == true })
            properties?.entrySet()?.forEach { (key, child) -> obj[key]?.let { validate(it, child) } }
        }
        if (value.isJsonArray) {
            val array = value.asJsonArray
            s["minItems"]?.let { require(array.size() >= it.asInt) }; s["maxItems"]?.let { require(array.size() <= it.asInt) }
            val prefix = s["prefixItems"]?.asJsonArray
            array.forEachIndexed { i, child -> if (prefix != null && i < prefix.size()) validate(child, prefix[i]) else s["items"]?.let { validate(child, it) } }
        }
        if (value.isJsonPrimitive && value.asJsonPrimitive.isString) {
            val text = value.asString
            s["maxLength"]?.let { require(text.codePointCount(0, text.length) <= it.asInt) }
            s["pattern"]?.let { require(Regex(it.asString).containsMatchIn(text)) }
        }
        if (value.isJsonPrimitive && value.asJsonPrimitive.isNumber) {
            s["minimum"]?.let { require(value.asLong >= it.asLong) }; s["maximum"]?.let { require(value.asLong <= it.asLong) }
        }
    }
}
