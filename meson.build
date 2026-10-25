project(
    'fvwm3',
    'c',
    version: run_command(['cat', '.current-version'], check: true).stdout().strip(),
    license: 'GPL-2.0-or-later',
    meson_version: '>=1.5.1', # Required to configure perl files without eating \@s
    default_options: [
        'c_std=c11',
        'warning_level=2',
        'werror=false',
        'b_pie=true',
    ],
)

add_project_arguments(
    [
        '-fno-common',
        '-Wno-sign-compare',
        '-Wno-unused-parameter',
    ],
    language: 'c',
)

# This script is used to clean up the release tarball when calling
# "meson dist".
meson.add_dist_script('meson-scripts/dist-tarball.sh')

cc = meson.get_compiler('c')

# A missing declaration can silently narrow a returned pointer on LP64.
foreach flag : ['-Werror=implicit-function-declaration', '-Werror=incompatible-pointer-types']
    if cc.has_argument(flag)
        add_project_arguments(flag, language: 'c')
    endif
endforeach

# Apply defenses that are independent of distributor CFLAGS.
hardening_flags = ['-fstack-protector-strong']
if host_machine.system() == 'linux'
    hardening_flags += '-fstack-clash-protection'
endif
if (
    get_option('optimization') != '0' and
    cc.get_define('_FORTIFY_SOURCE') == ''
)
    fortify_level_3 = '''
        #include <features.h>
        #if !defined(__USE_FORTIFY_LEVEL) || __USE_FORTIFY_LEVEL < 3
        #error Fortify level 3 is unavailable
        #endif
        int main(void) { return 0; }
    '''
    fortify_level_2 = '''
        #include <features.h>
        #if !defined(__USE_FORTIFY_LEVEL) || __USE_FORTIFY_LEVEL < 2
        #error Fortify level 2 is unavailable
        #endif
        int main(void) { return 0; }
    '''
    if cc.compiles(
        fortify_level_3,
        args: ['-O2', '-D_FORTIFY_SOURCE=3'],
        name: '_FORTIFY_SOURCE=3 support',
    )
        hardening_flags += '-D_FORTIFY_SOURCE=3'
    elif cc.compiles(
        fortify_level_2,
        args: ['-O2', '-D_FORTIFY_SOURCE=2'],
        name: '_FORTIFY_SOURCE=2 support',
    )
        hardening_flags += '-D_FORTIFY_SOURCE=2'
    endif
endif
foreach flag : hardening_flags
    if cc.has_argument(flag)
        add_project_arguments(flag, language: 'c')
    endif
endforeach

# Do not rely on GCC distribution specs: Clang/lld may default to lazy
# binding. Require complete RELRO and a non-executable stack where supported.
if host_machine.system() == 'linux'
    foreach flag : ['-Wl,-z,relro', '-Wl,-z,now', '-Wl,-z,noexecstack']
        if cc.has_link_argument(flag)
            add_project_link_arguments(flag, language: 'c')
        endif
    endforeach
endif

host = host_machine.system() # This is the target system when cross compiling

m_dep = cc.find_library('m', required: false)
if m_dep.found()
    add_project_link_arguments('-lm', language: 'c')
endif

# Convenience helpers for things we might refer to a few times.
bindir = get_option('bindir')
datadir = get_option('datadir')
sysconfdir = get_option('sysconfdir')
prefix = get_option('prefix')
fvwm_datadir = join_paths(prefix, datadir, meson.project_name())
fvwm_moduledir = (
    prefix / get_option('libexecdir') / meson.project_name() / meson.project_version()
)
fvwm_vcs_versioninfo = run_command(
    './utils/fvwm-version-str.sh',
    check: false,
).stdout().strip()

syslibs = []

