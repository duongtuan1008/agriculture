buildscript {
    repositories {
        google()
        mavenCentral()
        // jcenter() // nếu bạn cần thư viện cũ
    }

    dependencies {
        classpath ("com.android.tools.build:gradle:8.1.1")
        // Nếu bạn dùng Kotlin:
        // classpath "org.jetbrains.kotlin:kotlin-gradle-plugin:1.8.10"
    }
}

allprojects {
    repositories {
        google()
        mavenCentral()
    }
}
