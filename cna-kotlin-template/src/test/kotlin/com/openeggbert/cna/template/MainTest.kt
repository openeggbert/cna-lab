package com.openeggbert.cna.template

import Microsoft.Xna.Framework.Game
import Microsoft.Xna.Framework.Vector2
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertNotEquals
import org.junit.jupiter.api.Assertions.assertNotNull
import org.junit.jupiter.api.Assertions.assertThrows
import org.junit.jupiter.api.Test
import org.openeggbert.cna.kotlin.math.plus

class MainTest {
    @Test
    fun `parses deterministic frame modes`() {
        assertEquals(0, parseFrameLimit(emptyArray()))
        assertEquals(60, parseFrameLimit(arrayOf("--smoke-test")))
        assertEquals(600, parseFrameLimit(arrayOf("--stability-test")))
        assertEquals(17, parseFrameLimit(arrayOf("--frames", "17")))
        assertEquals(23, parseFrameLimit(arrayOf("--frames=23")))
    }

    @Test
    fun `rejects invalid arguments`() {
        assertThrows(IllegalArgumentException::class.java) {
            parseFrameLimit(arrayOf("--frames", "0"))
        }
        assertThrows(IllegalArgumentException::class.java) { parseFrameLimit(arrayOf("--frames")) }
        assertThrows(IllegalArgumentException::class.java) {
            parseFrameLimit(arrayOf("--pretend-web-works"))
        }
    }

    @Test
    fun `configures the mapped CNA-Java GameWindow before native startup`() {
        HelloGame(1).use { game ->
            assertEquals("CNA-Kotlin: HelloGame", game.window.title)
        }
    }

    @Test
    fun `ships a real raw PNG fixture rather than an XNB placeholder`() {
        val resource = MainTest::class.java.getResourceAsStream("/Content/logo.png")
        assertNotNull(resource)
        resource.use { png ->
            val signature = png!!.readNBytes(4)
            assertEquals(0x89.toByte(), signature[0])
            assertEquals('P'.code.toByte(), signature[1])
            assertEquals('N'.code.toByte(), signature[2])
            assertEquals('G'.code.toByte(), signature[3])
        }
    }

    @Test
    fun `XNA types come from CNA-Java while operators come from CNA-Kotlin`() {
        val gameLocation = Game::class.java.protectionDomain.codeSource.location
        assertEquals(gameLocation, Vector2::class.java.protectionDomain.codeSource.location)
        assertEquals(
            gameLocation,
            Class.forName("org.openeggbert.cna.internal.NativeBindings")
                .protectionDomain.codeSource.location,
        )
        val adapterLocation = Class.forName("org.openeggbert.cna.kotlin.math.CnaMathOperators")
            .protectionDomain.codeSource.location
        assertNotEquals(gameLocation, adapterLocation)
        assertEquals(Vector2.Add(Vector2(1f), Vector2(2f)), Vector2(1f) + Vector2(2f))
    }
}