conf = configuration_data()
conf.set('FVWM_COLORSET_LIMIT', get_option('colorset-limit'))
conf.set('FVWM_PAGER_MAX_DESKS', get_option('pager-max-desks'))
conf.set('FVWM_FONT_SHADOW_LIMIT', get_option('font-shadow-limit'))
conf.set('FVWM_IMAGE_INPUT_LIMIT', get_option('image-input-limit') * 1048576)
conf.set('FVWM_IMAGE_XPM_EXPANDED_LIMIT', get_option('image-xpm-expanded-limit') * 1048576)
conf.set('FVWM_IMAGE_DECOMPRESS_TIMEOUT_MS', get_option('image-decompress-timeout') * 1000)
conf.set('FVWM_IMAGE_EFFECT_TIMEOUT_MS', get_option('image-effect-timeout') * 1000)
conf.set('FVWM_IMAGE_EFFECT_MEMORY_LIMIT', get_option('image-effect-memory-limit').to_string() + 'ULL * 1048576ULL')
conf.set('FVWM_IMAGE_SVG_TIMEOUT_MS', get_option('image-svg-timeout') * 1000)
conf.set('FVWM_IMAGE_SVG_MEMORY_LIMIT', get_option('image-svg-memory-limit').to_string() + 'ULL * 1048576ULL')
conf.set_quoted('FVWM_IMAGE_SVG_WORKER', fvwm_moduledir / 'fvwm-image-svg-worker')
conf.set10('HAVE_CLOSEFROM', cc.has_function(
    'closefrom', prefix: '#define _DEFAULT_SOURCE 1\n#include <unistd.h>',
))

if host_machine.endian() == 'big'
    conf.set('WORDS_BIGENDIAN', true)
endif

if host_machine.system() == 'darwin'
    conf.set10('HOST_MACOS', true)
    add_project_arguments(['-Wno-nonportable-include-path'], language: 'c')
endif

# Set static configuration.
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('VERSIONINFO', fvwm_vcs_versioninfo)
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('FVWM2RC', '.fvwm2rc')
conf.set_quoted('FVWM_CONFIG', 'config')
conf.set_quoted(
    'FVWM_IMAGEPATH',
    '/usr/include/X11/bitmaps:/usr/include/X11/pixmaps',
)
conf.set_quoted(
    'FVWM_MODULEDIR',
    fvwm_moduledir,
)
conf.set_quoted('FVWM_DATADIR', fvwm_datadir)
conf.set_quoted('LOCALEDIR', prefix / get_option('localedir'))
conf.set_quoted('FVWM_CONFDIR', prefix / get_option('sysconfdir'))
conf.set('RETSIGTYPE', 'void')
conf.set('SIGNAL_RETURN', 'return')
conf.set('ICONV_ARG_CONST', '')
conf.set('fd_set_size_t', 'int')
conf.set('EXECUTABLE_EXTENSION', 'NULL')

# Enable FTMs
if host_machine.system() == 'linux'
    conf.set10('_DEFAULT_SOURCE', true) # usleep
endif

# Header checks.
headers = [
    'fcntl.h',
    'float.h',
    'getopt.h',
    'inttypes.h',
    'limits.h',
    'malloc.h',
    'memory.h',
    'stdarg.h',
    'stdint.h',
    'stdlib.h',
    'string.h',
    'strings.h',
    'sys/select.h',
    'sys/stat.h',
    'sys/systeminfo.h',
    'sys/time.h',
    'sys/wait.h',
    'unistd.h',
]

foreach h : headers
    if cc.has_header(h)
        conf.set10('HAVE_' + h.to_upper().underscorify(), true)
    endif
endforeach

if conf.has('HAVE_SYS_SELECT_H')
    conf.set10('HAVE_SELECT', true)
    conf.set('SELECT_FD_SET_CAST', '')
endif

# Function checks.
functions = [
    'div',
    'getpwuid',
    'gettimeofday',
    'lstat',
    'memcpy',
    'memmove',
    'mkfifo',
    'putenv',
    'setpgid',
    'setpgrp',
    'setvbuf',
    'sigaction',
    'siginterrupt',
    'siglongjmp',
    'sigsetjmp',
    'socket',
    'strchr',
    'strdup',
    'strstr',
    'strtol',
    'sysconf',
    'intl',
    'uname',
    'wait3',
    'waitpid',
]

foreach f : functions
    if cc.has_function(f, dependencies: syslibs)
        val = 'HAVE_' + f.to_upper().underscorify()
        conf.set10(val, true)
    endif
endforeach

