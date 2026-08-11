# PACE signing for the suite's AAX bundles.
#
# Retail Pro Tools only loads PACE-signed AAX, and the JUCE copy step
# installs an unsigned bundle on every build - so each plugin re-signs its
# installed bundle in place afterwards. Signing needs an open iLok Cloud
# session in iLok License Manager (the PACE account password is cached in
# the login keychain). Skipped automatically where wraptool isn't
# installed (e.g. CI, non-Mac); CI release signing will instead use PACE
# cloud signing credentials from repository secrets.

option(SIGN_AAX_AFTER_BUILD "PACE-sign the installed AAX bundle after each build" ON)
set(AAX_WRAPTOOL "/Applications/PACEAntiPiracy/Eden/Fusion/Versions/6/bin/wraptool"
    CACHE FILEPATH "PACE wraptool binary (iLok Tools SDK)")
set(AAX_SIGN_ACCOUNT "kogzee" CACHE STRING "PACE Central account")
set(AAX_SIGN_WCGUID "FCB93630-951E-11F1-9B0E-00505692AD3E"
    CACHE STRING "PACE wrap config GUID (product: Data Cogs Plugins; shared by all plugins)")
set(AAX_SIGN_ID "Developer ID Application: Mark Daunt (74N92K2DPP)"
    CACHE STRING "Apple Developer ID identity for the codesign layer")

# Call with the plugin's base target name (e.g. CompressorPlugin) after
# juce_add_plugin. No-op unless the AAX target exists and wraptool is
# present on this machine.
function(datacogs_sign_aax plugin_target)
    if(APPLE AND SIGN_AAX_AFTER_BUILD
       AND TARGET ${plugin_target}_AAX AND EXISTS "${AAX_WRAPTOOL}")
        add_custom_command(TARGET ${plugin_target}_AAX POST_BUILD
            COMMAND "${AAX_WRAPTOOL}" sign
                    --account "${AAX_SIGN_ACCOUNT}"
                    --wcguid "${AAX_SIGN_WCGUID}"
                    --signid "${AAX_SIGN_ID}"
                    --in "$<TARGET_BUNDLE_DIR:${plugin_target}_AAX>"
                    --out "/Library/Application Support/Avid/Audio/Plug-Ins/$<TARGET_BUNDLE_DIR_NAME:${plugin_target}_AAX>"
            COMMENT "PACE-signing installed AAX bundle"
            VERBATIM)
    endif()
endfunction()
