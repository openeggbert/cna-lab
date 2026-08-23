Imports System
Imports System.Collections.Generic
Imports System.IO
Imports Microsoft.Xna.Framework
Imports Microsoft.Xna.Framework.Graphics
Imports Microsoft.Xna.Framework.Input

Public NotInheritable Class HelloGame
    Inherits Game

    Private Const Maximum2DLogoScale As Single = 1.08F
    Private Const RendererBannerSeconds As Single = 5.0F
    Private Const AnimationSpeed As Single = 2.0F

    Private Shared ReadOnly CubeVertices As VertexPositionTexture() = CreateLogoCubeVertices()

    Private ReadOnly _graphics As GraphicsDeviceManager
    Private ReadOnly _frameLimit As Integer?
    Private _spriteBatch As SpriteBatch
    Private _cubeEffect As BasicEffect
    Private _logo As Texture2D
    Private _solid As Texture2D
    Private _position As Vector2
    Private _velocity As New Vector2(104.0F, 74.0F)
    Private _rendererName As String = "Unknown"
    Private _animationSeconds As Single
    Private _supports3D As Boolean
    Private _supportsDepth As Boolean
    Private _drawnFrames As Integer

    Public Sub New(Optional frameLimit As Integer? = Nothing)
        If frameLimit.HasValue AndAlso frameLimit.Value <= 0 Then
            Throw New ArgumentOutOfRangeException(NameOf(frameLimit))
        End If

        _frameLimit = frameLimit
        _graphics = New GraphicsDeviceManager(Me)
        Content.RootDirectory = "Content"
        IsMouseVisible = True
        Window.Title = "CNA VB.NET template - HelloGame"
        Window.AllowUserResizing = True
        AddHandler Window.ClientSizeChanged, AddressOf OnClientSizeChanged
    End Sub

    Protected Overrides Sub LoadContent()
        Dim newSpriteBatch As SpriteBatch = Nothing
        Dim newCubeEffect As BasicEffect = Nothing
        Dim newLogo As Texture2D = Nothing
        Dim newSolid As Texture2D = Nothing

        Try
            newSpriteBatch = New SpriteBatch(GraphicsDevice)
            newLogo = LoadRawTexture("logo.png")

            If newLogo.Width <= 0 OrElse newLogo.Height <= 0 Then
                Throw New InvalidDataException("Content/logo.png decoded to invalid dimensions.")
            End If

            newSolid = New Texture2D(GraphicsDevice, 1, 1)
            newSolid.SetData(New Color() {Color.White})

            Dim capabilities As EngineDiagnostics.Capabilities = EngineDiagnostics.Inspect(GraphicsDevice)
            If capabilities.Supports3D Then
                newCubeEffect = New BasicEffect(GraphicsDevice)
                newCubeEffect.TextureEnabled = True
                newCubeEffect.Texture = newLogo
                newCubeEffect.LightingEnabled = False
                newCubeEffect.VertexColorEnabled = False
            End If

            Dim viewport As Viewport = GraphicsDevice.Viewport
            Dim initialPosition As New Vector2(viewport.Width * 0.5F, viewport.Height * 0.5F)
            Window.Title = $"CNA VB.NET template - HelloGame ({capabilities.RendererName})"

            Console.WriteLine($"Loaded Content/logo.png ({newLogo.Width}x{newLogo.Height})")
            ReportRendererCapabilities(capabilities)

            ' Transfer ownership only after every potentially failing setup operation succeeds.
            _spriteBatch = newSpriteBatch
            _cubeEffect = newCubeEffect
            _logo = newLogo
            _solid = newSolid
            _rendererName = capabilities.RendererName
            _supports3D = capabilities.Supports3D
            _supportsDepth = capabilities.SupportsDepth
            _position = initialPosition
        Catch
            DisposeResources(newCubeEffect, newSolid, newLogo, newSpriteBatch)
            Throw
        End Try
    End Sub

    Private Function LoadRawTexture(fileName As String) As Texture2D
        Dim contentPath As String = Path.Combine(AppContext.BaseDirectory, Content.RootDirectory, fileName)
        Using stream As Stream = File.OpenRead(contentPath)
            Return Texture2D.FromStream(GraphicsDevice, stream)
        End Using
    End Function

    Private Sub ReportRendererCapabilities(capabilities As EngineDiagnostics.Capabilities)
        Console.WriteLine($"CNA VB.NET template: renderer {capabilities.RendererName}")
        Console.WriteLine($"  3D pipeline     : {If(capabilities.Supports3D, "yes", "no (2D only)")}")
        Console.WriteLine($"  depth/stencil   : {If(capabilities.SupportsDepth, "yes", "no")}")
        Console.WriteLine($"  rendering path  : {If(capabilities.Supports3D, "3D logo cube", "bouncing 2D logo")}")
        Console.WriteLine($"  max texture size: {GetMaxTextureDimension()}")
    End Sub

    Private Function GetMaxTextureDimension() As Integer
        Return If(GraphicsDevice.GraphicsProfile = GraphicsProfile.HiDef, 4096, 2048)
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

        Dim deltaTime As Single = CSng(gameTime.ElapsedGameTime.TotalSeconds)
        _animationSeconds += deltaTime

        If mouseState.LeftButton = ButtonState.Pressed Then
            _position = New Vector2(mouseState.X, mouseState.Y)
        End If

        If Not _supports3D Then
            Dim movementDelta As Single = Math.Min(deltaTime, 0.1F)
            _position += _velocity * movementDelta
            KeepLogoInsideViewport()
        End If

        MyBase.Update(gameTime)
    End Sub

    Private Sub KeepLogoInsideViewport()
        Dim viewport As Viewport = GraphicsDevice.Viewport
        Dim logoSize As Single = Math.Max(_logo.Width, _logo.Height)
        Dim halfExtent As Single = logoSize * 0.5F * Maximum2DLogoScale * 1.15F
        Dim minX As Single = Math.Min(halfExtent, viewport.Width * 0.5F)
        Dim minY As Single = Math.Min(halfExtent, viewport.Height * 0.5F)
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
        If _supports3D AndAlso _supportsDepth Then
            GraphicsDevice.Clear(
                ClearOptions.Target Or ClearOptions.DepthBuffer,
                Color.CornflowerBlue,
                1.0F,
                0)
        Else
            GraphicsDevice.Clear(Color.CornflowerBlue)
        End If

        If _supports3D Then
            Draw3DLogoCube()
        Else
            Draw2DLogo()
        End If

        If _animationSeconds < RendererBannerSeconds Then
            DrawRendererBanner()
        End If

        MyBase.Draw(gameTime)

        _drawnFrames += 1
        If _frameLimit.HasValue AndAlso _drawnFrames >= _frameLimit.Value Then
            Console.WriteLine($"Completed {_drawnFrames} frames")
            Me.Exit()
        End If
    End Sub

    Private Sub Draw2DLogo()
        If _spriteBatch Is Nothing OrElse _logo Is Nothing Then
            Return
        End If

        Dim motionSeconds As Single = _animationSeconds * AnimationSpeed
        Dim scale As Single = 0.96F + 0.12F * CSng(Math.Sin(motionSeconds * 0.65F))
        Dim rotation As Single = 0.11F * CSng(Math.Sin(motionSeconds * 0.55F))
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

    Private Sub Draw3DLogoCube()
        If _cubeEffect Is Nothing Then
            Return
        End If

        Dim viewport As Viewport = GraphicsDevice.Viewport
        Dim aspectRatio As Single = CSng(viewport.Width) / Math.Max(1.0F, CSng(viewport.Height))
        Dim motionSeconds As Single = _animationSeconds * AnimationSpeed
        Dim scale As Single = 0.88F + 0.1F * CSng(Math.Sin(motionSeconds * 0.48F))
        Dim moveX As Single = 1.15F * CSng(Math.Sin(motionSeconds * 0.24F))
        Dim moveY As Single = 0.7F * CSng(Math.Sin(motionSeconds * 0.19F + 1.1F))

        Dim world As Matrix = Matrix.CreateScale(scale)
        world = world * Matrix.CreateRotationX(motionSeconds * 0.22F)
        world = world * Matrix.CreateRotationY(motionSeconds * 0.31F)
        world = world * Matrix.CreateRotationZ(motionSeconds * 0.13F)
        world = world * Matrix.CreateTranslation(moveX, moveY, 0.0F)

        _cubeEffect.World = world
        _cubeEffect.View = Matrix.CreateLookAt(New Vector3(0.0F, 0.0F, 6.0F), Vector3.Zero, Vector3.Up)
        _cubeEffect.Projection = Matrix.CreatePerspectiveFieldOfView(
            0.78539816339F,
            aspectRatio,
            0.1F,
            100.0F)

        GraphicsDevice.BlendState = BlendState.Opaque
        GraphicsDevice.DepthStencilState = If(_supportsDepth, DepthStencilState.Default, DepthStencilState.None)
        GraphicsDevice.RasterizerState = RasterizerState.CullNone

        For Each effectPass As EffectPass In _cubeEffect.CurrentTechnique.Passes
            effectPass.Apply()
            GraphicsDevice.DrawUserPrimitives(
                PrimitiveType.TriangleList,
                CubeVertices,
                0,
                CubeVertices.Length \ 3)
        Next
    End Sub

    Private Sub DrawRendererBanner()
        If _spriteBatch Is Nothing OrElse _solid Is Nothing Then
            Return
        End If

        Dim viewport As Viewport = GraphicsDevice.Viewport
        Dim viewportWidth As Integer = viewport.Width
        Dim glyphColumns As Integer = Math.Max(1, _rendererName.Length * 6 - 1)
        Dim pixelSize As Integer = Math.Clamp((viewportWidth - 48) \ glyphColumns, 1, 8)
        Dim textWidth As Integer = glyphColumns * pixelSize
        Dim textHeight As Integer = 7 * pixelSize
        Dim textX As Integer = (viewportWidth - textWidth) \ 2
        Dim textY As Integer = 28
        Dim padding As Integer = 14
        Dim translucentWhite As New Color(255, 255, 255, 96)
        Dim textColor As New Color(24, 36, 55, 255)

        _spriteBatch.Begin()
        _spriteBatch.Draw(
            _solid,
            New Rectangle(
                textX - padding,
                textY - padding,
                textWidth + padding * 2,
                textHeight + padding * 2),
            translucentWhite)

        For index As Integer = 0 To _rendererName.Length - 1
            Dim rows As Byte() = GetGlyphRows(_rendererName(index))
            Dim characterX As Integer = textX + index * 6 * pixelSize

            For row As Integer = 0 To 6
                For column As Integer = 0 To 4
                    If (rows(row) And (1 << (4 - column))) <> 0 Then
                        _spriteBatch.Draw(
                            _solid,
                            New Rectangle(
                                characterX + column * pixelSize,
                                textY + row * pixelSize,
                                pixelSize,
                                pixelSize),
                            textColor)
                    End If
                Next
            Next
        Next

        _spriteBatch.End()
    End Sub

    Private Sub OnClientSizeChanged(sender As Object, args As EventArgs)
        If _logo Is Nothing Then
            Return
        End If

        Dim viewport As Viewport = GraphicsDevice.Viewport
        _position.X = Math.Clamp(_position.X, 0.0F, CSng(viewport.Width))
        _position.Y = Math.Clamp(_position.Y, 0.0F, CSng(viewport.Height))
    End Sub

    Protected Overrides Sub UnloadContent()
        Dim cubeEffect As BasicEffect = _cubeEffect
        Dim solid As Texture2D = _solid
        Dim logo As Texture2D = _logo
        Dim spriteBatch As SpriteBatch = _spriteBatch
        _cubeEffect = Nothing
        _solid = Nothing
        _logo = Nothing
        _spriteBatch = Nothing

        Try
            DisposeResources(cubeEffect, solid, logo, spriteBatch)
        Finally
            MyBase.UnloadContent()
        End Try

        Console.WriteLine("Released BasicEffect, SpriteBatch, and Texture2D resources")
    End Sub

    Private Shared Sub DisposeResources(
        cubeEffect As BasicEffect,
        solid As Texture2D,
        logo As Texture2D,
        spriteBatch As SpriteBatch)

        Try
            If cubeEffect IsNot Nothing Then
                cubeEffect.Dispose()
            End If
        Finally
            Try
                If solid IsNot Nothing Then
                    solid.Dispose()
                End If
            Finally
                Try
                    If logo IsNot Nothing Then
                        logo.Dispose()
                    End If
                Finally
                    If spriteBatch IsNot Nothing Then
                        spriteBatch.Dispose()
                    End If
                End Try
            End Try
        End Try
    End Sub

    Private Shared Function GetGlyphRows(character As Char) As Byte()
        Select Case Char.ToUpperInvariant(character)
            Case "A"c : Return New Byte() {&HE, &H11, &H11, &H1F, &H11, &H11, &H11}
            Case "B"c : Return New Byte() {&H1E, &H11, &H11, &H1E, &H11, &H11, &H1E}
            Case "C"c : Return New Byte() {&HE, &H11, &H10, &H10, &H10, &H11, &HE}
            Case "D"c : Return New Byte() {&H1E, &H11, &H11, &H11, &H11, &H11, &H1E}
            Case "E"c : Return New Byte() {&H1F, &H10, &H10, &H1E, &H10, &H10, &H1F}
            Case "F"c : Return New Byte() {&H1F, &H10, &H10, &H1E, &H10, &H10, &H10}
            Case "G"c : Return New Byte() {&HE, &H11, &H10, &H17, &H11, &H11, &HE}
            Case "H"c : Return New Byte() {&H11, &H11, &H11, &H1F, &H11, &H11, &H11}
            Case "I"c : Return New Byte() {&H1F, &H4, &H4, &H4, &H4, &H4, &H1F}
            Case "J"c : Return New Byte() {&H7, &H2, &H2, &H2, &H12, &H12, &HC}
            Case "K"c : Return New Byte() {&H11, &H12, &H14, &H18, &H14, &H12, &H11}
            Case "L"c : Return New Byte() {&H10, &H10, &H10, &H10, &H10, &H10, &H1F}
            Case "M"c : Return New Byte() {&H11, &H1B, &H15, &H15, &H11, &H11, &H11}
            Case "N"c : Return New Byte() {&H11, &H19, &H15, &H13, &H11, &H11, &H11}
            Case "O"c : Return New Byte() {&HE, &H11, &H11, &H11, &H11, &H11, &HE}
            Case "P"c : Return New Byte() {&H1E, &H11, &H11, &H1E, &H10, &H10, &H10}
            Case "Q"c : Return New Byte() {&HE, &H11, &H11, &H11, &H15, &H12, &HD}
            Case "R"c : Return New Byte() {&H1E, &H11, &H11, &H1E, &H14, &H12, &H11}
            Case "S"c : Return New Byte() {&HF, &H10, &H10, &HE, &H1, &H1, &H1E}
            Case "T"c : Return New Byte() {&H1F, &H4, &H4, &H4, &H4, &H4, &H4}
            Case "U"c : Return New Byte() {&H11, &H11, &H11, &H11, &H11, &H11, &HE}
            Case "V"c : Return New Byte() {&H11, &H11, &H11, &H11, &H11, &HA, &H4}
            Case "W"c : Return New Byte() {&H11, &H11, &H11, &H15, &H15, &H15, &HA}
            Case "X"c : Return New Byte() {&H11, &H11, &HA, &H4, &HA, &H11, &H11}
            Case "Y"c : Return New Byte() {&H11, &H11, &HA, &H4, &H4, &H4, &H4}
            Case "Z"c : Return New Byte() {&H1F, &H1, &H2, &H4, &H8, &H10, &H1F}
            Case "0"c : Return New Byte() {&HE, &H11, &H13, &H15, &H19, &H11, &HE}
            Case "1"c : Return New Byte() {&H4, &HC, &H4, &H4, &H4, &H4, &HE}
            Case "2"c : Return New Byte() {&HE, &H11, &H1, &H2, &H4, &H8, &H1F}
            Case "3"c : Return New Byte() {&H1E, &H1, &H1, &HE, &H1, &H1, &H1E}
            Case "4"c : Return New Byte() {&H2, &H6, &HA, &H12, &H1F, &H2, &H2}
            Case "5"c : Return New Byte() {&H1F, &H10, &H10, &H1E, &H1, &H1, &H1E}
            Case "6"c : Return New Byte() {&HE, &H10, &H10, &H1E, &H11, &H11, &HE}
            Case "7"c : Return New Byte() {&H1F, &H1, &H2, &H4, &H8, &H8, &H8}
            Case "8"c : Return New Byte() {&HE, &H11, &H11, &HE, &H11, &H11, &HE}
            Case "9"c : Return New Byte() {&HE, &H11, &H11, &HF, &H1, &H1, &HE}
            Case "_"c : Return New Byte() {&H0, &H0, &H0, &H0, &H0, &H0, &H1F}
            Case "-"c : Return New Byte() {&H0, &H0, &H0, &H1F, &H0, &H0, &H0}
            Case " "c : Return New Byte() {&H0, &H0, &H0, &H0, &H0, &H0, &H0}
            Case "("c : Return New Byte() {&H2, &H4, &H4, &H4, &H4, &H4, &H2}
            Case ")"c : Return New Byte() {&H8, &H4, &H4, &H4, &H4, &H4, &H8}
            Case "."c : Return New Byte() {&H0, &H0, &H0, &H0, &H0, &H0, &H4}
            Case Else : Return New Byte() {&HE, &H11, &H1, &H2, &H4, &H0, &H4}
        End Select
    End Function

    Private Shared Function CreateLogoCubeVertices() As VertexPositionTexture()
        Dim vertices As New List(Of VertexPositionTexture)()

        AddCubeFace(
            vertices,
            New Vector3(-1.0F, 1.0F, 1.0F),
            New Vector3(1.0F, 1.0F, 1.0F),
            New Vector3(1.0F, -1.0F, 1.0F),
            New Vector3(-1.0F, -1.0F, 1.0F))
        AddCubeFace(
            vertices,
            New Vector3(1.0F, 1.0F, -1.0F),
            New Vector3(-1.0F, 1.0F, -1.0F),
            New Vector3(-1.0F, -1.0F, -1.0F),
            New Vector3(1.0F, -1.0F, -1.0F))
        AddCubeFace(
            vertices,
            New Vector3(1.0F, 1.0F, 1.0F),
            New Vector3(1.0F, 1.0F, -1.0F),
            New Vector3(1.0F, -1.0F, -1.0F),
            New Vector3(1.0F, -1.0F, 1.0F))
        AddCubeFace(
            vertices,
            New Vector3(-1.0F, 1.0F, -1.0F),
            New Vector3(-1.0F, 1.0F, 1.0F),
            New Vector3(-1.0F, -1.0F, 1.0F),
            New Vector3(-1.0F, -1.0F, -1.0F))
        AddCubeFace(
            vertices,
            New Vector3(-1.0F, 1.0F, -1.0F),
            New Vector3(1.0F, 1.0F, -1.0F),
            New Vector3(1.0F, 1.0F, 1.0F),
            New Vector3(-1.0F, 1.0F, 1.0F))
        AddCubeFace(
            vertices,
            New Vector3(-1.0F, -1.0F, 1.0F),
            New Vector3(1.0F, -1.0F, 1.0F),
            New Vector3(1.0F, -1.0F, -1.0F),
            New Vector3(-1.0F, -1.0F, -1.0F))

        Return vertices.ToArray()
    End Function

    Private Shared Sub AddCubeFace(
        vertices As List(Of VertexPositionTexture),
        topLeft As Vector3,
        topRight As Vector3,
        bottomRight As Vector3,
        bottomLeft As Vector3)

        vertices.Add(New VertexPositionTexture(topLeft, New Vector2(0.0F, 0.0F)))
        vertices.Add(New VertexPositionTexture(topRight, New Vector2(1.0F, 0.0F)))
        vertices.Add(New VertexPositionTexture(bottomRight, New Vector2(1.0F, 1.0F)))
        vertices.Add(New VertexPositionTexture(topLeft, New Vector2(0.0F, 0.0F)))
        vertices.Add(New VertexPositionTexture(bottomRight, New Vector2(1.0F, 1.0F)))
        vertices.Add(New VertexPositionTexture(bottomLeft, New Vector2(0.0F, 1.0F)))
    End Sub
End Class
