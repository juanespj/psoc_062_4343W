# flash
west flash -r pyocd
# west build clean
west build -p always
west build -c
# Zephyrcfg
west config zephyr.base ~/zephyrproject/zephyr
west init

# Tell Zephyr where the SDK lives
export ZEPHYR_BASE=~/zephyrproject/zephyr

# Load the Zephyr build extensions (this adds the 'build' command to west)
source $ZEPHYR_BASE/zephyr-env.sh
export ZEPHYR_BASE=~/zephyrproject/zephyr
source ~/zephyrproject/.venv/bin/activate
~/zephyrproject/.venv/bin/activate
# attach probe
## Windows
usbipd list
usbipd bind --busid 1-3
usbipd attach --wsl --busid 1-3
## WSL
lsusb

west blobs fetch hal_infineon

west build -p always -b nrf52840dk/nrf52840 .

west build -b cy8cproto_062_4343w_m0  # build M0 app
west build -b cy8cproto_062_4343w  # build for M4

west build -b cy8cproto_062_4343w && west flash
west config -d build.board
# Flashing
Use pyocd instead of openocd
The CY8CPROTO board has a KitProg3 onboard. While OpenOCD is the Zephyr default, PyOCD often has much better "out of the box" support for PSoC 6 targets and is less finicky about script paths.

Install the PSoC 6 pack for PyOCD:

Bash
pip install pyocd
pyocd pack install psoc6
Flash using the PyOCD runner:

Bash
west flash -r pyocd
If this works, you can make it your default runner so you don't have to type -r every time:

Bash
west config board.cy8cproto_062_4343w.runner pyocd

# Access Serial Port (WSL)
Identify the device name:

Bash
dmesg | grep tty
You are looking for ttyACM0 or ttyUSB0.
# Open the port at 115200 baud (Zephyr default)
minicom -D /dev/ttyACM0 -b 115200

Check Permissions:
By default, you might not have permission to read the port. Add yourself to the dialout group:

Bash
sudo usermod -a -G dialout $USER
(Note: You'll need to restart your WSL session for this to take effect.)


# .map analysis at -> build/zephyr/zephyr.map
# find uart cfg
 grep -i "uart" build/zephyr/zephyr.dts | head -n 20



 config output
 build/zephyr/.config  -> merged config
 build/zephyr/include/generated/zephyr/autoconf.h C macros
 build/compile_commands.json


 kconfig udp
     config UDP_REMOTE_HOST
        string "UDP remote host"
        default "192.168.1.100"
        help
            IPv4 address of the UDP peer to send datagrams to.

    config UDP_REMOTE_PORT
        int "UDP remote port"
        default 5005
        range 1 65535
        help
            UDP destination port.

    config UDP_LOCAL_PORT
        int "UDP local bind port"
        default 5005
        range 1 65535
        help
            Local port to bind for receiving datagrams.

    config UDP_STACK_SIZE
        int "UDP thread stack size"
        default 2048
        help
            Stack size for the UDP receive thread.

    config UDP_BUF_SIZE
        int "UDP buffer size (bytes)"
        default 256
        range 64 1500
        help
            Size of the send/receive buffer.