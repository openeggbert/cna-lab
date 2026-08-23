Imports System
Imports System.IO
Imports System.Reflection
Imports CnaVbTemplate
Imports Microsoft.Xna.Framework
Imports Microsoft.Xna.Framework.Audio
Imports Microsoft.Xna.Framework.Content
Imports Microsoft.Xna.Framework.Graphics
Imports Microsoft.Xna.Framework.Input

Module Program
    Public Function Main() As Integer
        If GetType(HelloGame).FullName <> "CnaVbTemplate.HelloGame" Then
            Console.Error.WriteLine($"Unexpected HelloGame namespace: {GetType(HelloGame).FullName}")
            Return 1
        End If

        If GetType(HelloGame).Assembly.GetName().Name <> "CnaVbTemplate" Then
            Console.Error.WriteLine($"Unexpected assembly name: {GetType(HelloGame).Assembly.GetName().Name}")
            Return 1
        End If

        If Not GetType(Game).IsAssignableFrom(GetType(HelloGame)) Then
            Console.Error.WriteLine("HelloGame does not inherit Microsoft.Xna.Framework.Game.")
            Return 1
        End If

        Dim flags As BindingFlags = BindingFlags.Instance Or BindingFlags.NonPublic
        If GetType(ProbeGame).GetMethod("LoadContent", flags) Is Nothing OrElse
           GetType(ProbeGame).GetMethod("Update", flags) Is Nothing OrElse
           GetType(ProbeGame).GetMethod("Draw", flags) Is Nothing Then
            Console.Error.WriteLine("Representative VB.NET Game overrides were not emitted.")
            Return 1
        End If

        Console.WriteLine("VB compile probe passed: CnaVbTemplate.HelloGame")
        Return 0
    End Function
End Module

Friend NotInheritable Class ProbeGame
    Inherits Game

    Private ReadOnly _graphics As GraphicsDeviceManager

    Public Sub New()
        _graphics = New GraphicsDeviceManager(Me)
        AddHandler Exiting, AddressOf HandleExiting
    End Sub

    Protected Overrides Sub Initialize()
        Dim viewport As Viewport = GraphicsDevice.Viewport
        Dim width As Integer = viewport.Width
        MyBase.Initialize()
    End Sub

    Protected Overrides Sub LoadContent()
        MyBase.LoadContent()
    End Sub

    Protected Overrides Sub Update(gameTime As GameTime)
        Dim keyboardState As KeyboardState = Keyboard.GetState()
        Dim mouseState As MouseState = Mouse.GetState()
        Dim gamePadState As GamePadState = GamePad.GetState(PlayerIndex.One)
        MyBase.Update(gameTime)
    End Sub

    Protected Overrides Sub Draw(gameTime As GameTime)
        GraphicsDevice.Clear(Color.CornflowerBlue)
        MyBase.Draw(gameTime)
    End Sub

    Protected Overrides Sub UnloadContent()
        MyBase.UnloadContent()
    End Sub

    Private Shared Sub HandleExiting(sender As Object, args As EventArgs)
    End Sub

    ' This method is intentionally not executed. Its body is a source-compatibility corpus that
    ' makes the VB compiler resolve representative strict XNA types, generics, and overloads.
    Private Shared Function CompileXnaSurface(
        game As Game,
        device As GraphicsDevice,
        content As ContentManager,
        stream As Stream) As GraphicsResource

        Dim manager As New GraphicsDeviceManager(game)
        Dim texture As Texture2D = Texture2D.FromStream(device, stream)
        Dim resource As GraphicsResource = texture
        Dim destination As New Rectangle(10, 20, texture.Width, texture.Height)

        Using spriteBatch As New SpriteBatch(device)
            spriteBatch.Begin()
            spriteBatch.Draw(texture, New Vector2(10.0F, 20.0F), Color.White)
            spriteBatch.Draw(texture, destination, Color.White)
            spriteBatch.End()
        End Using

        Using effect As New BasicEffect(device)
            effect.World = Matrix.Identity
            effect.View = Matrix.Identity
            effect.Projection = Matrix.Identity
        End Using

        Dim model As Model = content.Load(Of Model)("model")
        Dim sound As SoundEffect = content.Load(Of SoundEffect)("sound")
        Dim vertex As New VertexPositionColor(Vector3.Zero, Color.White)
        Dim texturedVertices As VertexPositionTexture() = {
            New VertexPositionTexture(Vector3.Zero, Vector2.Zero),
            New VertexPositionTexture(Vector3.UnitX, Vector2.UnitX),
            New VertexPositionTexture(Vector3.UnitY, Vector2.UnitY)
        }
        Dim primitiveType As PrimitiveType = PrimitiveType.TriangleList
        Dim keyboardState As KeyboardState = Keyboard.GetState()
        Dim mouseState As MouseState = Mouse.GetState()
        Dim gamePadState As GamePadState = GamePad.GetState(PlayerIndex.One)

        AddHandler resource.Disposing, AddressOf HandleExiting
        device.DrawUserPrimitives(primitiveType, texturedVertices, 0, 1)
        Return resource
    End Function
End Class
