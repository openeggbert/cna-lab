package org.openeggbert.cna.kotlin.math

import Microsoft.Xna.Framework.Color
import Microsoft.Xna.Framework.Matrix
import Microsoft.Xna.Framework.Quaternion
import Microsoft.Xna.Framework.Vector2
import Microsoft.Xna.Framework.Vector3
import Microsoft.Xna.Framework.Vector4
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertNotSame
import org.junit.jupiter.api.Test

class OperatorsTest {
    @Test
    fun `Vector2 operators are CNA-Java calls`() {
        val left = Vector2(8f, -12f)
        val right = Vector2(2f, 4f)
        assertEquals(Vector2.Add(left, right), left + right)
        assertEquals(Vector2.Subtract(left, right), left - right)
        assertEquals(Vector2.Negate(left), -left)
        assertEquals(Vector2.Multiply(left, right), left * right)
        assertEquals(Vector2.Multiply(left, 2f), left * 2f)
        assertEquals(Vector2.Multiply(2f, left), 2f * left)
        assertEquals(Vector2.Divide(left, right), left / right)
        assertEquals(Vector2.Divide(left, 2f), left / 2f)
    }

    @Test
    fun `Vector3 operators are CNA-Java calls`() {
        val left = Vector3(8f, -12f, 16f)
        val right = Vector3(2f, 4f, -8f)
        assertEquals(Vector3.Add(left, right), left + right)
        assertEquals(Vector3.Subtract(left, right), left - right)
        assertEquals(Vector3.Negate(left), -left)
        assertEquals(Vector3.Multiply(left, right), left * right)
        assertEquals(Vector3.Multiply(left, 2f), left * 2f)
        assertEquals(Vector3.Multiply(2f, left), 2f * left)
        assertEquals(Vector3.Divide(left, right), left / right)
        assertEquals(Vector3.Divide(left, 2f), left / 2f)
    }

    @Test
    fun `Vector4 operators are CNA-Java calls`() {
        val left = Vector4(8f, -12f, 16f, -20f)
        val right = Vector4(2f, 4f, -8f, 5f)
        assertEquals(Vector4.Add(left, right), left + right)
        assertEquals(Vector4.Subtract(left, right), left - right)
        assertEquals(Vector4.Negate(left), -left)
        assertEquals(Vector4.Multiply(left, right), left * right)
        assertEquals(Vector4.Multiply(left, 2f), left * 2f)
        assertEquals(Vector4.Multiply(2f, left), 2f * left)
        assertEquals(Vector4.Divide(left, right), left / right)
        assertEquals(Vector4.Divide(left, 2f), left / 2f)
    }

    @Test
    fun `Matrix operators are CNA-Java calls`() {
        val left = Matrix.CreateRotationX(0.25f)
        val right = Matrix(
            2f, 3f, 4f, 5f,
            6f, 7f, 8f, 9f,
            10f, 11f, 12f, 13f,
            14f, 15f, 16f, 17f,
        )
        assertEquals(Matrix.Add(left, right), left + right)
        assertEquals(Matrix.Subtract(left, right), left - right)
        assertEquals(Matrix.Negate(left), -left)
        assertEquals(Matrix.Multiply(left, right), left * right)
        assertEquals(Matrix.Multiply(left, 2f), left * 2f)
        assertEquals(Matrix.Multiply(2f, left), 2f * left)
        assertEquals(Matrix.Divide(left, right), left / right)
        assertEquals(Matrix.Divide(left, 2f), left / 2f)
    }

    @Test
    fun `Quaternion operators are CNA-Java calls`() {
        val left = Quaternion(1f, 2f, 3f, 4f)
        val right = Quaternion(-2f, 3f, -4f, 5f)
        assertEquals(Quaternion.Add(left, right), left + right)
        assertEquals(Quaternion.Subtract(left, right), left - right)
        assertEquals(Quaternion.Negate(left), -left)
        assertEquals(Quaternion.Multiply(left, right), left * right)
        assertEquals(Quaternion.Multiply(left, 2f), left * 2f)
        assertEquals(Quaternion.Divide(left, right), left / right)
    }

    @Test
    fun `Color multiplication is the CNA-Java call`() {
        val value = Color(10, 20, 30, 40)
        assertEquals(Color.Multiply(value, 0.5f), value * 0.5f)
    }

    @Test
    fun `operators preserve CNA-Java mutable value semantics`() {
        val original = Vector2(3f, 4f)
        val result = original + Vector2(1f, 2f)
        assertNotSame(original, result)
        assertEquals(Vector2(3f, 4f), original)
        result.X = 100f
        assertEquals(Vector2(3f, 4f), original)
    }
}
