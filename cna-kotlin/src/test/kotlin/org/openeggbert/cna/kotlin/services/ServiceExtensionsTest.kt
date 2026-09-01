package org.openeggbert.cna.kotlin.services

import Microsoft.Xna.Framework.ServiceProvider
import org.junit.jupiter.api.Assertions.assertNull
import org.junit.jupiter.api.Assertions.assertSame
import org.junit.jupiter.api.Test

class ServiceExtensionsTest {
    @Test
    fun `reified service lookup preserves provider identity`() {
        val expected = Marker()
        val provider = ServiceProvider { type -> if (type == Marker::class.java) expected else null }

        assertSame(expected, provider.getService<Marker>())
        assertNull(provider.getService<String>())
    }

    @Test
    fun `reified service lookup safely rejects a provider type mismatch`() {
        val provider = ServiceProvider { Marker() }
        assertNull(provider.getService<String>())
    }

    private class Marker
}
