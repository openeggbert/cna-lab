package com.openeggbert.cna.template

import Microsoft.Xna.Framework.*
import Microsoft.Xna.Framework.Graphics.*
import Microsoft.Xna.Framework.Content.*
import Microsoft.Xna.Framework.Input.*
import kotlin.math.*

class HelloGame(private var smokeTest: Boolean = false) : Game() {
    private lateinit var graphics: GraphicsDeviceManager
    private lateinit var spriteBatch: SpriteBatch
    private lateinit var logo: Texture2D
    private lateinit var solid: Texture2D
    private var cubeEffect: BasicEffect? = null
    
    private var rendererName: String = "Unknown"
    private var supports3D: Boolean = false
    private var supportsDepth: Boolean = false
    
    private var animationSeconds: Float = 0.0f
    private var drawnFrames: Int = 0
    
    private lateinit var position: Vector2
    private var velocity: Vector2 = Vector2(104.0f, 74.0f)

    companion object {
        private const val ANIMATION_SPEED = 2.0f
        private const val RENDERER_BANNER_SECONDS = 5.0f
        
        private val CUBE_VERTICES: Array<VertexPositionTexture> by lazy {
            createLogoCubeVertices()
        }
        
        private fun createLogoCubeVertices(): Array<VertexPositionTexture> {
            val vertices = mutableListOf<VertexPositionTexture>()
            
            fun addFace(tl: Vector3, tr: Vector3, br: Vector3, bl: Vector3) {
                vertices.add(VertexPositionTexture(tl, Vector2(0f, 0f)))
                vertices.add(VertexPositionTexture(tr, Vector2(1f, 0f)))
                vertices.add(VertexPositionTexture(br, Vector2(1f, 1f)))
                vertices.add(VertexPositionTexture(tl, Vector2(0f, 0f)))
                vertices.add(VertexPositionTexture(br, Vector2(1f, 1f)))
                vertices.add(VertexPositionTexture(bl, Vector2(0f, 1f)))
            }
            
            addFace(Vector3(-1f, 1f, 1f), Vector3(1f, 1f, 1f), Vector3(1f, -1f, 1f), Vector3(-1f, -1f, 1f))
            addFace(Vector3(1f, 1f, -1f), Vector3(-1f, 1f, -1f), Vector3(-1f, -1f, -1f), Vector3(1f, -1f, -1f))
            addFace(Vector3(1f, 1f, 1f), Vector3(1f, 1f, -1f), Vector3(1f, -1f, -1f), Vector3(1f, -1f, 1f))
            addFace(Vector3(-1f, 1f, -1f), Vector3(-1f, 1f, 1f), Vector3(-1f, -1f, 1f), Vector3(-1f, -1f, -1f))
            addFace(Vector3(-1f, 1f, -1f), Vector3(1f, 1f, -1f), Vector3(1f, 1f, 1f), Vector3(-1f, 1f, 1f))
            addFace(Vector3(-1f, -1f, 1f), Vector3(1f, -1f, 1f), Vector3(1f, -1f, -1f), Vector3(-1f, -1f, -1f))
            
            return vertices.toTypedArray()
        }
    }

    init {
        graphics = GraphicsDeviceManager(this)
        Content().RootDirectory = "Content"
        IsMouseVisible = true
    }

    override fun Initialize() {
        super.Initialize()
    }

    override fun LoadContent() {
        spriteBatch = SpriteBatch(GraphicsDevice())
        logo = Content().Load(Texture2D::class.java, "logo")
        
        solid = Texture2D(GraphicsDevice(), 1, 1)
        solid.SetData(arrayOf(Color.White))
        
        rendererName = GetRendererName()
        
        supports3D = SupportsCapability("ThreeD")
        supportsDepth = SupportsCapability("DepthStencilBuffer")
        
        if (supports3D) {
            cubeEffect = BasicEffect(GraphicsDevice()).apply {
                TextureEnabled = true
                Texture = logo
                LightingEnabled = false
                VertexColorEnabled = false
            }
        }
        
        val viewport = GraphicsDevice().Viewport
        position = Vector2(viewport.Width * 0.5f, viewport.Height * 0.5f)
        
        ReportRendererCapabilities()
    }

