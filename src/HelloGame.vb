Imports System
Imports System.IO
Imports Microsoft.Xna.Framework
Imports Microsoft.Xna.Framework.Graphics
Imports Microsoft.Xna.Framework.Input

Public NotInheritable Class HelloGame
    Inherits Game

    Private Const MaximumLogoScale As Single = 1.08F
    Private Const AnimationSpeed As Single = 2.0F

    Private ReadOnly _graphics As GraphicsDeviceManager
    Private ReadOnly _frameLimit As Integer?
    Private _spriteBatch As SpriteBatch
    Private _logo As Texture2D
    Private _position As Vector2
    Private _velocity As New Vector2(104.0F, 74.0F)
    Private _animationSeconds As Single
    Private _drawnFrames As Integer

    Public Sub New(Optional frameLimit As Integer? = Nothing)
        If frameLimit.HasValue AndAlso frameLimit.Value <= 0 Then
            Throw New ArgumentOutOfRangeException(NameOf(frameLimit))
        End If

        _frameLimit = frameLimit
        _graphics = New GraphicsDeviceManager(Me)
        Content.RootDirectory = "Content"
        IsMouseVisible = True
        Window.Title = "CNA VB.NET HelloGame"
        Window.AllowUserResizing = True
        AddHandler Window.ClientSizeChanged, AddressOf OnClientSizeChanged
    End Sub

    Protected Overrides Sub LoadContent()
        Dim newSpriteBatch As SpriteBatch = Nothing
        Dim newLogo As Texture2D = Nothing

        Try
            newSpriteBatch = New SpriteBatch(GraphicsDevice)
            newLogo = LoadRawTexture("logo.png")

            If newLogo.Width <= 0 OrElse newLogo.Height <= 0 Then
                Throw New InvalidDataException("Content/logo.png decoded to invalid dimensions.")
            End If

            Dim viewport As Viewport = GraphicsDevice.Viewport
            Dim initialPosition As New Vector2(viewport.Width * 0.5F, viewport.Height * 0.5F)
            Console.WriteLine($"Loaded Content/logo.png ({newLogo.Width}x{newLogo.Height})")

            ' Transfer ownership only after every potentially failing setup operation succeeds.
            _spriteBatch = newSpriteBatch
            _logo = newLogo
            _position = initialPosition
        Catch
            If newLogo IsNot Nothing Then
                newLogo.Dispose()
            End If
            If newSpriteBatch IsNot Nothing Then
                newSpriteBatch.Dispose()
            End If
            Throw
        End Try
    End Sub

    Private Function LoadRawTexture(fileName As String) As Texture2D
        Dim contentPath As String = System.IO.Path.Combine(AppContext.BaseDirectory, Content.RootDirectory, fileName)
        Using stream As Stream = File.OpenRead(contentPath)
            Return Texture2D.FromStream(GraphicsDevice, stream)
        End Using
    End Function

    Protected Overrides Sub Update(gameTime As GameTime)
        Dim keyboardState As KeyboardState = Keyboard.GetState()
        Dim mouseState As MouseState = Mouse.GetState()
        Dim gamePadState As GamePadState = GamePad.GetState(PlayerIndex.One)

        If keyboardState.IsKeyDown(Keys.Escape) OrElse
           gamePadState.Buttons.Back = ButtonState.Pressed Then
            Me.Exit()
            Return
        End If

        If _logo Is Nothing Then
            MyBase.Update(gameTime)
            Return
        End If

        If mouseState.LeftButton = ButtonState.Pressed Then
            _position = New Vector2(mouseState.X, mouseState.Y)
        End If

        Dim deltaTime As Single = Math.Min(CSng(gameTime.ElapsedGameTime.TotalSeconds), 0.1F)
        _animationSeconds += deltaTime
        _position += _velocity * deltaTime

        KeepLogoInsideViewport()
        MyBase.Update(gameTime)
    End Sub

    Private Sub KeepLogoInsideViewport()
        Dim viewport As Viewport = GraphicsDevice.Viewport
        Dim halfWidth As Single = _logo.Width * 0.5F * MaximumLogoScale
        Dim halfHeight As Single = _logo.Height * 0.5F * MaximumLogoScale
        Dim minX As Single = Math.Min(halfWidth, viewport.Width * 0.5F)
        Dim minY As Single = Math.Min(halfHeight, viewport.Height * 0.5F)
        Dim maxX As Single = Math.Max(minX, viewport.Width - minX)
        Dim maxY As Single = Math.Max(minY, viewport.Height - minY)

        If _position.X < minX Then
            _position.X = minX
            _velocity.X = Math.Abs(_velocity.X)
        ElseIf _position.X > maxX Then
            _position.X = maxX
            _velocity.X = -Math.Abs(_velocity.X)
        End If

        If _position.Y < minY Then
            _position.Y = minY
            _velocity.Y = Math.Abs(_velocity.Y)
        ElseIf _position.Y > maxY Then
            _position.Y = maxY
            _velocity.Y = -Math.Abs(_velocity.Y)
        End If
    End Sub

    Protected Overrides Sub Draw(gameTime As GameTime)
        GraphicsDevice.Clear(Color.CornflowerBlue)
        DrawLogo()
        MyBase.Draw(gameTime)

        _drawnFrames += 1
        If _frameLimit.HasValue AndAlso _drawnFrames >= _frameLimit.Value Then
            Console.WriteLine($"Completed {_drawnFrames} frames")
            Me.Exit()
        End If
    End Sub

    Private Sub DrawLogo()
        If _spriteBatch Is Nothing OrElse _logo Is Nothing Then
            Return
        End If

        Dim motion As Single = _animationSeconds * AnimationSpeed
        Dim scale As Single = 0.96F + 0.12F * CSng(Math.Sin(motion * 0.65F))
        Dim rotation As Single = 0.11F * CSng(Math.Sin(motion * 0.55F))
        Dim origin As New Vector2(_logo.Width * 0.5F, _logo.Height * 0.5F)

        _spriteBatch.Begin()
        _spriteBatch.Draw(
            _logo,
            _position,
            Nothing,
            Color.White,
            rotation,
            origin,
            scale,
            SpriteEffects.None,
            0.0F)
        _spriteBatch.End()
    End Sub

    Private Sub OnClientSizeChanged(sender As Object, args As EventArgs)
        If _logo IsNot Nothing Then
            KeepLogoInsideViewport()
        End If
    End Sub

    Protected Overrides Sub UnloadContent()
        Dim spriteBatch As SpriteBatch = _spriteBatch
        Dim logo As Texture2D = _logo
        _spriteBatch = Nothing
        _logo = Nothing

        Try
            If spriteBatch IsNot Nothing Then
                spriteBatch.Dispose()
            End If
        Finally
            Try
                If logo IsNot Nothing Then
                    logo.Dispose()
                End If
            Finally
                MyBase.UnloadContent()
            End Try
        End Try

        Console.WriteLine("Released SpriteBatch and Texture2D")
    End Sub
End Class
