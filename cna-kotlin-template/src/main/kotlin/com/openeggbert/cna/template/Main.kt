package com.openeggbert.cna.template

import kotlin.system.exitProcess

private const val SMOKE_FRAMES = 60
private const val STABILITY_FRAMES = 600

fun main(arguments: Array<String>) {
    val frames = parseFrameLimit(arguments)
    try {
        HelloGame(frames).use { game -> game.Run() }
    } catch (failure: RuntimeException) {
        exitWithFailure(failure)
    } catch (failure: LinkageError) {
        exitWithFailure(failure)
    }
}

private fun exitWithFailure(failure: Throwable): Nothing {
    failure.printStackTrace(System.err)
    exitProcess(1)
}

internal fun parseFrameLimit(arguments: Array<String>): Int {
    var frames = 0
    var index = 0
    while (index < arguments.size) {
        val argument = arguments[index]
        frames = when {
            argument == "--smoke-test" -> SMOKE_FRAMES
            argument == "--stability-test" -> STABILITY_FRAMES
            argument.startsWith("--frames=") -> positiveFrameCount(argument.substringAfter('='))
            argument == "--frames" -> {
                index++
                require(index < arguments.size) { "--frames requires a positive integer" }
                positiveFrameCount(arguments[index])
            }
            else -> throw IllegalArgumentException("Unknown argument: $argument")
        }
        index++
    }
    return frames
}

private fun positiveFrameCount(value: String): Int {
    val frames = value.toIntOrNull() ?: throw IllegalArgumentException("invalid frame count: $value")
    require(frames > 0) { "frame count must be positive: $value" }
    return frames
}
