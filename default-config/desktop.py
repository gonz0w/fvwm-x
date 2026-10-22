#!@PYTHON@
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""FVWM-X desktop services, using only Python's standard library and base tools.

This process owns no managed-window state. File discovery, system commands and
screenshots run in bounded workers, never in FVWM's authoritative X event loop.
Only the main thread writes the module pipe; workers return data or argv arrays.
"""

import calendar
import concurrent.futures
import configparser
import ctypes as C
import ctypes.util
import datetime as dt
import fcntl
import hashlib
import json
import itertools
import os
from pathlib import Path
import re
import selectors
import secrets
import shlex
import shutil
import struct
import stat
import subprocess
import sys
import tempfile
import time
import zlib

WORD = struct.Struct('@L')
HEADER = struct.Struct('@LLLL')
M_NEW_DESK = 1 << 1
M_CONFIG_INFO = 1 << 18
M_END_CONFIG_INFO = 1 << 19
M_STRING = 1 << 22
PALETTE_NAMES = ('slate', 'dune', 'grove', 'ink')

# One data-only registry serves validation, queries and native menu consumers.
# Wallpaper is an internal retained surface, not a user-supplied path/action.
SETTINGS = {
    'palette': dict(type='enum', default='slate', choices=PALETTE_NAMES,
                    group='appearance', description='Desktop color palette'),
    'appearance': dict(type='enum', default='auto', choices=('light', 'dark', 'auto'),
                       group='appearance', description='Light, dark or scheduled appearance'),
    'day': dict(type='time', default='07:00', minimum='00:00', maximum='23:59',
                group='schedule', description='Local time when light appearance starts'),
    'night': dict(type='time', default='19:00', minimum='00:00', maximum='23:59',
                  group='schedule', description='Local time when dark appearance starts'),
    'transparency': dict(type='boolean', default=True, group='effects',
                         description='Tinted root background and menu snapshots'),
    'motion': dict(type='boolean', default=False, group='effects',
                   description='Menu repositioning and window shade steps'),
    'keep_wallpaper': dict(type='boolean', default=False, group='appearance',
                           description='Retain the current palette wallpaper'),
}
DEFAULTS = {key: item['default'] for key, item in SETTINGS.items()} | {
    'wallpaper': dict(palette='slate', appearance='dark')}
STATE_LIMIT = 8192
UNCHECKED_REVISION = object()


def setting_value(key, value, text=False):
    if key not in SETTINGS:
        raise ValueError('Unknown setting: ' + key)
    item = SETTINGS[key]
    if item['type'] == 'boolean':
        if text and isinstance(value, str):
            values = {'on': True, 'true': True, 'yes': True, '1': True,
                      'off': False, 'false': False, 'no': False, '0': False}
            value = values.get(value.lower(), value)
        valid = type(value) is bool
    elif item['type'] == 'enum':
        valid = isinstance(value, str) and value in item['choices']
    else:
        valid = isinstance(value, str)
        if valid:
            minute(value)
    if not valid:
        raise ValueError('Invalid value for ' + key)
    return value


def checked_settings(values):
    if not isinstance(values, dict) or set(values) != set(DEFAULTS):
        raise ValueError('Invalid settings fields')
    result = {key: setting_value(key, values[key]) for key in SETTINGS}
    if result['day'] == result['night']:
        raise ValueError('Day and night times must be different')
    wallpaper = values['wallpaper']
    if (not isinstance(wallpaper, dict) or set(wallpaper) != {'palette', 'appearance'}
            or wallpaper['palette'] not in PALETTE_NAMES
            or wallpaper['appearance'] not in ('light', 'dark')):
        raise ValueError('Invalid retained wallpaper')
    result['wallpaper'] = wallpaper.copy()
    return result


def setting_scope(scope):
    keys = [key for key, item in SETTINGS.items()
            if scope in ('all', key, 'group:' + item['group'])]
    if not keys:
        raise ValueError('Unknown settings scope: ' + scope)
    return keys


def desktop_configuration(message):
    prefix = '*FvwmDesktop'
    if not message.lower().startswith(prefix.lower()):
        return None
    # FVWM stores modern *Alias: option lines as *Aliasoption packets.
    # Also accept the colon form for direct protocol consumers.
    value = message[len(prefix):]
    if value.startswith(':'):
        value = value[1:]
    option, _, value = value.strip().partition(' ')
    return option.lower(), value.strip()


def clean_label(value, limit=100):
    """External names are text, never FVWM expansions/menu image directives."""
    return ''.join(c for c in str(value) if c.isprintable()
                   and c not in '\\"$%&').strip()[:limit]


def quote(value):
    return '"' + clean_label(value, 1024) + '"'


def minute(value):
    if not re.fullmatch(r'[0-2][0-9]:[0-5][0-9]', str(value)):
        raise ValueError('time must be HH:MM')
    hour, part = map(int, value.split(':'))
    if hour > 23:
        raise ValueError('hour must be 00..23')
    return hour * 60 + part


def validate_settings(values):
    """Migrate the original unversioned file, retaining valid known fields."""
    result = DEFAULTS.copy()
    result['wallpaper'] = DEFAULTS['wallpaper'].copy()
    if not isinstance(values, dict):
        return result
    for key in SETTINGS:
        try:
            result[key] = setting_value(key, values.get(key))
        except ValueError:
            pass
    if result['day'] == result['night']:
        result['day'], result['night'] = DEFAULTS['day'], DEFAULTS['night']
    wallpaper = values.get('wallpaper')
    if isinstance(wallpaper, dict) and wallpaper.get('palette') in PALETTE_NAMES and wallpaper.get('appearance') in ('light', 'dark'):
        result['wallpaper'] = dict(palette=wallpaper['palette'], appearance=wallpaper['appearance'])
    return result


def appearance(settings, now):
    if settings['appearance'] != 'auto':
        return settings['appearance']
    day, night = minute(settings['day']), minute(settings['night'])
    current = now.hour * 60 + now.minute
    light = day <= current < night if day < night else current >= day or current < night
    return 'light' if light else 'dark'


def state_directory(path):
    path.parent.mkdir(parents=True, mode=0o700, exist_ok=True)
    fd = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        info = os.fstat(fd)
        if info.st_uid != os.getuid() or info.st_mode & 0o022:
            raise ValueError('Settings directory must be owned and writable only by you')
        return fd
    except BaseException:
        os.close(fd)
        raise


def private_state_file(fd):
    info = os.fstat(fd)
    if (not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid()
            or info.st_nlink != 1 or info.st_mode & 0o077):
        raise ValueError('Settings file must be a private owned regular file')
    return info


def read_state(directory, name):
    try:
        fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK,
                     dir_fd=directory)
    except FileNotFoundError:
        return None, None
    try:
        info = private_state_file(fd)
        if info.st_size > STATE_LIMIT:
            raise ValueError('Settings file exceeds 8192 bytes')
        data = bytearray()
        while len(data) <= STATE_LIMIT:
            chunk = os.read(fd, STATE_LIMIT + 1 - len(data))
            if not chunk:
                break
            data.extend(chunk)
        if len(data) > STATE_LIMIT:
            raise ValueError('Settings file exceeds 8192 bytes')
        return bytes(data), hashlib.sha256(data).hexdigest()
    finally:
        os.close(fd)


def load_settings(path):
    directory = state_directory(path)
    try:
        data, revision = read_state(directory, path.name)
    finally:
        os.close(directory)
    if data is None:
        return validate_settings({}), revision
    def unique_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('Duplicate settings field: ' + key)
            result[key] = value
        return result
    try:
        values = json.loads(data, object_pairs_hook=unique_pairs)
    except (UnicodeError, RecursionError) as error:
        raise ValueError('Invalid settings JSON') from error
    if not isinstance(values, dict):
        raise ValueError('Settings must be a JSON object')
    if 'schema_version' not in values:
        return validate_settings(values), revision
    version = values.pop('schema_version')
    if type(version) is not int or version != 1:
        raise ValueError('Unsupported settings schema; file was preserved')
    return checked_settings(values), revision


def save_settings(path, settings, expected=UNCHECKED_REVISION):
    """Serialize writers and refuse stale snapshots before private replacement.

    All operations use the opened directory, including temporary cleanup. The
    lock is nonblocking: another session cannot stall the module event loop.
    """
    values = dict(checked_settings(settings), schema_version=1)
    data = (json.dumps(values, indent=2) + '\n').encode('utf-8')
    directory = state_directory(path)
    lock = None
    name = None
    try:
        lock = os.open('.desktop-state.lock', os.O_CREAT | os.O_RDWR |
                       os.O_NOFOLLOW | os.O_NONBLOCK, 0o600, dir_fd=directory)
        private_state_file(lock)
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        _, revision = read_state(directory, path.name)
        if expected is not UNCHECKED_REVISION and revision != expected:
            raise ValueError('Settings changed externally; reload before saving')
        temporary = '.desktop-' + secrets.token_hex(16)
        fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL |
                     os.O_NOFOLLOW, 0o600, dir_fd=directory)
        name = temporary
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, path.name, src_dir_fd=directory, dst_dir_fd=directory)
        name = None
        # Replacement is the commit point. No fallible operation after it may
        # report refusal while the in-memory state still describes old bytes.
        return hashlib.sha256(data).hexdigest()
    finally:
        if name is not None:
            os.unlink(name, dir_fd=directory)
        if lock is not None:
            os.close(lock)
        os.close(directory)


def command_output(argv, timeout=1):
    try:
        result = subprocess.run(argv, stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                timeout=timeout, check=False, close_fds=True,
                                env=dict(os.environ, LC_ALL='C'))
        if result.returncode == 0 and len(result.stdout) <= 65536:
            return result.stdout.decode('utf-8', 'replace').strip()
    except (OSError, subprocess.TimeoutExpired):
        pass
    return ''


def desktop_exec(value, name, filename, icon=''):
    """Expand supported desktop-entry field codes without invoking a shell."""
    words = shlex.split(value)
    result = []
    for word in words:
        if word in ('%f', '%F', '%u', '%U'):
            continue  # No file/URL was selected by this application launch.
        if word == '%i':
            if icon:
                result.extend(('--icon', icon))
        elif word == '%c':
            result.append(name)
        elif word == '%k':
            result.append(str(filename))
        else:
            literal = word.replace('%%', '\x01')
            if '%' in literal:
                raise ValueError('unsupported/embedded desktop Exec field code')
            result.append(literal.replace('\x01', '%'))
    if not result:
        raise ValueError('empty Exec')
    return result


def application_dirs():
    home = Path.home()
    user = Path(os.environ.get('XDG_DATA_HOME', str(home / '.local/share')))
    system = os.environ.get('XDG_DATA_DIRS', '/usr/local/share:/usr/share')
    return [user / 'applications'] + [Path(p) / 'applications'
                                       for p in system.split(':') if p]


def discover_apps(directories):
    result, seen = [], set()
    desktop = set(os.environ.get('XDG_CURRENT_DESKTOP', 'FVWM').split(':'))
    for directory in directories:
        if not directory.is_dir():
            continue
        for path in sorted(itertools.islice(directory.rglob('*.desktop'), 2048)):
            identity = str(path.relative_to(directory)).replace('/', '-')
            if identity in seen:
                continue
            seen.add(identity)  # A user Hidden entry masks the system entry.
            try:
                if path.stat().st_size > 65536:
                    continue
                parser = configparser.ConfigParser(interpolation=None, strict=False)
                parser.read(path, encoding='utf-8')
                entry = parser['Desktop Entry']
                if entry.get('Type') != 'Application':
                    continue
                if any(entry.get(k, '').lower() == 'true' for k in ('Hidden', 'NoDisplay')):
                    continue
                only = set(filter(None, entry.get('OnlyShowIn', '').split(';')))
                excluded = set(filter(None, entry.get('NotShowIn', '').split(';')))
                if (only and not only & desktop) or excluded & desktop:
                    continue
                if entry.get('TryExec') and not shutil.which(entry['TryExec']):
                    continue
                name = entry.get('Name', path.stem)
                argv = desktop_exec(entry['Exec'], name, path, entry.get('Icon', ''))
                if not shutil.which(argv[0]):
                    continue
                cwd = entry.get('Path') or None
                if cwd and not Path(cwd).is_dir():
                    continue
                result.append(dict(name=name, argv=argv, cwd=cwd,
                                   terminal=entry.get('Terminal', '').lower() == 'true'))
            except (OSError, ValueError, KeyError, configparser.Error):
                continue
            if len(result) >= 512:
                return sorted(result, key=lambda item: item['name'].casefold())
    return sorted(result, key=lambda item: item['name'].casefold())


def detect_terminal(value=''):
    if value:
        argv = shlex.split(value)
        return argv if argv and shutil.which(argv[0]) else []
    for name in ('xterm', 'uxterm', 'rxvt'):
        executable = shutil.which(name)
        if executable:
            return [executable]
    return []


def collect_status(system):
    """Return counters, not formatted FVWM commands. No dependency on top/btop."""
    status = dict(cpu=None, memory=None, network='Unavailable', battery=None)
    try:
        if system == 'Linux':
            counters = list(map(int, Path('/proc/stat').read_text().splitlines()[0].split()[1:9]))
            status['cpu'] = (sum(counters), counters[3] + counters[4])
            memory = dict(line.split(':', 1) for line in Path('/proc/meminfo').read_text().splitlines())
            total = int(memory['MemTotal'].split()[0])
            available = int(memory.get('MemAvailable', memory['MemFree']).split()[0])
            status['memory'] = max(0, total - available) * 1024
            links = [p for p in Path('/sys/class/net').iterdir() if p.name != 'lo']
            up = [p.name for p in links if (p / 'operstate').read_text().strip() == 'up']
            status['network'] = ', '.join(up) if up else 'Disconnected'
            batteries = []
            for device in Path('/sys/class/power_supply').glob('*'):
                if (device / 'type').read_text().strip() == 'Battery':
                    batteries.append(int((device / 'capacity').read_text()))
            if batteries:
                status['battery'] = round(sum(batteries) / len(batteries))
        elif system in ('FreeBSD', 'OpenBSD'):
            # FreeBSD separates counters with spaces; OpenBSD uses commas and
            # has an additional state. Both put the idle counter last.
            counters = list(map(int, re.findall(r'\d+', command_output(['sysctl', '-n', 'kern.cp_time']))))
            if counters:
                status['cpu'] = (sum(counters), counters[-1])
            total = command_output(['sysctl', '-n', 'hw.physmem'])
            page_size = command_output(['sysctl', '-n', 'hw.pagesize'])
            if system == 'FreeBSD':
                free = command_output(['sysctl', '-n', 'vm.stats.vm.v_free_count'])
                battery = command_output(['sysctl', '-n', 'hw.acpi.battery.life'])
            else:
                match = re.search(r'^\s*(\d+) pages free$',
                                  command_output(['vmstat', '-s']), re.M)
                free = match.group(1) if match else ''
                state = command_output(['apm', '-b'])
                battery = command_output(['apm', '-l']) if state in ('0', '1', '2', '3') else ''
            if total.isdigit() and free.isdigit() and page_size.isdigit():
                status['memory'] = max(0, int(total) - int(free) * int(page_size))
            if battery.isdigit() and 0 <= int(battery) <= 100:
                status['battery'] = int(battery)
            links = command_output(['ifconfig', '-a'])
            active = []
            current = ''
            for line in links.splitlines():
                if line and not line[0].isspace():
                    current = line.split(':')[0]
                elif 'status: active' in line and not current.startswith('lo'):
                    active.append(current)
            status['network'] = ', '.join(active) if active else 'Disconnected'
    except (OSError, ValueError, KeyError, IndexError):
        pass
    return status


def png_bytes(width, height, rows):
    def chunk(kind, data):
        crc = zlib.crc32(kind + data) & 0xffffffff
        return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', crc)
    raw = b''.join(b'\0' + row for row in rows)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!IIBBBBB', width, height, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def screenshot(directory):
    """Use the existing Xlib connection ABI; encode PNG with stdlib zlib.

    XGetPixel handles visual masks, depth and endianness (including 16-bit X).
    This intentionally happens in a worker with its own display connection.
    """
    class Image(C.Structure):
        _fields_ = [('width', C.c_int), ('height', C.c_int), ('xoffset', C.c_int),
                    ('format', C.c_int), ('data', C.c_void_p), ('byte_order', C.c_int),
                    ('bitmap_unit', C.c_int), ('bitmap_bit_order', C.c_int),
                    ('bitmap_pad', C.c_int), ('depth', C.c_int),
                    ('bytes_per_line', C.c_int), ('bits_per_pixel', C.c_int),
                    ('red_mask', C.c_ulong), ('green_mask', C.c_ulong),
                    ('blue_mask', C.c_ulong)]
    x = C.CDLL(ctypes.util.find_library('X11') or 'libX11.so')
    for name, args, result in (
        ('XOpenDisplay', [C.c_char_p], C.c_void_p),
        ('XDefaultScreen', [C.c_void_p], C.c_int),
        ('XRootWindow', [C.c_void_p, C.c_int], C.c_ulong),
        ('XDisplayWidth', [C.c_void_p, C.c_int], C.c_int),
        ('XDisplayHeight', [C.c_void_p, C.c_int], C.c_int),
        ('XGetImage', [C.c_void_p, C.c_ulong, C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_ulong, C.c_int], C.POINTER(Image)),
        ('XGetPixel', [C.POINTER(Image), C.c_int, C.c_int], C.c_ulong),
        ('XDestroyImage', [C.POINTER(Image)], C.c_int),
        ('XCloseDisplay', [C.c_void_p], C.c_int),
        ('XSync', [C.c_void_p, C.c_int], C.c_int),
        ('XSetErrorHandler', [C.c_void_p], C.c_void_p),
    ):
        function = getattr(x, name)
        function.argtypes, function.restype = args, result
    display = None
    # Connection retries are bounded and stay in the capture worker. A brief
    # local-server connection failure should not immediately lose the request.
    for attempt in range(3):
        display = x.XOpenDisplay(None)
        if display:
            break
        if attempt < 2:
            time.sleep(0.1)
    if not display:
        raise RuntimeError('Cannot open the X display')
    # A concurrent RandR resize may invalidate the capture rectangle. Contain
    # BadMatch here instead of letting Xlib terminate the desktop helper.
    handler_type = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_void_p)
    handler = handler_type(lambda _display, _event: 0)
    previous_handler = x.XSetErrorHandler(C.cast(handler, C.c_void_p))
    image = None
    try:
        screen = x.XDefaultScreen(display)
        width, height = x.XDisplayWidth(display, screen), x.XDisplayHeight(display, screen)
        if width <= 0 or height <= 0 or width * height > 16777216:
            raise RuntimeError('Screenshot exceeds the 16 megapixel limit')
        image = x.XGetImage(display, x.XRootWindow(display, screen), 0, 0,
                            width, height, C.c_ulong(-1).value, 2)
        if not image:
            raise RuntimeError('X server cannot capture this display')
        channels = []
        for mask in (image.contents.red_mask, image.contents.green_mask, image.contents.blue_mask):
            if not mask:
                raise RuntimeError('Screenshot needs a TrueColor visual')
            shift = (mask & -mask).bit_length() - 1
            channels.append((mask, shift, mask >> shift))
        rows = []
        info = image.contents
        for y in range(height):
            row = bytearray()
            # Decode common ZPixmap formats directly. Respect server byte order
            # and padded scanlines; XGetPixel handles less common pixel layouts.
            if (info.bits_per_pixel in (24, 32) and info.xoffset == 0 and
                    (info.red_mask, info.green_mask, info.blue_mask) == (0xff0000, 0xff00, 0xff)):
                size = info.bits_per_pixel // 8
                data = C.string_at(info.data + y * info.bytes_per_line, width * size)
                offsets = (2, 1, 0) if info.byte_order == 0 else tuple(range(size - 3, size))
                row = bytearray(width * 3)
                for channel, offset in enumerate(offsets):
                    row[channel::3] = data[offset::size]
                rows.append(row)
                continue
            if info.bits_per_pixel in (16, 32) and info.xoffset == 0:
                size = info.bits_per_pixel // 8
                data = C.string_at(info.data + y * info.bytes_per_line, width * size)
                fmt = ('<' if info.byte_order == 0 else '>') + ('H' if size == 2 else 'I')
                pixels = (item[0] for item in struct.iter_unpack(fmt, data))
            else:
                pixels = (x.XGetPixel(image, column, y) for column in range(width))
            for pixel in pixels:
                row.extend(((pixel & mask) >> shift) * 255 // maximum
                           for mask, shift, maximum in channels)
            rows.append(row)
        directory.mkdir(parents=True, exist_ok=True)
        fd, filename = tempfile.mkstemp(prefix='FVWM-X-' + dt.datetime.now().strftime('%Y%m%d-%H%M%S-'),
                                        suffix='.png', dir=directory)
        with os.fdopen(fd, 'wb') as stream:
            stream.write(png_bytes(width, height, rows))
        return 'Screenshot saved: ' + filename
    finally:
        if image:
            x.XDestroyImage(image)
        x.XSync(display, 0)
        x.XCloseDisplay(display)
        x.XSetErrorHandler(previous_handler)


class Desktop:
    def __init__(self, send, root, state_path):
        self.send = send
        self.root = root
        self.state_path = state_path
        self.palettes = json.loads((root / 'palettes.json').read_text())
        self.persistence_error = None
        try:
            self.saved, self.state_revision = load_settings(state_path)
        except (OSError, ValueError):
            self.saved, self.state_revision = validate_settings({}), None
            self.persistence_error = 'Cannot read private settings; repair the file then reload'
            print('FvwmDesktop: ' + self.persistence_error, file=sys.stderr)
        self.overrides = {}
        self.settings = validate_settings(self.saved)
        self.terminal = detect_terminal(os.environ.get('FVWM_TERMINAL', ''))
        self.system = os.uname().sysname
        self.apps, self.files = [], []
        self.last_theme, self.last_wallpaper = None, None
        self.last_cpu = None
        self.status = dict(cpu=None, memory=None, network='Checking...', battery=None)
        self.pool = concurrent.futures.ThreadPoolExecutor(max_workers=2)
        self.jobs = {}
        self.desk = 0
        self.last_clock = None
        self.monitor_width = 1440

    def configure_setting(self, key, value):
        self.overrides[key] = setting_value(key, value, text=True)
        # Configuration lines arrive individually. Check the combined schedule
        # at END_CONFIG_INFO, so a valid two-line change has no transient refusal.

    def finish_configuration(self):
        try:
            self.settings = checked_settings(dict(self.saved, **self.overrides))
        except ValueError as error:
            print('FvwmDesktop: ' + str(error) + '; ignoring schedule overrides',
                  file=sys.stderr)
            self.overrides.pop('day', None)
            self.overrides.pop('night', None)
            self.settings = checked_settings(dict(self.saved, **self.overrides))

    def change_settings(self, changes):
        if self.persistence_error:
            raise ValueError(self.persistence_error)
        for key, value in changes.items():
            setting_value(key, value)
        candidate = checked_settings(dict(self.saved, **changes))
        effective = checked_settings(dict(candidate, **self.overrides))
        mode = appearance(effective, dt.datetime.now())
        candidate['wallpaper'] = (self.settings['wallpaper'].copy()
            if effective['keep_wallpaper'] else
            dict(palette=effective['palette'], appearance=mode))
        effective['wallpaper'] = candidate['wallpaper'].copy()
        revision = save_settings(self.state_path, candidate, self.state_revision)
        self.saved, self.settings, self.state_revision = candidate, effective, revision
        self.theme(force=True)

    def reset_settings(self, scope):
        self.change_settings({key: SETTINGS[key]['default'] for key in setting_scope(scope)})

    def reload_settings(self):
        saved, revision = load_settings(self.state_path)
        effective = checked_settings(dict(saved, **self.overrides))
        self.saved, self.settings, self.state_revision = saved, effective, revision
        self.persistence_error = None
        self.theme(force=True)

    def query_settings(self, scope='all'):
        return {key: dict(SETTINGS[key], owner='FvwmDesktop', available=True,
                    capability='owned-desktop-preference', preview=False,
                    saved=self.saved[key], effective=self.settings[key],
                    source='module-configuration' if key in self.overrides else 'saved',
                    application='submitted-to-fvwm' if self.last_theme else 'not-submitted',
                    persistence_error=self.persistence_error)
                for key in setting_scope(scope)}

    def settings_menu(self, scope='all'):
        rows = []
        for key, item in self.query_settings(scope).items():
            name = 'SlateSetting_' + key
            label = key + ': ' + str(item['effective']).lower()
            choices = item.get('choices', (True, False) if item['type'] == 'boolean' else ())
            options = [(item['description'], 'Greyed'),
                       ('Saved: ' + str(item['saved']).lower(), 'Greyed'),
                       ('Source: ' + item['source'], 'Greyed')]
            if choices:
                options += [(str(value).lower(), 'SendToModule FvwmDesktop set ' +
                             key + ' ' + str(value).lower()) for value in choices]
            else:
                options += [('Edit schedule...', 'SendToModule FvwmDesktop schedule')]
            options += [('Reset this setting', 'SendToModule FvwmDesktop reset ' + key)]
            self.menu(name, key, options)
            rows.append((label, 'Popup ' + name))
        rows += [('', 'Nop'), ('Reset ' + scope, 'SendToModule FvwmDesktop reset ' + scope),
                 ('Reload saved settings', 'SendToModule FvwmDesktop reload-settings'),
                 ('FVWM commands submitted; external Style/Colorset overrides are unobserved', 'Greyed')]
        if self.persistence_error:
            rows.append((self.persistence_error, 'Greyed'))
        self.menu('SlateSettings', 'Desktop settings', rows)

    def submit(self, key, function, *args):
        if key not in self.jobs and len(self.jobs) < 4:
            self.jobs[key] = self.pool.submit(function, *args)

    def menu(self, name, title, rows):
        self.send('DestroyMenu recreate ' + name)
        self.send('AddToMenu ' + name + ' ' + quote(title) + ' Title')
        for label, action in rows:
            # Greyed is AddToMenu's label prefix, not an executable action.
            # Informational/unavailable rows must never become selectable.
            if action == 'Greyed':
                self.send('AddToMenu ' + name + ' Greyed ' + quote(label))
            else:
                self.send('AddToMenu ' + name + ' ' + quote(label) + ' ' + action)

    def button(self, name, value):
        self.send('SendToModule SlateBar ChangeButton ' + name + ' Title ' + quote(value))

    def notice(self, message):
        self.send('KillModule SlateNoticeForm')
        self.send('DestroyModuleConfig SlateNoticeForm:*')
        for command in ('Title "FVWM-X"', 'Position -16 -64', 'Line left', 'Text ' + quote(message),
                        'Line right', 'Button quit "Dismiss" ^[', 'Command Nop',
                        'Timeout 80 Nop ""'):
            self.send('*SlateNoticeForm: ' + command)
        self.send('Module FvwmForm SlateNoticeForm')

    def spawn(self, argv, cwd=None):
        try:
            subprocess.Popen(argv, cwd=cwd, stdin=subprocess.DEVNULL,
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                             close_fds=True, start_new_session=True)
        except OSError as error:
            self.notice('Could not start program: ' + str(error))

    def term(self, argv=None, cwd=None):
        if self.terminal:
            self.spawn(self.terminal + (['-e'] + argv if argv else []), cwd)
        else:
            self.notice('No terminal found. Set FVWM_TERMINAL to your terminal command.')

    def theme(self, force=False):
        mode = appearance(self.settings, dt.datetime.now())
        key = (self.settings['palette'], mode, self.settings['transparency'], self.settings['motion'])
        if not force and key == self.last_theme:
            return
        p = self.palettes[key[0]][mode]
        roles = {0:('text','surface'), 1:('muted','surface'), 2:('text','raised'),
                 3:('border','border'), 4:('accent','accent'), 5:('text','surface'),
                 6:('on_accent','accent'), 7:('muted','surface'), 8:('accent','surface'),
                 10:('text','surface'), 11:('on_accent','accent'), 12:('text','raised'),
                 13:('on_accent','accent'), 14:('muted','surface')}
        for number, (fg, bg) in roles.items():
            self.send(f'Colorset {number} Plain, fg {p[fg]}, bg {p[bg]}, hi {p[bg]}, sh {p[bg]}')
        if not self.settings['keep_wallpaper']:
            self.settings['wallpaper'] = dict(palette=key[0], appearance=mode)
        selected = self.settings['wallpaper']
        wallpaper = (selected['palette'], selected['appearance'])
        if wallpaper != self.last_wallpaper:
            path = self.root / 'wallpapers' / f'{wallpaper[0]}-{wallpaper[1]}.png'
            background = self.palettes[wallpaper[0]][wallpaper[1]]['background']
            self.send('Colorset 20 Plain, bg ' + background)
            if '@DESKTOP_PNG@' != 'false':
                self.send('Colorset 20 AspectPixmap ' + quote(path))
            self.last_wallpaper = wallpaper
        if self.settings['transparency']:
            self.send(f'Colorset 10 RootTransparent buffer, Tint {p["surface"]} 88')
            self.send('MenuStyle * Translucent 94')
        else:
            self.send('MenuStyle * !Translucent')
        # Menu Animation is submenu repositioning, not a compositor fade.
        self.send('MenuStyle * ' + ('Animation' if self.settings['motion'] else '!Animation'))
        self.send('CopyMenuStyle * SlateLauncherStyle')
        self.send('MenuStyle SlateLauncherStyle ItemFormat "%24p%l%80p%>%24p"')
        self.send('Style * WindowShadeSteps ' + ('6' if self.settings['motion'] else '0'))
        self.button('appearance', key[0].title())
        self.last_theme = key
        self.appearance_menu()
        self.settings_menu()
        self.highlight_desk()

    def appearance_menu(self):
        def selected(label, value, current):
            return ('* ' if value == current else '  ') + label
        rows = [(selected(n.title(), n, self.settings['palette']),
                 'SendToModule FvwmDesktop set palette ' + n) for n in PALETTE_NAMES]
        rows += [('', 'Nop')]
        rows += [(selected(n.title(), n, self.settings['appearance']),
                  'SendToModule FvwmDesktop set appearance ' + n)
                 for n in ('light', 'dark', 'auto')]
        rows += [('Day ' + self.settings['day'] + ' / Night ' + self.settings['night'],
                  'SendToModule FvwmDesktop schedule')]
        for key, label in (('transparency', 'Transparency'), ('motion', 'Motion'),
                           ('keep_wallpaper', 'Keep current wallpaper')):
            rows.append((label + ('  On' if self.settings[key] else '  Off'),
                         'SendToModule FvwmDesktop toggle ' + key))
        rows += [('Refresh application list', 'SendToModule FvwmDesktop discover')]
        rows += [('Desktop settings...', 'SendToModule FvwmDesktop settings')]
        self.menu('SlateAppearance', 'Appearance', rows)

    def highlight_desk(self):
        for desk in range(4):
            self.send(f'SendToModule SlateBar ChangeButton desk{desk} Colorset ' +
                      ('11' if desk == self.desk else '10'))

    def application_menu(self, query=None):
        indexes = [i for i, app in enumerate(self.apps)
                   if query is None or query.casefold() in app['name'].casefold()]
        # Paginate rather than letting a large installed app set dominate the screen.
        rows = []
        for page in range(0, len(indexes), 20):
            group = indexes[page:page + 20]
            name = ('SlateApps' if query is None else 'SlateResults') + str(page // 20)
            items = [(self.apps[i]['name'], f'SendToModule FvwmDesktop launch {i}') for i in group]
            self.menu(name, 'Applications', items)
            rows.append((self.apps[group[0]]['name'] + ' ...', 'Popup ' + name))
        if query is not None:
            items = [(self.apps[i]['name'], f'SendToModule FvwmDesktop launch {i}')
                     for i in indexes[:20]] if len(indexes) <= 20 else rows
            self.menu('SlateSearchResults', 'Search: ' + query,
                      items or [('No matching applications', 'Greyed')])
            self.send('Menu SlateSearchResults Monitor c c')
            return
        if len(indexes) <= 20:
            rows = [(self.apps[i]['name'], f'SendToModule FvwmDesktop launch {i}') for i in indexes]
        rows += [('', 'Nop'), ('Terminal', 'SendToModule FvwmDesktop terminal'),
                 ('Search applications...', 'Module FvwmForm SlateSearchForm'),
                 ('Run command...', 'Module FvwmForm SlateRunForm'),
                 ('Files', 'SendToModule FvwmDesktop files'),
                 ('Workspaces', 'Module FvwmPager 0 3'),
                 ('Appearance', 'Popup SlateAppearance'), ('System', 'Popup SlateSystem'),
                 ('Session', 'Popup SlateSession')]
        self.menu('SlateLauncherMenu', 'Applications', rows)
        self.send('ChangeMenuStyle SlateLauncherStyle SlateLauncherMenu')

    def system_menu(self):
        self.menu('SlateSystem', 'System', [
            ('Network: ' + self.status['network'], 'SendToModule FvwmDesktop network'),
            ('Sound', 'Popup SlateSound'),
            ('Battery: ' + (str(self.status['battery']) + '%' if self.status['battery'] is not None else 'not reported'), 'Nop'),
            ('Calendar', 'SendToModule FvwmDesktop calendar'),
            ('Display information', 'SendToModule FvwmDesktop displays'),
            ('Screenshot', 'SendToModule FvwmDesktop screenshot'),
            ('System monitor', 'SendToModule FvwmDesktop monitor')])
        sound = shutil.which('sndioctl') if self.system == 'OpenBSD' else shutil.which('mixer') if self.system == 'FreeBSD' else shutil.which('amixer')
        self.menu('SlateSound', 'Sound', [(label, 'SendToModule FvwmDesktop volume ' + action)
                    for label, action in [('Volume up', 'up'), ('Volume down', 'down'), ('Mute / unmute', 'mute')]]
                  if sound else [('No sound control backend is available', 'Greyed')])
        lock = bool(shutil.which('xlock') or shutil.which('slock'))
        rows = [('Lock', 'SendToModule FvwmDesktop lock' if lock else 'Greyed'),
                ('Log out...', 'SendToModule FvwmDesktop confirm logout'),
                ('Restart FVWM', 'Restart'),
                ('Power off...', 'SendToModule FvwmDesktop confirm poweroff'),
                ('Reboot...', 'SendToModule FvwmDesktop confirm reboot')]
        self.menu('SlateSession', 'Session', rows)

    def poll_jobs(self):
        for key, future in list(self.jobs.items()):
            if not future.done():
                continue
            del self.jobs[key]
            try:
                value = future.result()
                if key == 'apps':
                    self.apps = value
                    self.application_menu()
                elif key == 'status':
                    self.status = value
                    cpu = '--'
                    counters = value['cpu']
                    if counters and self.last_cpu:
                        total, idle = counters[0] - self.last_cpu[0], counters[1] - self.last_cpu[1]
                        if total > 0 and idle >= 0:
                            cpu = str(round(100 * max(0, min(total, total - idle)) / total)) + '%'
                    self.last_cpu = counters
                    memory = '--' if value['memory'] is None else f'{value["memory"] / 1073741824:.1f}G'
                    text = f'CPU {cpu}  RAM {memory}'
                    if value['battery'] is not None and self.monitor_width >= 1600:
                        text += '  BAT ' + str(value['battery']) + '%'
                    self.button('status', text)
                    self.system_menu()
                elif key == 'files':
                    directory, self.files = value
                    rows = [('Up one folder', 'SendToModule FvwmDesktop files-up'),
                            ('Home', 'SendToModule FvwmDesktop files'),
                            ('Open terminal here', 'SendToModule FvwmDesktop files-terminal'), ('', 'Nop')]
                    rows += [(('> ' if p.is_dir() else '') + p.name,
                              f'SendToModule FvwmDesktop file {i}') for i, p in enumerate(self.files)]
                    self.directory = directory
                    self.menu('SlateFiles', str(directory), rows or [('Empty folder', 'Greyed')])
                    self.send('Menu SlateFiles Monitor c c')
                elif key == 'screenshot':
                    self.notice(value)
                elif key == 'volume':
                    self.notice(value or 'Sound control is unavailable or permission was denied.')
            except Exception as error:
                self.notice(str(error))

    def handle(self, message):
        command, _, argument = message.strip().partition(' ')
        try:
            if command == 'barrier' and re.fullmatch(r'[A-Za-z0-9_-]{1,64}', argument):
                # Echo follows earlier commands in this module's ordered pipe.
                # Saved settings alone do not mean that FVWM has repainted yet.
                self.send('Echo ' + argument)
            elif command in ('set', 'toggle'):
                if (command == 'toggle' and argument in SETTINGS
                        and SETTINGS[argument]['type'] == 'boolean'):
                    changes = {argument: not self.saved[argument]}
                elif command == 'set':
                    key, _, value = argument.partition(' ')
                    changes = {key: setting_value(key, value, text=True)}
                else:
                    raise ValueError('Unknown setting')
                self.change_settings(changes)
            elif command == 'reset':
                self.reset_settings(argument)
            elif command == 'reload-settings':
                if argument:
                    raise ValueError('reload-settings takes no arguments')
                self.reload_settings()
            elif command == 'settings':
                self.settings_menu(argument or 'all')
                self.send('Menu SlateSettings Monitor c c')
            elif command == 'query-settings':
                token, _, scope = argument.partition(' ')
                if not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', token):
                    raise ValueError('Invalid settings query token')
                for key, item in self.query_settings(scope or 'all').items():
                    # Each reply fits the core's existing 1000-byte command
                    # line; do not submit one oversized aggregate Echo.
                    self.send('Echo desktop-settings-' + token + ' ' + key + ' ' +
                              json.dumps(item, separators=(',', ':')))
                self.send('Echo desktop-settings-' + token + ' done')
            elif command == 'times':
                day, night = argument.split()
                self.change_settings({'day': day, 'night': night})
            elif command == 'schedule':
                self.send('DestroyModuleConfig SlateScheduleForm:*')
                for line in ('Title "Day / night schedule"', 'Line left',
                             'Text "Day starts (HH:MM)"', 'Input day 5 ' + quote(self.settings['day']),
                             'Line left', 'Text "Night starts (HH:MM)"',
                             'Input night 5 ' + quote(self.settings['night']), 'Line expand',
                             'Button quit "Save" ^M', 'Command SendToModule FvwmDesktop times $(day) $(night)',
                             'Button quit "Cancel" ^[', 'Command Nop'):
                    self.send('*SlateScheduleForm: ' + line)
                self.send('Module FvwmForm SlateScheduleForm')
            elif command == 'refresh':
                if argument.isdigit():
                    self.monitor_width = int(argument)
                self.theme(force=True)
                self.last_clock = None
            elif command == 'discover':
                self.submit('apps', discover_apps, application_dirs())
            elif command == 'search':
                self.application_menu(argument[:80])
            elif command == 'launch':
                index = int(argument)
                if not 0 <= index < len(self.apps):
                    raise ValueError('Application selection is out of range')
                app = self.apps[index]
                if app['terminal']:
                    self.term(app['argv'], app['cwd'])
                else:
                    self.spawn(app['argv'], app['cwd'])
            elif command == 'terminal':
                self.term()
            elif command == 'run' and argument:
                # Shell interpretation is intentional only for explicit Run input.
                self.spawn(['/bin/sh', '-c', argument])
            elif command == 'calendar':
                now = dt.datetime.now()
                rows = [(line, 'Nop') for line in calendar.TextCalendar().formatmonth(now.year, now.month).splitlines()]
                self.menu('SlateCalendar', 'Calendar', rows)
                self.send('Menu SlateCalendar Monitor c c')
            elif command == 'files':
                self.open_directory(Path.home())
            elif command == 'files-up':
                self.open_directory(self.directory.parent)
            elif command == 'files-terminal':
                self.term(cwd=self.directory)
            elif command == 'file':
                index = int(argument)
                if not 0 <= index < len(self.files):
                    raise ValueError('File selection is out of range')
                path = self.files[index]
                if path.is_dir():
                    self.open_directory(path)
                elif shutil.which('xdg-open'):
                    self.spawn(['xdg-open', str(path)])
                else:
                    self.term(['less', '--', str(path)])
            elif command == 'displays':
                self.term(['xrandr', '--query']) if shutil.which('xrandr') else self.notice('xrandr is unavailable on this system.')
            elif command == 'network':
                executable = shutil.which('ifconfig') or shutil.which('ip')
                self.term([executable, '-a' if Path(executable).name == 'ifconfig' else 'address']) if executable else self.notice('No network information command is available.')
            elif command == 'monitor':
                self.term(['top'])
            elif command == 'screenshot':
                self.submit('screenshot', screenshot, Path.home() / 'Pictures' / 'Screenshots')
            elif command == 'lock':
                executable = shutil.which('xlock') or shutil.which('slock')
                if executable:
                    self.spawn([executable])
                else:
                    self.notice('No secure X11 locker is available. The screen has NOT been locked.')
            elif command == 'volume' and argument in ('up', 'down', 'mute'):
                self.submit('volume', volume, self.system, argument)
            elif command == 'confirm' and argument in ('logout', 'poweroff', 'reboot'):
                self.confirm(argument)
            elif command == 'session' and argument in ('logout', 'poweroff', 'reboot'):
                if argument == 'logout':
                    self.send('Quit')
                else:
                    self.submit('volume', power_action, self.system, argument)
        except (ValueError, IndexError, AttributeError, OSError) as error:
            self.notice(str(error))

    def open_directory(self, directory):
        def listing(path):
            entries = itertools.islice(path.iterdir(), 1000)
            return path, sorted(entries, key=lambda p: (not p.is_dir(), p.name.casefold()))[:100]
        self.submit('files', listing, directory)

    def confirm(self, action):
        self.send('DestroyModuleConfig SlateSessionForm:*')
        for line in ('Title "Session"', 'Line left',
                     'Text ' + quote('Confirm ' + action + '? Save your work first.'),
                     'Line expand', 'Button quit "Cancel" ^[', 'Command Nop',
                     'Button quit ' + quote(action.title()),
                     'Command SendToModule FvwmDesktop session ' + action):
            self.send('*SlateSessionForm: ' + line)
        self.send('Module FvwmForm SlateSessionForm')

    def tick(self):
        now = dt.datetime.now()
        clock = now.strftime('%a %d %b  %H:%M')
        if clock != self.last_clock:
            self.button('clock', clock)
            self.last_clock = clock
            self.theme()
        self.poll_jobs()


def volume(system, action):
    if system == 'OpenBSD' and shutil.which('sndioctl'):
        if action == 'mute':
            current = command_output(['sndioctl', '-n', 'output.mute'])
            if current not in ('0', '1'):
                return ''
            setting = 'output.mute=' + ('0' if current == '1' else '1')
        else:
            setting = 'output.level=' + ('+0.05' if action == 'up' else '-0.05')
        return command_output(['sndioctl', setting])
    if system == 'FreeBSD' and shutil.which('mixer'):
        # FreeBSD's mixer(8) supports relative volume and mute toggling.
        return command_output(['mixer', 'vol.mute=toggle' if action == 'mute'
                               else 'vol.volume=' + ('+5%' if action == 'up' else '-5%')])
    if shutil.which('amixer'):
        return command_output(['amixer', 'sset', 'Master',
                               {'up':'5%+', 'down':'5%-', 'mute':'toggle'}[action]])
    return ''


def power_action(system, action):
    # No automatic sudo/doas: use existing session permissions, report failures.
    argv = ['systemctl', action] if system == 'Linux' and shutil.which('systemctl') else ['shutdown', '-p' if action == 'poweroff' else '-r', 'now']
    try:
        result = subprocess.run(argv, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL, close_fds=True, timeout=3)
        return 'Power request sent.' if result.returncode == 0 else 'Power request denied by the system. Check session permissions.'
    except (OSError, subprocess.TimeoutExpired):
        return 'Power service is unavailable or did not respond.'


def main():
    if len(sys.argv) < 6:
        print('FvwmDesktop must be started with Module FvwmDesktop.', file=sys.stderr)
        return 1
    out_fd, in_fd = map(int, sys.argv[1:3])
    # Protocol descriptors remain in this process and are closed in child execs.
    os.set_inheritable(out_fd, False)
    os.set_inheritable(in_fd, False)
    os.set_blocking(in_fd, False)
    os.set_blocking(out_fd, False)
    pending = bytearray()
    def send(message):
        payload = message.encode('utf-8')
        if len(payload) > 4096 or b'\0' in payload or b'\n' in payload:
            raise ValueError('Invalid module command')
        packet = WORD.pack(0) + WORD.pack(len(payload)) + payload + WORD.pack(1)
        if len(pending) + len(packet) > 1048576:
            raise RuntimeError('FVWM command queue exceeded its limit')
        pending.extend(packet)
    root = Path(os.environ.get('FVWM_DESKTOP_ROOT',
                               os.environ.get('FVWM_DATADIR', '@FVWM_DATADIR@') + '/default-config'))
    user = Path(os.environ.get('FVWM_USERDIR', str(Path.home() / '.fvwm')))
    desktop = Desktop(send, root, user / 'desktop-state.json')
    send('SET_MASK ' + str(M_STRING | M_NEW_DESK | M_CONFIG_INFO | M_END_CONFIG_INFO))
    send('Send_ConfigInfo *FvwmDesktop')
    selector = selectors.DefaultSelector()
    selector.register(in_fd, selectors.EVENT_READ)
    buffer = bytearray()
    started = False
    last_status = 0
    write_registered = False
    try:
        while True:
            if pending and not write_registered:
                selector.register(out_fd, selectors.EVENT_WRITE)
                write_registered = True
            elif not pending and write_registered:
                selector.unregister(out_fd)
                write_registered = False
            for event, _ in selector.select(0.25):
                if event.fd == out_fd:
                    try:
                        count = os.write(out_fd, pending[:8192])
                        del pending[:count]
                    except BlockingIOError:
                        pass
                    continue
                chunk = os.read(in_fd, 65536)
                if not chunk:
                    return 0
                buffer.extend(chunk)
            # Bounded packet consumption keeps both command output and timers live.
            for _ in range(64):
                if len(buffer) < HEADER.size:
                    break
                flag, kind, size, _stamp = HEADER.unpack_from(buffer)
                if flag != 0xffffffff or size < 4 or size > 4096:
                    raise RuntimeError('Invalid FVWM packet')
                length = size * WORD.size
                if len(buffer) < length:
                    break
                body = bytes(buffer[HEADER.size:length])
                del buffer[:length]
                if kind in (M_STRING, M_CONFIG_INFO):
                    message = body[3 * WORD.size:].split(b'\0', 1)[0].decode('utf-8', 'replace')
                    if kind == M_STRING:
                        desktop.handle(message)
                    elif desktop_configuration(message) is not None:
                        option, value = desktop_configuration(message)
                        if option in SETTINGS:
                            try:
                                desktop.configure_setting(option, value.strip())
                            except ValueError as error:
                                print('FvwmDesktop: ' + str(error), file=sys.stderr)
                        elif option == 'terminal':
                            desktop.terminal = detect_terminal(value.strip())
                elif kind == M_NEW_DESK and len(body) >= WORD.size:
                    desktop.desk = WORD.unpack_from(body)[0]
                    desktop.highlight_desk()
                elif kind == M_END_CONFIG_INFO and not started:
                    started = True
                    desktop.finish_configuration()
                    desktop.theme()
                    desktop.system_menu()
                    desktop.submit('apps', discover_apps, application_dirs())
                    send('NOP FINISHED STARTUP')
            if started:
                desktop.tick()
                if time.monotonic() - last_status >= 5:
                    desktop.submit('status', collect_status, desktop.system)
                    last_status = time.monotonic()
    except (BrokenPipeError, ConnectionResetError):
        return 0
    finally:
        selector.close()
        desktop.pool.shutdown(wait=False)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError) as error:
        print('FvwmDesktop: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
