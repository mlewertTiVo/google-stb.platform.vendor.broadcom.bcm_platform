# [1] create the apk shell with the new version.

${ANDROID}/out/host/linux-x86/bin/aapt package -M AndroidManifest.xml --version-code ${GPU_VERSION_UPDATE} --version-name ${GPU_NAME_UPDATE} -F gfxdriver-bcmstb-update -I ${ANDROID}/prebuilts/sdk/current/android.jar

# [2] package in the libGLES_nexus.so (for all ARCHes) into the apk

find ${ANDROID_OUT}/system -name libGLES_nexus.so | xargs zip -q0 gfxdriver-bcmstb-update.apk

# [3] align...

${ANDROID}/out/host/linux-x86/bin/zipalign -p 4 gfxdriver-bcmstb-update.apk gfxdriver-bcmstb-update-aligned.apk

# [4] sign using the driver jks, see the build.gradle for default values (password).

apksigner sign --ks gfxdriver-bcmstb.jks gfxdriver-bcmstb-update-aligned.apk

# [5] adb install ([-r]: replace).

adb install -r gfxdriver-bcmstb-update-aligned.apk

