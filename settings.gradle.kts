pluginManagement {
    repositories {
        gradlePluginPortal()
        mavenCentral()
    }
}

dependencyResolutionManagement {
    repositories {
        maven {
            url = uri(
                providers.gradleProperty("cnaRepository")
                    .orElse("${rootDir}/.cna-repository")
                    .get(),
            )
        }
        mavenCentral()
    }
}

rootProject.name = "cna-kotlin"
