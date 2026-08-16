plugins {
    kotlin("multiplatform")
}

kotlin {
    jvm()
    js(IR) {
        browser()
    }
    
    sourceSets {
        val commonMain by getting {
            dependencies {
                // Placeholder for CNA Kotlin common binding
                implementation("org.openeggbert.cna:cna-kotlin-common:0.1.0-SNAPSHOT")
            }
        }
        val jvmMain by getting
        val jsMain by getting
    }
}
