#!/bin/bash
set -e

# Check for root privileges
if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root (use sudo)."
    exit 1
fi

VENDOR_ID="1a86"
PRODUCT_ID="55d3"
RULE_FILE="/etc/udev/rules.d/99-soarm-usb-serial.rules"

# Function to get the serial number of a device by its /dev/ttyACM* path
get_serial() {
    local dev="$1"
    udevadm info -q property -n "$dev" | grep '^ID_SERIAL_SHORT=' | cut -d= -f2
}

# Function to find all tty devices with the specified VID:PID
find_devices() {
    for dev in /dev/ttyACM*; do
        if [[ -e "$dev" ]]; then
            local vid=$(udevadm info -q property -n "$dev" | grep '^ID_VENDOR_ID=' | cut -d= -f2)
            local pid=$(udevadm info -q property -n "$dev" | grep '^ID_MODEL_ID=' | cut -d= -f2)
            if [[ "$vid" == "$VENDOR_ID" && "$pid" == "$PRODUCT_ID" ]]; then
                echo "$dev"
            fi
        fi
    done
}

# Associative array: serial number -> desired name
declare -A SERIAL_TO_NAME

echo "=== Identifying devices $VENDOR_ID:$PRODUCT_ID ==="
echo "Please disconnect ALL target devices and press Enter."
read

# Wait until no devices remain
while true; do
    current_devs=$(find_devices)
    if [[ -z "$current_devs" ]]; then
        echo "All devices disconnected. Great."
        break
    else
        echo "Still connected devices: $current_devs"
        echo "Disconnect them and press Enter."
        read
    fi
done

previous_devs=""

echo ""
echo "Now connect the devices one by one."
echo "For each device, enter a desired name (e.g., left, right, gripper)."
echo "If you leave the name empty, the serial number will be used."
echo "When all devices are added, enter 'done'."

while true; do
    echo ""
    echo "Connect the next device and press Enter (or type 'done' to finish)."
    read input
    if [[ "$input" == "done" ]]; then
        break
    fi

    # Find the new device that wasn't present before
    current_devs=$(find_devices)
    new_dev=""
    for dev in $current_devs; do
        if ! echo "$previous_devs" | grep -q "$dev"; then
            new_dev="$dev"
            break
        fi
    done

    if [[ -z "$new_dev" ]]; then
        echo "No new device detected. Make sure it is connected and try again."
        continue
    fi

    serial=$(get_serial "$new_dev")
    if [[ -z "$serial" ]]; then
        echo "Could not obtain serial number for $new_dev. Skipping."
        previous_devs="$current_devs"
        continue
    fi

    echo "Detected device: $new_dev, serial number: $serial"
    read -p "Enter a name for this device (Enter = use serial number): " name
    if [[ -z "$name" ]]; then
        name="$serial"
    fi

    if [[ -n "${SERIAL_TO_NAME[$serial]}" ]]; then
        echo "A device with this serial number is already added ($serial -> ${SERIAL_TO_NAME[$serial]}). Skipping."
    else
        SERIAL_TO_NAME["$serial"]="$name"
        echo "Recorded: $serial -> $name"
    fi

    previous_devs="$current_devs"
done

# Check that at least one device was added
if [[ ${#SERIAL_TO_NAME[@]} -eq 0 ]]; then
    echo "No devices added. Exiting."
    exit 1
fi

# Create the udev rule
echo ""
echo "Creating rule file: $RULE_FILE"
{
    echo "# Udev rules for SoArm USB serial devices (auto-generated)"
    for serial in "${!SERIAL_TO_NAME[@]}"; do
        name="${SERIAL_TO_NAME[$serial]}"
        echo "SUBSYSTEM==\"tty\", ATTRS{idVendor}==\"$VENDOR_ID\", ATTRS{idProduct}==\"$PRODUCT_ID\", ATTRS{serial}==\"$serial\", SYMLINK+=\"$name\", MODE=\"0666\""
    done
} > "$RULE_FILE"

# Apply the rules
udevadm control --reload-rules
udevadm trigger
udevadm settle

# Show the result   
echo ""
echo "Rules applied. Symbolic links:"
for serial in "${!SERIAL_TO_NAME[@]}"; do
    name="${SERIAL_TO_NAME[$serial]}"
    if [[ -e "/dev/$name" ]]; then
        echo "  /dev/$name -> $(readlink -f /dev/$name)"
    else
        echo "  /dev/$name -> not found (device may be disconnected)"
    fi
done

echo ""
echo "Done. You can now use the stable names /dev/<name> for your devices."