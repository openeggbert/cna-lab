package com.openeggbert.cna.template

import Microsoft.Xna.Framework.Color
import Microsoft.Xna.Framework.Game
import Microsoft.Xna.Framework.GameTime
import Microsoft.Xna.Framework.Graphics.SpriteBatch
import Microsoft.Xna.Framework.Graphics.Texture2D
import Microsoft.Xna.Framework.GraphicsDeviceManager
import Microsoft.Xna.Framework.Input.ButtonState
import Microsoft.Xna.Framework.Input.Keyboard
import Microsoft.Xna.Framework.Input.Keys
import Microsoft.Xna.Framework.Input.Mouse
import Microsoft.Xna.Framework.Vector2
import org.openeggbert.cna.kotlin.math.plus

/** Small truthful desktop canary for the currently verified CNA-Java runtime slice. */
class HelloGame(private val frameLimit: Int) : Game() {
    internal val graphicsManager: GraphicsDeviceManager
    private var spriteBatch: SpriteBatch? = null
    private var logo: Texture2D? = null
    private var logoPosition = Vector2(16f)
    internal var drawnFrames: Int = 0
        private set

    init {
        require(frameLimit >= 0) { "frameLimit must not be negative" }
        graphicsManager = GraphicsDeviceManager(this)
        window.title = "CNA-Kotlin: ${javaClass.simpleName}"
        content.rootDirectory = "Content"
        isMouseVisible = true
    }

    override fun Initialize() {
        super.Initialize()
        println("cna-kotlin-template: initialized")
    }

    override fun LoadContent() {
        val resource = HelloGame::class.java.getResourceAsStream("/Content/logo.png")
            ?: error("Missing Content/logo.png resource")
        resource.use { png ->
            logo = Texture2D.FromStream(graphicsDevice, png)
        }
        spriteBatch = SpriteBatch(graphicsDevice)
        super.LoadContent()
    }

    override fun Update(gameTime: GameTime) {
        if (
            Keyboard.GetState().IsKeyDown(Keys.Escape) ||
            Mouse.GetState().leftButton == ButtonState.Pressed
        ) {
            Exit()
        }
        val deltaX = gameTime.elapsedGameTime.toNanos().toFloat() / 20_000_000f
        logoPosition = logoPosition + Vector2(deltaX, 0f)
        super.Update(gameTime)
    }

    override fun Draw(gameTime: GameTime) {
        graphicsDevice.Clear(Color.CornflowerBlue)
        val batch = checkNotNull(spriteBatch)
        val texture = checkNotNull(logo)
        batch.Begin()
        batch.Draw(texture, logoPosition, Color.White)
        batch.End()
        drawnFrames++
        if (frameLimit > 0 && drawnFrames >= frameLimit) {
            Exit()
        }
        super.Draw(gameTime)
    }

    override fun UnloadContent() {
        spriteBatch?.close()
        spriteBatch = null
        logo?.close()
        logo = null
        super.UnloadContent()
    }

    override fun EndRun() {
        println("cna-kotlin-template: completed $drawnFrames frames")
        super.EndRun()
    }
}
