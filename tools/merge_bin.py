# PlatformIO post-build script: merges bootloader, partition table and app into one image
# (.pio/build/<env>/windkiek.bin, flashed at offset 0). The web installer uses this file.
Import("env")  # noqa: F821  (provided by PlatformIO/SCons)


def merge(source, target, env):
    cmd = ['"$PYTHONEXE"', '"$OBJCOPY"', "--chip", env.get("BOARD_MCU"), "merge_bin",
           "-o", '"$BUILD_DIR/windkiek.bin"', "--flash_mode", "keep",
           "--flash_size", env.BoardConfig().get("upload.flash_size", "8MB")]
    for offset, image in env.get("FLASH_EXTRA_IMAGES", []):
        cmd += [offset, '"%s"' % image]
    cmd += [env.get("ESP32_APP_OFFSET", "0x10000"), '"$BUILD_DIR/${PROGNAME}.bin"']
    env.Execute(" ".join(cmd))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge)  # noqa: F821
