plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}
android {
    namespace = "de.larpescape.companion"
    compileSdk = 35
    buildToolsVersion = "35.0.0"
    defaultConfig { applicationId = "de.larpescape.companion"; minSdk = 33; targetSdk = 35; versionCode = 2; versionName = "0.2" }
    buildFeatures { compose = true; buildConfig = true }
    buildTypes {
        getByName("debug") { buildConfigField("boolean", "ALLOW_CONTRACT_LAB", "true") }
        getByName("release") { buildConfigField("boolean", "ALLOW_CONTRACT_LAB", "false") }
    }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    kotlinOptions { jvmTarget = "17" }
}
dependencies {
    implementation(project(":core:protocol"))
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.material3)
    implementation(libs.activity.compose)
    implementation(libs.lifecycle.compose)
    implementation(libs.lifecycle.viewmodel)
    implementation(libs.coroutines)
    testImplementation(libs.junit)
}
