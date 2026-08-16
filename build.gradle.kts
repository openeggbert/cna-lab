plugins {
    kotlin("jvm") version "1.9.10" apply false
    id("com.android.application") version "8.1.1" apply false
    kotlin("android") version "1.9.10" apply false
    kotlin("js") version "1.9.10" apply false
}

allprojects {
    repositories {
        google()
        mavenCentral()
        mavenLocal()
        maven { url = uri("https://maven.openeggbert.org/repository/maven-public/") }
    }
}
