#!/bin/bash

REMOTE_PATH="/userdata/NanoArch"
DIRECTORIES=("configs" "cores" "roms" "bin" "assets")
echo "--- starting NanoArch Deployment to RK3506G ---"

adb_status=$(adb get-state 2>/dev/null)
if [ "$adb_status" != "device" ]; then
    echo "[Error] No ADB device detected. Please check your USB connection or drivers."
    exit 1
fi

echo "[1/3] preparing remote directory: $REMOTE_PATH"
adb shell "mkdir -p $REMOTE_PATH"
adb shell "mkdir -p $REMOTE_PATH/configs"
adb shell "mkdir -p $REMOTE_PATH/cores"
adb shell "mkdir -p $REMOTE_PATH/roms"
adb shell "mkdir -p $REMOTE_PATH/bin"
adb shell "mkdir -p $REMOTE_PATH/assets"

echo "[2/3] transferring data..."
for dir in "${DIRECTORIES[@]}"; do
    if [ -d "$dir" ]; then
        echo "  Pushing $dir -> $REMOTE_PATH/$dir"
        adb push "$dir" "$REMOTE_PATH/"
    else
        echo "  [Warning] Local directory not found: $dir. Skipping."
    fi
done

echo "[3/3] Setting execution permissions..."
adb shell "chmod +x $REMOTE_PATH/bin/NanoArch"
adb shell "ls $REMOTE_PATH/bin/"
adb shell "mv $REMOTE_PATH/bin/NanoArch $REMOTE_PATH"
adb shell "rm -rf $REMOTE_PATH/bin"

echo "--- deployment Complete ---"
echo "You can run the manager on the board using the following command:"
echo "adb shell \"cd $REMOTE_PATH/bin && echo 0 > /sys/class/graphics/fb0/blank && ./NanoArch\""
