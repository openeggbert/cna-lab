plugins {
    kotlin("jvm")
    application
}

application {
    mainClass.set("com.openeggbert.cna.template.MainKt")
}

dependencies {
    implementation(project(":game"))
    implementation("org.openeggbert.cna:cna-kotlin-desktop:0.1.0-SNAPSHOT")
}
