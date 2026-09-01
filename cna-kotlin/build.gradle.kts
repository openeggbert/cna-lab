import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import org.jetbrains.kotlin.gradle.tasks.KotlinCompile

plugins {
    `java-library`
    kotlin("jvm") version "2.1.10"
    `maven-publish`
}

group = "org.openeggbert"
version = "0.1.0-SNAPSHOT"

val cnaJavaVersion = providers.gradleProperty("cnaJavaVersion").orElse("0.1.0-SNAPSHOT")

java {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
    withSourcesJar()
}

dependencies {
    api("org.openeggbert:cna-java:${cnaJavaVersion.get()}")

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

tasks.withType<AbstractArchiveTask>().configureEach {
    isPreserveFileTimestamps = false
    isReproducibleFileOrder = true
}

val adapterAudit by tasks.registering(Exec::class) {
    group = "verification"
    description = "Audits the adapter source, artifact ownership, API boundary, and Maven metadata."
    dependsOn(tasks.jar, "generatePomFileForMavenJavaPublication")
    doFirst {
        commandLine(
            "python3",
            layout.projectDirectory.file("scripts/audit-adapter.py").asFile,
            "--jar",
            tasks.jar.get().archiveFile.get().asFile,
            "--source-root",
            layout.projectDirectory.dir("src/main/kotlin").asFile,
            "--pom",
            layout.buildDirectory.file("publications/mavenJava/pom-default.xml").get().asFile,
            "--classpath",
            configurations.compileClasspath.get().asPath,
        )
    }
}

tasks.check {
    dependsOn(adapterAudit)
}

publishing {
    publications {
        create<MavenPublication>("mavenJava") {
            from(components["java"])
            pom {
                name.set("CNA Kotlin")
                description.set("Thin Kotlin/JVM ergonomic extensions over CNA-Java")
                url.set("https://github.com/openeggbert/cna-kotlin")
                licenses {
                    license {
                        name.set("Microsoft Public License")
                        url.set("https://opensource.org/license/ms-pl-html")
                    }
                }
            }
        }
    }
}
