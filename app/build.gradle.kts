plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.tracostudio.dreams"
    compileSdk = 34
    ndkVersion = "26.3.11579264"

    defaultConfig {
        applicationId = "com.tracostudio.dreams"
        minSdk = 26          // OpenGL ES 3.0 garantido
        targetSdk = 34
        versionCode = 1
        versionName = "0.1.0-native-skeleton"

        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.4")
    testImplementation("junit:junit:4.13.2")
}
