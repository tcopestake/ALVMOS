import os
import math

output_dir = ".out"
iso_path = output_dir + "/alvm.iso"
boot_bin_path = output_dir + "/boot.bin"
kernel_bin_path = output_dir + "/kernel.bin"

with open(iso_path, mode='wb') as output:
    padding = bytearray([0x00 for _ in range(10 * 1024 * 1024)])

    output.write(padding)

    output.seek(0)

    with open(boot_bin_path, mode='rb') as mbr:
        output.write(mbr.read())

    with open(kernel_bin_path, mode='rb') as kernel:
        kernel_size = os.path.getsize(kernel_bin_path)

        kernel_sectors = math.ceil(kernel_size / 512)

        output.write(b"AL")
        
        output.write(kernel_sectors.to_bytes(2, byteorder='little'))

        output.write(kernel.read())
