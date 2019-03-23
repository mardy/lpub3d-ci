#!/bin/bash
# Trevor SANDY
# Last Update March 23, 2019
# Copyright (c) 2018 - 2019 by Trevor SANDY
# LPub3D Unix checks - for remote CI (Trevis, OBS)
# NOTE: Source with variables as appropriate:
#       $BUILD_OPT = compile
#       $XMING = true,
#       $LP3D_BUILD_APPIMAGE = true,
#       $SOURCE_DIR = <lpub3d source folder>

# Initialize platform variables
LP3D_OS_NAME=$(uname)
LP3D_TARGET_ARCH="$(uname -m)"
LP3D_PLATFORM=$(. /etc/os-release 2>/dev/null; [ -n "$ID" ] && echo $ID || echo $OS_NAME | awk '{print tolower($0)}')

# Set XDG_RUNTIME_DIR
if [[ "${LP3D_OS_NAME}" != "Darwin" && $UID -ge 1000 && -z "$(printenv | grep XDG_RUNTIME_DIR)" ]]
then
    runtime_dir="/tmp/runtime-user-$UID"
    if [ ! -d "$runtime_dir" ]; then
       mkdir -p $runtime_dir
    fi
    export XDG_RUNTIME_DIR="$runtime_dir"
fi

# Initialize XVFB
if [[ "${XMING}" != "true" && ("${DOCKER}" = "true" || ("${LP3D_OS_NAME}" != "Darwin")) ]]; then
    echo && echo "- Using XVFB from working directory: ${PWD}"
    USE_XVFB="true"
fi

# Interim workaround for LDView Single Call fail on Arch and Fedora Linux
if [[ "${LP3D_PLATFORM}" = "arch" || "${LP3D_PLATFORM}" = "fedora" ]]; then
    LP3D_RENDERER3="ldview"
    LP3D_RENDERER7="ldview"
else
    LP3D_RENDERER3="ldview-sc"
    LP3D_RENDERER7="ldview-scsl"
fi

# macOS, compile only, initialize build paths and libraries
if [[ "${LP3D_OS_NAME}" = "Darwin" && "$BUILD_OPT" = "compile" ]]; then
    cd ${SOURCE_DIR}/mainApp/64bit_release

    echo "- set macOS LPub3D executable..."
    LPUB3D_EXE="LPub3D.app/Contents/MacOS/LPub3D"

    echo "- install library links..."
    /usr/bin/install_name_tool -id @executable_path/../Libs/libLDrawIni.16.dylib LPub3D.app/Contents/Libs/libLDrawIni.16.dylib
    /usr/bin/install_name_tool -id @executable_path/../Libs/libQuaZIP.0.dylib LPub3D.app/Contents/Libs/libQuaZIP.0.dylib

    echo "- change mapping to LPub3D..."
    /usr/bin/install_name_tool -change libLDrawIni.16.dylib @executable_path/../Libs/libLDrawIni.16.dylib LPub3D.app/Contents/MacOS/LPub3D
    /usr/bin/install_name_tool -change libQuaZIP.0.dylib @executable_path/../Libs/libQuaZIP.0.dylib LPub3D.app/Contents/MacOS/LPub3D

    echo "- bundle LPub3D..."
    macdeployqt LPub3D.app -verbose=1 -executable=LPub3D.app/Contents/MacOS/LPub3D -always-overwrite

    echo "- change library dependency mapping..."
    /usr/bin/install_name_tool -change libLDrawIni.16.dylib @executable_path/../Libs/libLDrawIni.16.dylib LPub3D.app/Contents/Frameworks/QtCore.framework/Versions/5/QtCore
    /usr/bin/install_name_tool -change libQuaZIP.0.dylib @executable_path/../Libs/libQuaZIP.0.dylib LPub3D.app/Contents/Frameworks/QtCore.framework/Versions/5/QtCore
fi

# AppImage execute permissions
if [ "$LP3D_BUILD_APPIMAGE" = "true" ]; then
    cd ${SOURCE_DIR}

    echo && echo "- set AppImage $(realpath ${LPUB3D_EXE}) execute permissions..."
    [ -f "${LPUB3D_EXE}" ] && \
    chmod u+x ${LPUB3D_EXE} || \
    echo "ERROR - $(realpath ${LPUB3D_EXE}) not found."
fi

# Log file paths
#if [ "${LP3D_OS_NAME}" = "Darwin" ]; then
#    LP3D_LOG_FILE="~/Library/Application Support/LPub3D Software/LPub3D/logs/LPub3DLog.txt"
#elif [ "$LP3D_BUILD_APPIMAGE" = "true" ]; then
#    LP3D_LOG_FILE="~/.local/share/LPub3D Software/${LPUB3D_EXE}/logs/LPub3DLog.txt"
#else
#    LP3D_LOG_FILE="~/.local/share/LPub3D Software/LPub3D/logs/LPub3DLog.txt"
#fi

