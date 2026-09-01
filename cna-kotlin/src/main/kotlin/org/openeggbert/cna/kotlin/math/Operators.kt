@file:JvmName("CnaMathOperators")

package org.openeggbert.cna.kotlin.math

import Microsoft.Xna.Framework.Color
import Microsoft.Xna.Framework.Matrix
import Microsoft.Xna.Framework.Quaternion
import Microsoft.Xna.Framework.Vector2
import Microsoft.Xna.Framework.Vector3
import Microsoft.Xna.Framework.Vector4

/** Kotlin operators that delegate exactly to CNA-Java's XNA methods. */
public operator fun Vector2.plus(other: Vector2): Vector2 = Vector2.Add(this, other)
public operator fun Vector2.minus(other: Vector2): Vector2 = Vector2.Subtract(this, other)
public operator fun Vector2.unaryMinus(): Vector2 = Vector2.Negate(this)
public operator fun Vector2.times(other: Vector2): Vector2 = Vector2.Multiply(this, other)
public operator fun Vector2.times(scale: Float): Vector2 = Vector2.Multiply(this, scale)
public operator fun Float.times(value: Vector2): Vector2 = Vector2.Multiply(this, value)
public operator fun Vector2.div(other: Vector2): Vector2 = Vector2.Divide(this, other)
public operator fun Vector2.div(scale: Float): Vector2 = Vector2.Divide(this, scale)

public operator fun Vector3.plus(other: Vector3): Vector3 = Vector3.Add(this, other)
public operator fun Vector3.minus(other: Vector3): Vector3 = Vector3.Subtract(this, other)
public operator fun Vector3.unaryMinus(): Vector3 = Vector3.Negate(this)
public operator fun Vector3.times(other: Vector3): Vector3 = Vector3.Multiply(this, other)
public operator fun Vector3.times(scale: Float): Vector3 = Vector3.Multiply(this, scale)
public operator fun Float.times(value: Vector3): Vector3 = Vector3.Multiply(this, value)
public operator fun Vector3.div(other: Vector3): Vector3 = Vector3.Divide(this, other)
public operator fun Vector3.div(scale: Float): Vector3 = Vector3.Divide(this, scale)

public operator fun Vector4.plus(other: Vector4): Vector4 = Vector4.Add(this, other)
public operator fun Vector4.minus(other: Vector4): Vector4 = Vector4.Subtract(this, other)
public operator fun Vector4.unaryMinus(): Vector4 = Vector4.Negate(this)
public operator fun Vector4.times(other: Vector4): Vector4 = Vector4.Multiply(this, other)
public operator fun Vector4.times(scale: Float): Vector4 = Vector4.Multiply(this, scale)
public operator fun Float.times(value: Vector4): Vector4 = Vector4.Multiply(this, value)
public operator fun Vector4.div(other: Vector4): Vector4 = Vector4.Divide(this, other)
public operator fun Vector4.div(scale: Float): Vector4 = Vector4.Divide(this, scale)

public operator fun Matrix.plus(other: Matrix): Matrix = Matrix.Add(this, other)
public operator fun Matrix.minus(other: Matrix): Matrix = Matrix.Subtract(this, other)
public operator fun Matrix.unaryMinus(): Matrix = Matrix.Negate(this)
public operator fun Matrix.times(other: Matrix): Matrix = Matrix.Multiply(this, other)
public operator fun Matrix.times(scale: Float): Matrix = Matrix.Multiply(this, scale)
public operator fun Float.times(value: Matrix): Matrix = Matrix.Multiply(this, value)
public operator fun Matrix.div(other: Matrix): Matrix = Matrix.Divide(this, other)
public operator fun Matrix.div(scale: Float): Matrix = Matrix.Divide(this, scale)

public operator fun Quaternion.plus(other: Quaternion): Quaternion = Quaternion.Add(this, other)
public operator fun Quaternion.minus(other: Quaternion): Quaternion = Quaternion.Subtract(this, other)
public operator fun Quaternion.unaryMinus(): Quaternion = Quaternion.Negate(this)
public operator fun Quaternion.times(other: Quaternion): Quaternion = Quaternion.Multiply(this, other)
public operator fun Quaternion.times(scale: Float): Quaternion = Quaternion.Multiply(this, scale)
public operator fun Quaternion.div(other: Quaternion): Quaternion = Quaternion.Divide(this, other)

public operator fun Color.times(scale: Float): Color = Color.Multiply(this, scale)
