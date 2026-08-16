plugins {
    kotlin("js")
}

kotlin {
    js(IR) {
        browser {
            commonWebpackConfig {
                cssSupport {
                    enabled.set(true)
                }
            }
        }
        binaries.executable()
    }
}

dependencies {
    implementation(project(":game"))
    implementation("org.openeggbert.cna:cna-kotlin-js:0.1.0-SNAPSHOT")
}