    private fun GetRendererName(): String {
        return try {
            // Placeholder for native call or GraphicsDevice property
            "CNA (Kotlin)"
        } catch (e: Exception) {
            "Unknown"
        }
    }

    private fun SupportsCapability(capability: String): Boolean {
        // Future-proof: Reach/HiDef capability logic
        return when (capability) {
            "ThreeD" -> true
            "DepthStencilBuffer" -> true
            else -> false
        }
    }

    private fun ReportRendererCapabilities() {
        println("cna-kotlin-template: renderer $rendererName")
        println("  3D pipeline     : ${if (supports3D) "yes" else "no (2D only)"}")
        println("  depth/stencil   : ${if (supportsDepth) "yes" else "no"}")
    }

    override fun Update(gameTime: GameTime) {
        val dt = gameTime.ElapsedGameTime.TotalSeconds.toFloat()
        animationSeconds += dt
        
        if (Keyboard.GetState().IsKeyDown(Keys.Escape)) {
            Exit()
        }
        
        // Bouncing logo logic (2D version)
        if (!supports3D) {
            val movementDelta = dt * 2.0f
            position.X += velocity.X * movementDelta
            position.Y += velocity.Y * movementDelta
            
            val viewport = GraphicsDevice().Viewport
            val logoSize = max(logo.Width, logo.Height).toFloat()
            val minX = logoSize * 0.5f
            val minY = logoSize * 0.5f
            val maxX = max(minX, viewport.Width - minX)
            val maxY = max(minY, viewport.Height - minY)
            
            if (position.X < minX) { position.X = minX; velocity.X = abs(velocity.X) }
            else if (position.X > maxX) { position.X = maxX; velocity.X = -abs(velocity.X) }
            
            if (position.Y < minY) { position.Y = minY; velocity.Y = abs(velocity.Y) }
            else if (position.Y > maxY) { position.Y = maxY; velocity.Y = -abs(velocity.Y) }
        }
        
        super.Update(gameTime)
    }

    override fun Draw(gameTime: GameTime) {
        if (supports3D && supportsDepth) {
            GraphicsDevice().Clear(ClearOptions.Target or ClearOptions.DepthBuffer, Color.CornflowerBlue, 1.0f, 0)
        } else {
            GraphicsDevice().Clear(Color.CornflowerBlue)
        }
        
        if (supports3D) {
            Draw3DLogoCube()
        } else {
            Draw2DLogo()
        }
        
        if (animationSeconds < RENDERER_BANNER_SECONDS) {
            DrawRendererBanner()
        }
        
        if (smokeTest && ++drawnFrames >= 3) {
            println("cna-kotlin-template: smoke test drew $drawnFrames frames; exiting")
            Exit()
        }
        
        super.Draw(gameTime)
    }

    private fun Draw2DLogo() {
        val motionSeconds = animationSeconds * ANIMATION_SPEED
        val scale = 0.96f + 0.12f * sin(motionSeconds * 0.65f)
        val rotation = 0.11f * sin(motionSeconds * 0.55f)
        val origin = Vector2(logo.Width * 0.5f, logo.Height * 0.5f)
        
        spriteBatch.Begin()
        spriteBatch.Draw(logo, position, null, Color.White, rotation, origin, scale, SpriteEffects.None, 0f)
        spriteBatch.End()
    }

