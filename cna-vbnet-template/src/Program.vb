Imports System
Imports System.Runtime.CompilerServices

Module Program
    <STAThread>
    Public Function Main(args As String()) As Integer
        Try
            RunGame(ResolveFrameLimit(args))
            Return 0
        Catch exception As ArgumentException
            Console.Error.WriteLine(exception.Message)
            Return 2
        Catch exception As DllNotFoundException
            Console.Error.WriteLine($"Native CNA dependency could not be loaded: {exception.Message}")
            Console.Error.WriteLine("Set CNA_NATIVE_LIBRARY or CNA_NATIVE_DIR to a compatible CNA C ABI library.")
            Return 2
        End Try
    End Function

    <MethodImpl(MethodImplOptions.NoInlining)>
    Private Sub RunGame(frameLimit As Integer?)
        Using game As New HelloGame(frameLimit)
            game.Run()
        End Using
    End Sub

    Private Function ResolveFrameLimit(args As String()) As Integer?
        Dim smokeTest As Boolean = False
        Dim stabilityTest As Boolean = False
        Dim explicitFrames As Integer? = Nothing
        Dim index As Integer = 0

        While index < args.Length
            Select Case args(index)
                Case "--smoke-test"
                    smokeTest = True
                    index += 1
                Case "--stability-test"
                    stabilityTest = True
                    index += 1
                Case "--frames"
                    If index + 1 >= args.Length Then
                        Throw New ArgumentException("--frames requires a positive integer.")
                    End If

                    Dim value As Integer
                    If Not Integer.TryParse(args(index + 1), value) OrElse value <= 0 Then
                        Throw New ArgumentException("--frames requires a positive integer.")
                    End If

                    explicitFrames = value
                    index += 2
                Case Else
                    Throw New ArgumentException($"Unknown argument: {args(index)}")
            End Select
        End While

        If explicitFrames.HasValue Then
            Return explicitFrames
        End If

        If stabilityTest Then
            Return ReadEnvironmentFrameLimit(600)
        End If

        If smokeTest Then
            Return ReadEnvironmentFrameLimit(60)
        End If

        Return Nothing
    End Function

    Private Function ReadEnvironmentFrameLimit(defaultValue As Integer) As Integer
        Dim value As Integer
        If Integer.TryParse(Environment.GetEnvironmentVariable("CNA_SMOKE_FRAMES"), value) AndAlso value > 0 Then
            Return value
        End If

        Return defaultValue
    End Function
End Module
