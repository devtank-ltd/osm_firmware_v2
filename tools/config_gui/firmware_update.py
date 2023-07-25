#!/usr/bin/env python3

import subprocess
import argparse

class firmware_update:
    def __init__(self, image, dev):
        self.image = image
        self.dev = dev

    def cleanup(self):
        print("Failed to write firmware.")
        exit(-1)

    def firmware_update(self):
        print(self.image)
        print(f"Using {self.dev}")

        bootloader = "bootloader.bin"
        firmware = "firmware.bin"

        gpio_str = "-rts&-dtr,dtr,rts,:-dtr,-rts"

        bootloader_addr = 0x08000000
        bootloader_size = 0x00001000
        config_addr = 0x08001000
        config_size = 0x00001000
        firmware_addr = 0x08002000

        ret = subprocess.call(["dd", f"of={bootloader}", f"if={self.image}", "count=2", "bs=2k"])
        if ret != 0:
            self.cleanup()
        else:
            ret_two = subprocess.call(["dd", f"of={firmware}", f"if={self.image}", "skip=4", "bs=2k"])

        if ret_two != 0:
            self.cleanup()
        else:
            stm_ret = subprocess.call(
                ["stm32flash", "-b", "115200",
                "-i", f"{gpio_str}" ,"-S",
                f"{bootloader_addr}:{bootloader_size}",
                "-w", f"{bootloader}", f"{self.dev}"])

        if stm_ret != 0:
            self.cleanup()
        else:
            stm_ret_2 = subprocess.call(
                ["stm32flash", "-b", "115200",
                "-i", f"{gpio_str}", "-S", f"{firmware_addr}",
                "-w", f"{firmware}", f"{self.dev}"])

        if stm_ret_2 != 0:
            self.cleanup()
        else:
            rm_f = subprocess.call(["rm", f"{bootloader}", f"{firmware}"])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
    prog='Write firmware image to OSM',
    epilog='Run this program with the path to the device and firmware image as an argument e.g. python firmware_update.py /dev/ttyUSB0 complete.bin'
    )
    parser.add_argument('device')
    parser.add_argument('image')
    args = parser.parse_args()
    dev = args.device
    image = args.image
    fw = firmware_update(image, dev)
    fw.firmware_update()