# SETPGRP_VOID is obsolete
# All current systems have a setpgrp whose signature conforms to Posix.
# We need to set this to use the correct implementation internally
# but we don't need to check for it.
if conf.has('HAVE_SETPGRP')
    conf.set10('SETPGRP_VOID', true)
endif

# Dependencies.
all_found_deps = []

# Required dependencies, we can easily iterate over these.
summary_depvals = {}
all_req_deps = ['fontconfig', 'ice', 'libevent', 'x11', 'xft', 'xrandr', 'xt', 'xtrans']
foreach rd : all_req_deps
    dep_version = []
    if rd == 'libevent'
        dep_version = '>=2.0'
    elif rd == 'xrandr'
        dep_version = '>=1.5'
    endif
    this_dep = dependency(rd, required: true, version: dep_version)
    summary_depvals += {rd: this_dep}
    conf.set10('HAVE_' + rd.to_upper().underscorify(), true)
    all_found_deps += this_dep
endforeach

xext = dependency('xext', required: true)
all_found_deps += xext
summary_depvals += {'xext': xext}
conf.set10('SHAPE', true)

# Runtime owners retain full package dependencies and their transitive links.
core_runtime_deps = [
    summary_depvals['fontconfig'],
    summary_depvals['x11'],
    summary_depvals['xft'],
    summary_depvals['xrandr'],
    xext,
]

# Begin Xlib XKB requirement.
# XKB key translation is part of libX11, not libxkbcommon.  Refuse an
# incomplete Xlib development package rather than advertising an unchecked
# capability or falling back to a different keyboard interpretation.
xkb_probe = '''
    #include <X11/XKBlib.h>
    int main(void)
    {
        return XkbKeycodeToKeysym((Display *)0, 0, 0, 0) == NoSymbol;
    }
'''
if not cc.links(
    xkb_probe,
    dependencies: summary_depvals['x11'],
    name: 'Xlib XKB key translation',
)
    error('Xlib development files must provide X11/XKBlib.h and XkbKeycodeToKeysym')
endif
conf.set10('HAVE_X11_XKBLIB_H', true)
# End Xlib XKB requirement.

# Python is a required dependency, we generate shebangs at buildtime
# This will error by default if there is no python interpreter found
py_mod = import('python')
if get_option('python') == ''
    # Just look for a python3 impl using standard logic, we weren't given a value
    # as an option
    py = py_mod.find_installation('python3')
else
    # Try and find python by provided version or path
    message('Trying to find python3: ' + get_option('python'))
    py = py_mod.find_installation(get_option('python'))
endif

# Perl is a required dependency, we generate shebangs at buildtime
perl = find_program('perl', required: true)

# Required Go compiler policy; keep this block independent of native libraries.
golang = find_program('go', required: true)
go_env = run_command(
    'env', 'GOTOOLCHAIN=local', golang, 'env', 'GOVERSION', check: true,
)
go_policy = run_command(
    py, files('tools/maintenance/go_version.py'),
    go_env.stdout().strip(), check: false,
)
if go_policy.returncode() != 0
    error(go_policy.stderr().strip())
endif
gover = go_policy.stdout().strip()
# End required Go compiler policy.

# Sed is used for substitution in several configure_file calls
sed = find_program('sed', required: true)

# Optional dependencies
# Tip: 'Feature' types always report 'not found' if disabled

freetype = dependency('freetype2', required: get_option('freetype'))
if freetype.found()
    all_found_deps += freetype
    conf.set10('HAVE_FREETYPE', true)
endif

fribidi = dependency(
    'fribidi',
    version: '>=0.19.2',
    required: get_option('bidi'),
)
if fribidi.found()
    all_found_deps += fribidi
    core_runtime_deps += fribidi
    conf.set10('HAVE_BIDI', true)
endif

iconv_warning = false

# BSD builds also search ports headers. Meson's libc probe does not,
# so a ports iconv.h can otherwise select libiconv symbols without
# linking the library that supplies them. OpenBSD also needs the explicit
# ports library directory to discover its optional iconv implementation.
use_ports_iconv = false
if host in ['freebsd', 'openbsd'] and not get_option('iconv').disabled()
    use_ports_iconv = cc.has_header_symbol(
        'iconv.h', '_LIBICONV_VERSION', args: ['-I/usr/local/include'],
    )