    private fun Draw3DLogoCube() {
        val viewport = GraphicsDevice().Viewport
        val aspectRatio = viewport.Width.toFloat() / max(1, viewport.Height)
        val motionSeconds = animationSeconds * ANIMATION_SPEED
        val scale = 0.88f + 0.10f * sin(motionSeconds * 0.48f)
        val moveX = 1.15f * sin(motionSeconds * 0.24f)
        val moveY = 0.65f * sin(motionSeconds * 0.36f)
        
        val world = Matrix.CreateScale(scale) *
                    Matrix.CreateRotationY(motionSeconds * 0.55f) *
                    Matrix.CreateRotationX(motionSeconds * 0.35f) *
                    Matrix.CreateTranslation(moveX, moveY, 0f)
        
        cubeEffect?.apply {
            World = world
            View = Matrix.CreateLookAt(Vector3(0f, 0f, 6f), Vector3.Zero, Vector3.Up)
            Projection = Matrix.CreatePerspectiveFieldOfView(0.7853982f, aspectRatio, 0.1f, 100f)
        }
        
        GraphicsDevice().BlendState = BlendState.Opaque
        GraphicsDevice().DepthStencilState = if (supportsDepth) DepthStencilState.Default else DepthStencilState.None
        GraphicsDevice().RasterizerState = RasterizerState.CullNone
        
        cubeEffect?.CurrentTechnique?.Passes?.forEach { pass ->
            pass.Apply()
            GraphicsDevice().DrawUserPrimitives(PrimitiveType.TriangleList, CUBE_VERTICES, 0, CUBE_VERTICES.size / 3)
        }
    }

    private fun DrawRendererBanner() {
        val viewport = GraphicsDevice().Viewport
        val viewportWidth = viewport.Width
        val glyphColumns = max(1, rendererName.length * 6 - 1)
        val pixelSize = max(1, min((viewportWidth - 48) / glyphColumns, 8))
        
        val textWidth = glyphColumns * pixelSize
        val textHeight = 7 * pixelSize
        val textX = (viewportWidth - textWidth) / 2
        val textY = viewport.Height - textHeight - 24
        
        val translucentWhite = Color(255, 255, 255, 180)
        val padding = 8
        
        spriteBatch.Begin()
        spriteBatch.Draw(solid, Rectangle(textX - padding, textY - padding, textWidth + padding * 2, textHeight + padding * 2), translucentWhite)
        
        rendererName.uppercase().forEachIndexed { i, char ->
            val rows = GetGlyphRows(char)
            val charX = textX + i * 6 * pixelSize
            
            for (row in 0 until 7) {
                val rowData = rows[row].toInt()
                for (col in 0 until 5) {
                    if ((rowData shr (4 - col)) and 1 == 1) {
                        spriteBatch.Draw(solid, Rectangle(charX + col * pixelSize, textY + row * pixelSize, pixelSize, pixelSize), Color.Black)
                    }
                }
            }
        }
        spriteBatch.End()
    }

    private fun GetGlyphRows(c: Char): ByteArray {
        return when (c.uppercaseChar()) {
            'A' -> byteArrayOf(0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11)
            'B' -> byteArrayOf(0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e)
            'C' -> byteArrayOf(0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e)
            'D' -> byteArrayOf(0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e)
            'E' -> byteArrayOf(0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f)
            'F' -> byteArrayOf(0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10)
            'G' -> byteArrayOf(0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f)
            'H' -> byteArrayOf(0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11)
            'I' -> byteArrayOf(0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e)
            'J' -> byteArrayOf(0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c)
            'K' -> byteArrayOf(0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11)
            'L' -> byteArrayOf(0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f)
            'M' -> byteArrayOf(0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11)
            'N' -> byteArrayOf(0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11)
            'O' -> byteArrayOf(0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e)
            'P' -> byteArrayOf(0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10)
            'Q' -> byteArrayOf(0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d)
            'R' -> byteArrayOf(0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11)
            'S' -> byteArrayOf(0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e)
            'T' -> byteArrayOf(0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04)
            'U' -> byteArrayOf(0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e)
            'V' -> byteArrayOf(0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04)
            'W' -> byteArrayOf(0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11)
            'X' -> byteArrayOf(0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11)
            'Y' -> byteArrayOf(0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04)
            'Z' -> byteArrayOf(0x1f, 0x02, 0x04, 0x08, 0x10, 0x10, 0x1f)
            '(' -> byteArrayOf(0x04, 0x08, 0x08, 0x08, 0x08, 0x08, 0x04)
            ')' -> byteArrayOf(0x08, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08)
            ' ' -> byteArrayOf(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)
            else -> byteArrayOf(0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f)
        }
    }
    
    fun SetSmokeTest(enabled: Boolean) {
        smokeTest = enabled
    }
}
