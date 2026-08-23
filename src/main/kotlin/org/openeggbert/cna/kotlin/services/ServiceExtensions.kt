@file:JvmName("CnaServiceExtensions")

package org.openeggbert.cna.kotlin.services

import Microsoft.Xna.Framework.ServiceProvider

/** Returns the exact object held by CNA-Java's service provider, or null when absent/wrongly typed. */
public inline fun <reified T : Any> ServiceProvider.getService(): T? =
    GetService(T::class.java) as? T