endif
if use_ports_iconv
    ports_iconv = cc.find_library(
        'iconv', dirs: ['/usr/local/lib'], required: get_option('iconv'),
    )
    if ports_iconv.found()
        iconv = declare_dependency(
            dependencies: ports_iconv,
            compile_args: ['-I/usr/local/include'],
        )
    else
        iconv = dependency('iconv', required: get_option('iconv'))
    endif
else
    iconv = dependency('iconv', required: get_option('iconv'))
endif
if iconv.found()
    iconv_code = '''
    #include <iconv.h>
    int main(void) {
        iconv_t converter = iconv_open("UTF-8", "UTF-8");
        return converter == (iconv_t)-1 ? 1 : iconv_close(converter);
    }
    '''
    if not cc.links(
        iconv_code, dependencies: iconv, name: 'iconv header/library match',
    )
        error('The selected iconv header and library do not link together.')
    endif
    conf.set10('HAVE_ICONV', true)
    all_found_deps += iconv
    core_runtime_deps += iconv

    # Check if we have nl_langinfo function (typically glibc)
    if cc.has_function('nl_langinfo')
        conf.set10('HAVE_CODESET', true)
    endif

    libcharset_code = '''
    #include <libcharset.h>

    int main(void) {
        const char *charset = locale_charset();
        return 0;
    }
    '''

    if cc.links(libcharset_code, dependencies: iconv, name: 'libcharset check')
        conf.set10('HAVE_LIBCHARSET', true)
    elif cc.has_header_symbol('libcharset.h', 'locale_charset')
        # If header is present but linking fails, it might be a conflict
        # we'll warn after the summary so that this isn't lost in configure noise
        iconv_warning = true
    endif
endif

# BSD ports install gettext outside the compiler's default library search.
# Select its headers and library together, as with the ports iconv above.
nls_enabled = false
if host in ['freebsd', 'openbsd'] and not get_option('nls').disabled()
    ports_libintl = cc.find_library(
        'intl', dirs: ['/usr/local/lib'], required: false,
    )
    if ports_libintl.found()
        libintl = declare_dependency(
            dependencies: ports_libintl,
            compile_args: ['-I/usr/local/include'],
        )
    else
        libintl = dependency('intl', required: get_option('nls'))
    endif
else
    libintl = dependency('intl', required: get_option('nls'))
endif
if libintl.found()
    nls_msgfmt = find_program('msgfmt', required: get_option('nls'))
    nls_enabled = nls_msgfmt.found()
endif
if nls_enabled
    gettext_code = '''
    #include <libintl.h>
    int main(void) {
        bindtextdomain("fvwm3", ".");
        textdomain("fvwm3");
        return gettext("") == 0;
    }
    '''
    if not cc.links(
        gettext_code, dependencies: libintl, name: 'gettext header/library match',
    )
        error('The selected gettext header and library do not link together.')
    endif
    all_found_deps += libintl
    core_runtime_deps += libintl
    conf.set10('HAVE_NLS', true)
elif libintl.found()
    warning('NLS disabled: msgfmt is required to build translation catalogs.')
endif

libpng = dependency('libpng', version: '>=1.2', required: get_option('png'))
if libpng.found()
    if libpng.version().version_compare('<1.6.59') and not get_option('png-security-backport')
        error('PNG support requires libpng >=1.6.59 or a verified vendor ' +
              'security backport. Set -Dpng-security-backport=true only for ' +
              'an audited patched package; -Dpng=disabled explicitly omits PNG support.')
    endif
    all_found_deps += libpng
    core_runtime_deps += libpng
    conf.set10('HAVE_PNG', true)
else
    # We only check for libpng, no automagic
    libpng_check = dependency('libpng', required: false)
    if libpng_check.found()
        png_msg = '''libpng found, and -Dpng=disabled was given.
        It is recommended that libpng be enabled so that PNG icons can
        be rendered by FVWM, which is expected by the default configuration
        as well as other third-party programs which generate menus,
        among others.

        If it is critical that FVWM does not link against libpng
        (perhaps due to size limitations of the binary),
        continue down this path.
        '''
    else
        png_msg = ''' Disabled PNG support means that the default
            configuration shipped with FVWM will be without icon
            support.  Be sure this is what you mean to do.
            '''
    endif
    warning(png_msg)
