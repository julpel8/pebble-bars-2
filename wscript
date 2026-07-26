#
# Standard Pebble SDK build rules, matching the working Solar Earth base.
#
import os.path

top = '.'
out = 'build'


def options(ctx):
    ctx.load('pebble_sdk')


def configure(ctx):
    ctx.load('pebble_sdk')


def build(ctx):
    ctx.load('pebble_sdk')

    build_worker = os.path.exists('worker_src')
    binaries = []

    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        if os.environ.get('BARS_SCREENSHOT_BUILD') == '1':
            ctx.env.append_value('DEFINES', 'BARS_SCREENSHOT_BUILD')
        else:
            ctx.env.DEFINES = [
                define for define in ctx.env.DEFINES
                if define != 'BARS_SCREENSHOT_BUILD'
            ]
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        # The native modules apply settings sent by the self-contained
        # configuration page in src/pkjs/index.js. The PBW must carry the
        # PebbleKit JS for the physical Time 2 installation path.
        ctx.pbl_build(source=ctx.path.ant_glob('src/c/**/*.c',
                                               excl=['src/c/watchface_only.c']),
                      target=app_elf,
                      bin_type='app')

        if build_worker:
            worker_elf = '{}/pebble-worker.elf'.format(ctx.env.BUILD_DIR)
            binaries.append({
                'platform': platform,
                'app_elf': app_elf,
                'worker_elf': worker_elf
            })
            ctx.pbl_build(source=ctx.path.ant_glob('worker_src/c/**/*.c'),
                          target=worker_elf,
                          bin_type='worker')
        else:
            binaries.append({'platform': platform, 'app_elf': app_elf})

    ctx.env = cached_env
    ctx.set_group('bundle')
    # Bundle the PebbleKit JS payload. The app declares messageKeys and ships a
    # src/pkjs/index.js, so the PBW must carry the JS; otherwise the phone
    # companion errors while loading the install (the watch install path that
    # the emery emulator does not exercise).
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js',
                                         'src/pkjs/**/*.json']),
                   js_entry_file='src/pkjs/index.js')
