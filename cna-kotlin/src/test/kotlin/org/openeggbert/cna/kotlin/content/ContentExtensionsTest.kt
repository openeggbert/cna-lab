package org.openeggbert.cna.kotlin.content

import Microsoft.Xna.Framework.Content.ContentManager
import Microsoft.Xna.Framework.ServiceProvider
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertNull
import org.junit.jupiter.api.Assertions.assertSame
import org.junit.jupiter.api.Test

class ContentExtensionsTest {
    @Test
    fun `reified load passes the exact class token and asset name`() {
        val expected = Asset("logo")
        val content = RecordingContentManager(expected)

        val actual = content.load<Asset>("images/logo")

        assertSame(expected, actual)
        assertEquals(Asset::class.java, content.requestedType)
        assertEquals("images/logo", content.requestedName)
    }

    @Test
    fun `reified load preserves a nullable Java result`() {
        val content = RecordingContentManager(null)
        assertNull(content.load<Asset>("optional"))
    }

    private data class Asset(val name: String)

    private class RecordingContentManager(private val result: Any?) :
        ContentManager(ServiceProvider { null }) {
        var requestedType: Class<*>? = null
        var requestedName: String? = null

        override fun <T : Any?> Load(assetType: Class<T>, assetName: String): T? {
            requestedType = assetType
            requestedName = assetName
            return assetType.cast(result)
        }
    }
}
