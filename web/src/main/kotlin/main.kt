import com.openeggbert.cna.template.HelloGame
import kotlinx.browser.window

fun main() {
    val smokeTest = window.location.search.contains("smoke-test")
    val game = HelloGame(smokeTest)
    game.Run()
}
