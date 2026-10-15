plugins { alias(libs.plugins.kotlin.jvm) }
java { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
kotlin { compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17) } }
dependencies {
    implementation("com.google.code.gson:gson:2.10.1")
    testImplementation(libs.junit)
}
tasks.test { systemProperty("firmwareFixtures", rootProject.file("../contract/firmware-v2/fixtures").absolutePath) }

tasks.test { systemProperty("ledFixtures", rootProject.file("../../docs/led/fixtures").absolutePath) }
