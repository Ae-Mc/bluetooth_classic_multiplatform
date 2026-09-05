# bluetooth_classic_multiplatform

Checkout [flutter_blue_classic](https://pub.dev/packages/flutter_blue_classic), origin of this package.

## Android build requirements

Requires Flutter 3.47 or later, Dart 3.13 or later, and JDK 17 or later.
Android apps must use Android Gradle Plugin 9.2.1 or later with a compatible
Gradle version (the example uses Gradle 9.6.0).

The plugin uses built-in Kotlin. Migrate the host app using the
[Flutter migration guide](https://docs.flutter.dev/release/breaking-changes/migrate-to-built-in-kotlin/for-app-developers),
remove the standalone Kotlin Android plugin, and set these properties in
the app's `android/gradle.properties`:

```properties
android.builtInKotlin=true
android.newDsl=false
```

`android.newDsl=false` retains the Android DSL used by Flutter; it does not
disable built-in Kotlin.

If AGP resolves a Kotlin compiler older than Flutter's minimum, upgrade its
runtime dependency in `android/settings.gradle.kts`, after `pluginManagement`
and before `plugins` (as in the example):

```kotlin
buildscript {
    repositories {
        google()
        mavenCentral()
    }
    dependencies {
        classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:2.3.21")
    }
}
```

This selects the compiler used by built-in Kotlin without applying
`org.jetbrains.kotlin.android`.

To verify the example, run `flutter build apk --debug` from `example/`.

