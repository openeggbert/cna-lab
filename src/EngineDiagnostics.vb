Imports CNA.XnaCompat.Extensions
Imports Microsoft.Xna.Framework.Graphics

''' <summary>Keeps CNA-specific diagnostics outside the portable game implementation.</summary>
Friend NotInheritable Class EngineDiagnostics
    Private Sub New()
    End Sub

    Friend Structure Capabilities
        Friend Sub New(rendererName As String, supports3D As Boolean, supportsDepth As Boolean)
            Me.RendererName = rendererName
            Me.Supports3D = supports3D
            Me.SupportsDepth = supportsDepth
        End Sub

        Friend ReadOnly Property RendererName As String
        Friend ReadOnly Property Supports3D As Boolean
        Friend ReadOnly Property SupportsDepth As Boolean
    End Structure

    Friend Shared Function Inspect(graphicsDevice As GraphicsDevice) As Capabilities
        Return New Capabilities(
            graphicsDevice.GetCnaRendererName(),
            graphicsDevice.SupportsCnaCapability(CnaGraphicsCapability.ThreeD),
            graphicsDevice.SupportsCnaCapability(CnaGraphicsCapability.DepthStencilBuffer))
    End Function
End Class
