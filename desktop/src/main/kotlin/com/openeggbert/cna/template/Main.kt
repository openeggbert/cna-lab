package com.openeggbert.cna.template

fun main(args: Array<String>) {
    val smokeTest = args.contains("--smoke-test")
    HelloGame(smokeTest).use { game ->
        game.Run()
    }
}
