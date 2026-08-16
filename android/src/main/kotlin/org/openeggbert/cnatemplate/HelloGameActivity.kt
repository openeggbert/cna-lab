package org.openeggbert.cnatemplate

import android.os.Bundle
import Microsoft.Xna.Framework.AndroidGameActivity
import com.openeggbert.cna.template.HelloGame

class HelloGameActivity : AndroidGameActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val game = HelloGame()
        setContentView(game.Services.GetService(android.view.View::class.java))
        game.Run()
    }
}
