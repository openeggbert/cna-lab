plugins {
    id("com.android.application")
    kotlin("android")
}

android {
    namespace = "org.openeggbert.cnatemplate"
    compileSdk = 33

    defaultConfig {
        applicationId = "org.openeggbert.cnatemplate"
        minSdk = 21
        targetSdk = 33
        versionCode = 1
        versionName = "1.0"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
    
    kotlinOptions {
        jvmTarget = "1.8"
    }
}

dependencies {
    implementation(project(":game"))
    implementation("org.openeggbert.cna:cna-kotlin-android:0.1.0-SNAPSHOT")
}
