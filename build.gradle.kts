import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import org.jetbrains.kotlin.gradle.tasks.KotlinCompile

plugins {
    kotlin("jvm") version "2.1.10"
    application
}

group = providers.gradleProperty("projectGroup").getOrElse("org.openeggbert.examples")
version = "0.1.0-SNAPSHOT"

base {
    archivesName.set(providers.gradleProperty("artifactId").getOrElse("cna-kotlin-template"))
}

java {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
}

dependencies {
    implementation(
        "org.openeggbert:cna-kotlin:" +
            providers.gradleProperty("cnaKotlinVersion").getOrElse("0.1.0-SNAPSHOT"),
    )

    testImplementation(platform("org.junit:junit-bom:5.11.4"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

tasks.withType<JavaCompile>().configureEach {
    options.release = 17
    options.encoding = "UTF-8"
}

tasks.withType<KotlinCompile>().configureEach {
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_17)
        allWarningsAsErrors.set(true)
        freeCompilerArgs.add("-Xjsr305=strict")
    }
}

tasks.test {
    useJUnitPlatform()
}

application {
    mainClass.set(
        providers.gradleProperty("applicationMainClass")
            .getOrElse("com.openeggbert.cna.template.MainKt"),
    )
}