endif

# Decoder/scale types need X headers, without X11/XRandR runtime linkage.
image_renderer_deps = [
    summary_depvals['x11'].partial_dependency(compile_args: true, includes: true),
    summary_depvals['xrandr'].partial_dependency(compile_args: true, includes: true),
]
# This is the API floor used by the existing modern renderer, not a claim
# that every older upstream release remains maintained or security-patched.
librsvg = dependency(
    'librsvg-2.0',
    version: '>=2.52',
    required: get_option('svg'),
)
if librsvg.found()
    svg_cairo = dependency('cairo', required: true)
    svg_api_code = '''#include <librsvg/rsvg.h>
#include <cairo.h>
#if !LIBRSVG_CHECK_VERSION(2, 52, 0)
#error librsvg headers are older than the renderer API floor
#endif
int main(void)
{
    RsvgHandle *handle = rsvg_handle_new();
    cairo_surface_t *surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *cr = cairo_create(surface);
    RsvgRectangle viewport = {0, 0, 1, 1};
    double width, height;
    rsvg_handle_get_intrinsic_size_in_pixels(handle, &width, &height);
    gboolean ok = rsvg_handle_render_document(handle, cr, &viewport, NULL);
    g_object_unref(handle);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return !ok;
}
'''
    # Link only: configuration never executes a renderer.
    if not cc.links(svg_api_code, dependencies: [librsvg, svg_cairo],
                    name: 'librsvg 2.52/Cairo renderer API')
        error('SVG support requires matching librsvg >=2.52/Cairo headers ' +
              'and libraries. Use -Dsvg=disabled to omit SVG support.')
    endif
    all_found_deps += [librsvg, svg_cairo]
    summary_depvals += {'cairo': svg_cairo}
    conf.set10('HAVE_RSVG', true)
    # Only the isolated renderer owns SVG/Cairo runtime and static transitives.
    image_renderer_deps += [librsvg, svg_cairo]
endif

sm = dependency('sm', required: get_option('sm'))
if sm.found()
    all_found_deps += sm
    core_runtime_deps += [sm, summary_depvals['ice']]
    conf.set10('SESSION', true)
endif

xcursor = dependency('xcursor', required: get_option('xcursor'))
if xcursor.found()
    all_found_deps += xcursor
    core_runtime_deps += xcursor
    conf.set10('HAVE_XCURSOR', true)
endif

xfixes = dependency('xfixes', required: get_option('xfixes'))
if xfixes.found()
    all_found_deps += xfixes
    core_runtime_deps += xfixes
    conf.set10('HAVE_XFIXES', true)
endif

# Preserve system-decompressor support without introducing a library. A custom
# path is a trusted build choice; it is never a theme-supplied shell command.
image_gzip_option = get_option('image-gzip')
image_gzip = find_program(
    image_gzip_option == '' ? ['gzip'] : [image_gzip_option],
    required: image_gzip_option != '',
)
conf.set_quoted('FVWM_IMAGE_GZIP', image_gzip.found() ? image_gzip.full_path() : '')

xpm = dependency('xpm', required: get_option('xpm'))
if xpm.found()
    all_found_deps += xpm
    core_runtime_deps += xpm
    conf.set10('XPM', true)
endif

xrender = dependency('xrender', required: get_option('xrender'))
if xrender.found()
    all_found_deps += xrender
    core_runtime_deps += xrender
    conf.set10('HAVE_XRENDER', true)
endif

# Legacy umbrella headers need broad compile visibility. The archive must not
# pass package link requirements to consumers that do not extract their owners.
all_compile_deps = []
foreach dep : all_found_deps
    all_compile_deps += dep.partial_dependency(compile_args: true, includes: true)
endforeach

