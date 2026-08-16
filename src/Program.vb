Imports System
Imports System.Linq

Namespace CNA.VB.Template
    Module Program
        Sub Main(args As String())
            Dim smokeTest As Boolean = args.Contains("--smoke-test")
            Using game As New HelloGame(smokeTest)
                game.Run()
            End Using
        End Sub
    End Module
End Namespace
