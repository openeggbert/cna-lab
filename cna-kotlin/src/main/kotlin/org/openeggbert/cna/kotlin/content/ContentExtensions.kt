@file:JvmName("CnaContentExtensions")

package org.openeggbert.cna.kotlin.content

import Microsoft.Xna.Framework.Content.ContentManager

/**
 * Loads through CNA-Java's ordinary class-token API without changing its cache or ownership.
 *
 * The result remains nullable because the current Java API has no nullability metadata and its
 * managed reader path can return null for reference assets.
 */
public inline fun <reified T : Any> ContentManager.load(assetName: String): T? =
    Load(T::class.java, assetName)