# Initialize variables
LP3D_CHECK_FILE="$(realpath ${SOURCE_DIR})/builds/check/build_checks.mpd"
LP3D_CHECK_SUCCESS="Application terminated with return code 0."
LP3D_LOG_FILE="Check.out"
let LP3D_CHECK_PASS=0
let LP3D_CHECK_FAIL=0
LP3D_CHECKS_PASS=
LP3D_CHECKS_FAIL=

echo && echo "------------Build checks start--------------" && echo

# Remove old log if exist
if [ -e "${LP3D_LOG_FILE}" ]; then
    rm -rf "${LP3D_LOG_FILE}"
fi

for LP3D_BUILD_CHECK in CHECK01 CHECK02 CHECK03 CHECK04 CHECK05 CHECK06 CHECK07; do
    case ${LP3D_BUILD_CHECK} in
    CHECK01)
        LP3D_CHECK_HDR="- Check 1 of 7: Native File Process Check..."
        LP3D_CHECK_OPTIONS="--no-stdout-log --process-file --liblego --preferred-renderer native"
        ;;
    CHECK02)
        LP3D_CHECK_HDR="- Check 2 of 7: LDView File Process Check..."
        LP3D_CHECK_OPTIONS="--no-stdout-log --process-file --clear-cache --liblego --preferred-renderer ldview"
        ;;
    CHECK03)
        LP3D_CHECK_HDR="- Check 3 of 7: LDView (Single Call) File Process Check..."
        LP3D_CHECK_OPTIONS="--no-stdout-log --process-file --clear-cache --liblego --preferred-renderer ${LP3D_RENDERER3}"
        ;;
    CHECK04)
        LP3D_CHECK_HDR="- Check 4 of 7: LDGLite Export Range Check..."
        LP3D_CHECK_OPTIONS="--no-stdout-log --process-export --range 1-3 --clear-cache --liblego --preferred-renderer ldglite"
        ;;
    CHECK05)
        LP3D_CHECK_HDR="- Check 5 of 7: Native POV Generation Check..."
        LP3D_CHECK_OPTIONS="--no-stdout-log --process-file --clear-cache --liblego --preferred-renderer povray"
        ;;
    CHECK06)
        LP3D_CHECK_HDR="- Check 6 of 7: LDView TENTE Model Check..."
        LP3D_CHECK_OPTIONS="--no-stdout-log --process-file --clear-cache --libtente --preferred-renderer ldview"
        LP3D_CHECK_FILE="$(realpath ${SOURCE_DIR})/builds/check/TENTE/astromovil.ldr"
        ;;
    CHECK07)
        LP3D_CHECK_HDR="- Check 7 of 7: Native VEXIQ Model Check..."
        LP3D_CHECK_OPTIONS="--no-stdout-log --process-file --clear-cache --libvexiq --preferred-renderer ${LP3D_RENDERER7}"
        LP3D_CHECK_FILE="$(realpath ${SOURCE_DIR})/builds/check/VEXIQ/spider.mpd"
        ;;
      esac

    echo && echo ${LP3D_CHECK_HDR}
    #echo "  Executable: $(realpath ${LPUB3D_VER})"
    [ -n "$USE_XVFB" ] && xvfb-run --auto-servernum --server-num=1 --server-args="-screen 0 1024x768x24" \
    ${LPUB3D_EXE} ${LP3D_CHECK_OPTIONS} ${LP3D_CHECK_FILE} &> ${LP3D_LOG_FILE} || \
    ${LPUB3D_EXE} ${LP3D_CHECK_OPTIONS} ${LP3D_CHECK_FILE} &> ${LP3D_LOG_FILE}
#    ${LPUB3D_EXE} ${LP3D_CHECK_OPTIONS} ${LP3D_CHECK_FILE} || \
#    ${LPUB3D_EXE} ${LP3D_CHECK_OPTIONS} ${LP3D_CHECK_FILE}
    # allow some time between checks
    sleep 4
    # check output log for build check status
    if [ -f "${LP3D_LOG_FILE}" ]; then
        if grep -q "${LP3D_CHECK_SUCCESS}" "${LP3D_LOG_FILE}"; then
            echo "${LP3D_CHECK_HDR} SUCCESS"
            let LP3D_CHECK_PASS++
            LP3D_CHECKS_PASS="${LP3D_CHECKS_PASS},$(echo ${LP3D_CHECK_HDR} | cut -d " " -f 3)"
        else
            echo "${LP3D_CHECK_HDR} FAIL"
            let LP3D_CHECK_FAIL++
            LP3D_CHECKS_FAIL="${LP3D_CHECKS_FAIL},$(echo ${LP3D_CHECK_HDR} | cut -d " " -f 3)"
            cat "${LP3D_LOG_FILE}"
        fi
        rm -rf "${LP3D_LOG_FILE}"
    else
        echo "ERROR - ${LP3D_LOG_FILE} not found."
    fi
done

echo && echo "----Build Check Status: PASS (${LP3D_CHECK_PASS})[${LP3D_CHECKS_PASS}], FAIL (${LP3D_CHECK_FAIL})[${LP3D_CHECKS_FAIL}]----" && echo
echo && echo "------------Build checks completed----------" && echo