# Shared archive objects retain every enabled backend, even when a consumer
# only uses one of their entry points. Keep full package dependencies here.
image_runtime_deps = [summary_depvals['x11'], xext]
foreach dep : [libpng, xcursor, xpm, xrender]
    if dep.found()
        image_runtime_deps += dep
    endif
endforeach

# Graphics initialization owns X drawing; SVG executes in the worker.
graphics_init_runtime_deps = image_runtime_deps + [summary_depvals['xrandr']]

text_runtime_deps = graphics_init_runtime_deps + [
    summary_depvals['fontconfig'],
    summary_depvals['xft'],
]
foreach dep : [fribidi, iconv]
    if dep.found()
        text_runtime_deps += dep
    endif
endforeach

localized_text_runtime_deps = text_runtime_deps
if nls_enabled
    localized_text_runtime_deps += libintl
endif

# Only modules that extract fsm.c own its session transport dependencies.
module_session_deps = []
if sm.found()
    module_session_deps += [sm, summary_depvals['ice']]
endif

# Hard-coded
non_configurable_ops = [
    'FMiniIconsSupported',
    'FORK_CREATES_CHILD',
    'HAVE_SAFETY_MKSTEMP', #  safer since 2008; no real need to check for this on a modern system.
    'HAVE_X11_FD',
    'HAVE_XOUTPUT_METHOD',
    'HAVE_XSHM',
]

foreach nco : non_configurable_ops
    conf.set10(nco, true)
endforeach

build_docs = false
if get_option('htmldoc') or get_option('mandoc')
    build_docs = true
    asciidoc = find_program('asciidoctor', required: build_docs)
    subdir('doc')
endif

conf_template = '@0@/config_defines.h'.format(meson.current_source_dir())

conf_out = configure_file(
    output: 'configtemp.h',
    configuration: conf,
)

conf_cmd = '@0@/meson-scripts/conf-cmd.sh'.format(meson.current_source_dir())
build_dir = '@0@/config.h'.format(meson.current_build_dir())
configure_file(
    output: 'config.h',
    command: [conf_cmd, conf_out, conf_template, build_dir],
)

# Configured files inherit the permissions of the template,
# so we need to set the permissions explicitly for executable scripts.
install_mask_755 = 'rwxr-xr-x'

includedirs = []
# To be consistent with the autotools build, we need to add /usr/local/include
# to the include path on *BSD.
if host in ['freebsd', 'openbsd']
    usrlocal_includes = include_directories('/usr/local/include')
    includedirs += usrlocal_includes
endif
lib_includes = include_directories('.')
fvwm_includes = include_directories('fvwm')

includedirs += lib_includes
includedirs += fvwm_includes

# These are used to configure files in 'bin', 'perllib', and other misc files.
# while we use terms like 'bindir' here, these don't (quite) map to meson;
# most things in fvwm3 want full paths. Autotools would use variables in the
# output, but we can construct full paths here instead.

# This could be tidied up, but we just want to be consistent with
# autotools for now.

file_config = configuration_data()
file_config.set('bindir', prefix / bindir)
file_config.set('datadir', prefix / datadir) # We could probably simplify this and the below into one config...
file_config.set('datarootdir', prefix / datadir)
file_config.set('FVWM_DATADIR', fvwm_datadir)
file_config.set(
    'FVWM_PERLLIBDIR',
    fvwm_datadir / 'perllib',
)
file_config.set('PERL', perl.full_path())
file_config.set('prefix', prefix)
file_config.set('PYTHON', py.full_path())
file_config.set('SED', sed.full_path())
file_config.set('VERSION', meson.project_version())
file_config.set('VERSIONINFO', fvwm_vcs_versioninfo)

if nls_enabled
    subdir('po')
endif
subdir('libs')
subdir('fvwm')
subdir('default-config')
subdir('perllib')

fvwm3 = executable(
    meson.project_name(),
    fvwm_sources,
    include_directories: includedirs,
    dependencies: all_compile_deps + core_runtime_deps,
    link_with: libfvwm3,
    install: true,
)

