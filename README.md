# Nokia 2626 hybrid prototype
Unofficial clean-room UI simulation, not original firmware. Java games run in a separately installed J2ME Loader, launched from Games > OK. Import legally obtained JARs in that app; no embedded JVM or automatic import. No real calls, SMS, FM, contact storage, predictive T9 or original assets. Messaging is local multi-tap only; calculator supports integer addition/subtraction. UI is a stylized approximation. No permissions requested.

## Build
Create a GitHub repository, extract this ZIP and upload all files including .github/workflows/build.yml. In Actions select Build Android APK, Run workflow (or push to trigger). Download Nokia2626-debug artifact and extract app-debug.apk. Android may require allowing installation from your file manager. Debug signing only. Locally use JDK 17, Android SDK 35 and Gradle 8.9: `gradle assembleDebug`. No Gradle wrapper included. Build has not been executed in the authoring environment.

## Smoke test
Launch, open Menu, navigate with arrows, select Messages and type using repeated number presses; End returns home. Calculator: 2 * 3 # should show 5. Games > OK opens J2ME Loader or installation prompt. Verify on a physical Android device after build.
