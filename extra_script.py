Import("env", "projenv")
from shutil import copyfile

# Dump global construction environment (for debug purpose)
#print(env.Dump()))
#print(projenv.Dump())

def save_bin(*args, **kwargs):
    print("Copying output to project directory...")
    target = str(kwargs['target'][0])
    env = DefaultEnvironment()
    version = env.GetProjectOption("custom_version")
    version = version[2:]  # remove 2 1st char
    version = version[:-2] # remove 2 last char
    dest = "build/" + env['PIOENV'] + "-" + version + ".bin"
    copyfile(target, dest)
    dest = "build/" + env['PIOENV'] + ".bin"
    copyfile(target, dest)
    copyfile(env['FLASH_EXTRA_IMAGES'][0][1], "build/bootloader.bin")
    copyfile(env['FLASH_EXTRA_IMAGES'][1][1], "build/partition.bin")
    print("Done.")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", save_bin)   #post action for the target bin file
