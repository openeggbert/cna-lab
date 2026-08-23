package org.openeggbert.cna.kotlin

import Microsoft.Xna.Framework.Game
import org.junit.jupiter.api.Assertions.assertDoesNotThrow
import org.junit.jupiter.api.Assertions.assertSame
import org.junit.jupiter.api.Assertions.assertThrows
import org.junit.jupiter.api.Test

class AutoCloseableInteropTest {
    @Test
    fun `standard Kotlin use closes the exact CNA-Java object`() {
        val game = Game()
        game.use { assertSame(game, it) }

        assertThrows(IllegalStateException::class.java) { game.RunOneFrame() }
        assertDoesNotThrow { game.close() }
    }

    @Test
    fun `explicit CNA-Java close remains idempotent`() {
        val game = Game()
        game.close()
        assertDoesNotThrow { game.close() }
    }
}
