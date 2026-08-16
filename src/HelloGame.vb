Imports System
Imports Microsoft.Xna.Framework
Imports Microsoft.Xna.Framework.Graphics
Imports Microsoft.Xna.Framework.Input

Namespace CNA.VB.Template
    Public Class HelloGame
        Inherits Game

        Private _graphics As GraphicsDeviceManager
        Private _spriteBatch As SpriteBatch
        Private _logo As Texture2D
        Private _solid As Texture2D
        Private _cubeEffect As BasicEffect
        
        Private ReadOnly _smokeTest As Boolean
        Private _drawnFrames As UInteger = 0
        Private _animationSeconds As Single = 0
        Private _rendererBannerSeconds As Single = 5.0
        
        Private _velocity As New Vector2(104, 74)
        Private _position As Vector2
        Private _supports3D As Boolean = False
        
        Private Const RendererName As String = "CNA (VB.NET)"

        Public Sub New(smokeTest As Boolean)
            _smokeTest = smokeTest
            _graphics = New GraphicsDeviceManager(Me)
        End Sub

        Protected Overrides Sub Initialize()
            _supports3D = GraphicsDevice.SupportsCapability(GraphicsCapability.ThreeD)
            
            Dim viewport = GraphicsDevice.Viewport
            _position = New Vector2(viewport.Width / 2.0F, viewport.Height / 2.0F)
            
            Console.WriteLine($"{RendererName}: renderer initialized")
            MyBase.Initialize()
        End Sub

        Protected Overrides Sub LoadContent()
            _spriteBatch = New SpriteBatch(GraphicsDevice)
            
            ' Mock content loading
            _logo = New Texture2D()
            _solid = New Texture2D()
            
            If _supports3D Then
                _cubeEffect = New BasicEffect(GraphicsDevice)
                _cubeEffect.TextureEnabled = True
                _cubeEffect.Texture = _logo
            End If
        End Sub

        Protected Overrides Sub Update(gameTime As GameTime)
            Dim dt = CSng(gameTime.ElapsedGameTime.TotalSeconds)
            _animationSeconds += dt
            
            If Keyboard.GetState().IsKeyDown(Keys.Escape) Then
                ExitGame()
            End If
            
            If Not _supports3D Then
                Dim movementDelta = dt * 2.0F
                _position.X += _velocity.X * movementDelta
                _position.Y += _velocity.Y * movementDelta
                
                Dim viewport = GraphicsDevice.Viewport
                Dim logoSize As Single = 256.0F
                
                Dim minX = logoSize / 2
                Dim minY = logoSize / 2
                Dim maxX = viewport.Width - minX
                Dim maxY = viewport.Height - minY
                
                If _position.X < minX OrElse _position.X > maxX Then
                    _velocity.X *= -1
                    _position.X = Math.Clamp(_position.X, minX, maxX)
                End If
                If _position.Y < minY OrElse _position.Y > maxY Then
                    _velocity.Y *= -1
                    _position.Y = Math.Clamp(_position.Y, minY, maxY)
                End If
            End If
            
            If _rendererBannerSeconds > 0 Then
                _rendererBannerSeconds -= dt
            End If
            
            MyBase.Update(gameTime)
        End Sub

        Protected Overrides Sub Draw(gameTime As GameTime)
            GraphicsDevice.Clear(Color.CornflowerBlue)
            
            If _supports3D Then
                Draw3DCube()
            Else
                Draw2DLogo()
            End If
            
            If _rendererBannerSeconds > 0 Then
                DrawRendererBanner()
            End If
            
            _drawnFrames += 1
            If _smokeTest AndAlso _drawnFrames >= 10 Then
                Console.WriteLine($"Smoke test passed ({_drawnFrames} frames)")
                ExitGame()
            End If
            
            MyBase.Draw(gameTime)
        End Sub

        Private Sub Draw2DLogo()
            If _spriteBatch Is Nothing OrElse _logo Is Nothing Then Return
            
            Dim motion = _animationSeconds * 2.0F
            Dim scale = 0.96F + 0.12F * CSng(Math.Sin(motion))
            
            _spriteBatch.Begin()
            _spriteBatch.Draw(_logo, _position, Color.White)
            _spriteBatch.End()
        End Sub

        Private Sub Draw3DCube()
            If _cubeEffect Is Nothing Then Return
            
            Dim viewport = GraphicsDevice.Viewport
            Dim aspect = CSng(viewport.Width) / CSng(viewport.Height)
            
            Dim motion = _animationSeconds * 2.0F
            Dim scale = 0.88F + 0.10F * CSng(Math.Sin(motion * 0.48F))
            
            Dim world = Matrix.CreateScale(scale)
            world *= Matrix.CreateRotationY(motion * 0.55F)
            world *= Matrix.CreateRotationX(motion * 0.35F)
            
            _cubeEffect.World = world
            _cubeEffect.View = Matrix.CreateLookAt(New Vector3(0, 0, 6), Vector3.Zero, Vector3.Up)
            _cubeEffect.Projection = Matrix.CreatePerspectiveFieldOfView(0.7853982F, aspect, 0.1F, 100.0F)
            
            _cubeEffect.Apply()
            ' Primitive drawing would happen here
        End Sub

        Private Sub DrawRendererBanner()
            If _spriteBatch Is Nothing OrElse _solid Is Nothing Then Return
            Dim viewport = GraphicsDevice.Viewport
            
            Dim name = RendererName.ToUpper()
            Dim glyphCols = (name.Length * 6) - 1
            Dim pixelSize = Math.Max(1, Math.Min(8, (viewport.Width - 48) \ Math.Max(1, glyphCols)))
            
            Dim textW = glyphCols * pixelSize
            Dim textH = 7 * pixelSize
            Dim textX = (viewport.Width - textW) \ 2
            Dim textY = viewport.Height - textH - 24
            
            _spriteBatch.Begin()
            ' Background
            _spriteBatch.DrawRect(_solid, {CSng(textX - 8), CSng(textY - 8), CSng(textW + 16), CSng(textH + 16)}, New Color(255, 255, 255, 180))
            
            For i As Integer = 0 To name.Length - 1
                Dim rows = GetGlyphRows(name(i))
                Dim charX = textX + i * 6 * pixelSize
                For row As Integer = 0 To 6
                    Dim rowData = rows(row)
                    For col As Integer = 0 To 4
                        If (rowData >> (4 - col) And 1) = 1 Then
                            _spriteBatch.DrawRect(_solid, {CSng(charX + col * pixelSize), CSng(textY + row * pixelSize), CSng(pixelSize), CSng(pixelSize)}, Color.Black)
                        End If
                    Next
                Next
            Next
            _spriteBatch.End()
        End Sub

        Private Function GetGlyphRows(c As Char) As Byte()
            Select Case c
                Case "A"c : Return {&H4, &HA, &H11, &H11, &H1F, &H11, &H11}
                Case "B"c : Return {&H1E, &H11, &H11, &H1E, &H11, &H11, &H1E}
                Case "C"c : Return {&HE, &H11, &H10, &H10, &H10, &H11, &HE}
                Case "D"c : Return {&H1C, &H12, &H11, &H11, &H11, &H12, &H1C}
                Case "E"c : Return {&H1F, &H10, &H10, &H1E, &H10, &H10, &H1F}
                Case "F"c : Return {&H1F, &H10, &H10, &H1E, &H10, &H10, &H10}
                Case "G"c : Return {&HE, &H11, &H10, &H17, &H11, &H11, &HF}
                Case "H"c : Return {&H11, &H11, &H11, &H1F, &H11, &H11, &H11}
                Case "I"c : Return {&HE, &H4, &H4, &H4, &H4, &H4, &HE}
                Case "J"c : Return {&H7, &H2, &H2, &H2, &H2, &H12, &HC}
                Case "K"c : Return {&H11, &H12, &H14, &H18, &H14, &H12, &H11}
                Case "L"c : Return {&H10, &H10, &H10, &H10, &H10, &H10, &H1F}
                Case "M"c : Return {&H11, &H1B, &H15, &H15, &H11, &H11, &H11}
                Case "N"c : Return {&H11, &H11, &H19, &H15, &H13, &H11, &H11}
                Case "O"c : Return {&HE, &H11, &H11, &H11, &H11, &H11, &HE}
                Case "P"c : Return {&H1E, &H11, &H11, &H1E, &H10, &H10, &H10}
                Case "Q"c : Return {&HE, &H11, &H11, &H11, &H15, &H12, &HD}
                Case "R"c : Return {&H1E, &H11, &H11, &H1E, &H14, &H12, &H11}
                Case "S"c : Return {&HF, &H10, &H10, &HE, &H1, &H1, &H1E}
                Case "T"c : Return {&H1F, &H4, &H4, &H4, &H4, &H4, &H4}
                Case "U"c : Return {&H11, &H11, &H11, &H11, &H11, &H11, &HE}
                Case "V"c : Return {&H11, &H11, &H11, &H11, &H11, &HA, &H4}
                Case "W"c : Return {&H11, &H11, &H11, &H15, &H15, &H1B, &H11}
                Case "X"c : Return {&H11, &H11, &HA, &H4, &HA, &H11, &H11}
                Case "Y"c : Return {&H11, &H11, &HA, &H4, &H4, &H4, &H4}
                Case "Z"c : Return {&H1F, &H1, &H2, &H4, &H8, &H10, &H1F}
                Case "("c : Return {&H2, &H4, &H8, &H8, &H8, &H4, &H2}
                Case ")"c : Return {&H8, &H4, &H2, &H2, &H2, &H4, &H8}
                Case " "c : Return {0, 0, 0, 0, 0, 0, 0}
                Case Else : Return {&H1F, &H1F, &H1F, &H1F, &H1F, &H1F, &H1F}
            End Select
        End Function
    End Class
End Namespace