# Core modules
modules = {
    'FvwmAnimate': {'subdir': 'modules/FvwmAnimate'},
    'FvwmAuto': {'subdir': 'modules/FvwmAuto'},
    'FvwmBacker': {'subdir': 'modules/FvwmBacker'},
    'FvwmButtons': {'subdir': 'modules/FvwmButtons'},
    'FvwmEvent': {'subdir': 'modules/FvwmEvent'},
    'FvwmForm': {'subdir': 'modules/FvwmForm'},
    'FvwmIconMan': {'subdir': 'modules/FvwmIconMan'},
    'FvwmIdent': {'subdir': 'modules/FvwmIdent'},
    'FvwmMFL': {'subdir': 'modules/FvwmMFL'},
    'FvwmPager': {'subdir': 'modules/FvwmPager'},
    'FvwmPerl': {'subdir': 'modules/FvwmPerl'},
    'FvwmRearrange': {'subdir': 'modules/FvwmRearrange'},
    'FvwmScript': {'subdir': 'modules/FvwmScript'},
}

foreach m, _ : modules
    subdir(modules[m]['subdir'])
endforeach

# FvwmPrompt and FvwmCommand
subdir('bin')
subdir('tests')

# Contrib
# Provide a freedesktop-compilant, minimal xsession.
configure_file(
    input: 'contrib/xsession/fvwm3.desktop.in',
    output: 'fvwm3.desktop',
    configuration: file_config,
    install_dir: datadir / 'xsessions',
    install: true,
)

summary_docdir = get_option('datadir') / 'doc' / meson.project_name()
if get_option('docdir') != ''
    summary_docdir = get_option('docdir')
endif

summary(
    {
        'prefix': get_option('prefix'),
        'bindir': get_option('bindir'),
        'datadir': get_option('datadir'),
        'docdir': summary_docdir,
        'libdir': get_option('libdir'),
        'libexecdir': get_option('libexecdir'),
        'Module dir': get_option('libexecdir') / meson.project_name() / meson.project_version(),
    },
    section: 'Directories',
)

environment_summary = {
    'Build': build_machine.system(),
    'Build CPU Family': build_machine.cpu_family(),
    'Host CPU Family': host_machine.cpu_family(),
    'Cross-compiling': meson.is_cross_build(),
    'Build Endianness': build_machine.endian(),
    'Host Endianness': host_machine.endian(),
    'Target': host,
    'VCS Version': fvwm_vcs_versioninfo,
    'C Compiler': cc.get_id(),
    'C Compiler Version': cc.version(),
    'Go Version': gover,
    'Linker': cc.get_linker_id(),
    'Python Path': py.full_path(),
    'Python Version': py.version(),
    'Perl': perl,
    'Perl Version': run_command(
        perl,
        '-e', 'print "$^V\\n";',
        check: true,
    ).stdout().strip('v'),
}

if build_docs
    environment_summary += {
        'asciidoctor': asciidoc.version(),
    }
endif

summary(
    environment_summary,
    section: 'Environment',
)

summary(
    {
        'HTML pages': get_option('htmldoc'),
        'Man pages': get_option('mandoc'),
    },
    bool_yn: true,
    section: 'Documentation',
)

featurevals = {
    'BiDi': fribidi.found() ? fribidi : false,
    'FreeType': freetype.found() ? freetype : false,
    'Go Modules': true,
    'iconv': iconv.found(),
    'NLS': nls_enabled,
    'PNG support': libpng.found() ? libpng : false,
    'Session Management': sm.found() ? sm : false,
    'SVG support': librsvg.found() ? librsvg : false,
    'Xcursor': xcursor.found() ? xcursor : false,
    'XFixes': xfixes.found() ? xfixes : false,
    'XPM support': xpm.found() ? xpm : false,
    'XRender': xrender.found() ? xrender : false,
}

summary(
    featurevals,
    bool_yn: true,
    section: 'Features',
)

summary(
    summary_depvals,
    bool_yn: true,
    section: 'Dependencies',
)

if iconv_warning
    warning(
        '''Found libcharset.h header, but couldn't link against locale_charset() function.
        This typically happens when GNU libiconv conflicts with the iconv built into libc.
        GNU libc systems shouldn't have GNU libiconv installed as they already provide iconv.
        This is a bug in your distribution's packaging.
        Please report this to your distribution's developers.
        '''
    )
endif
