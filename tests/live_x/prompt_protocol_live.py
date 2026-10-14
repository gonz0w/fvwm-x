#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Use actual MFL commands and installed Perl event consumers on a real WM."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fp-live-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            perl_result = runtime.root / 'perl-events.json'
            fixture = runtime.root / 'events.fpl'
            library = runtime.root / 'perllib'
            (library / 'FVWM').mkdir(parents=True)
            # Meson configures Module.pm outside its eventual FVWM/ directory.
            # Relocate those exact bytes for an uninstalled build-tree check.
            shutil.copy2(args.module_library, library / 'FVWM/Module.pm')
            perl_module = runtime.root / 'FvwmPerl'
            shutil.copy2(args.perl_module, perl_module)
            # configure_file output is 0644; its install contract is 0755.
            perl_module.chmod(0o755)
            fixture.write_text("""use JSON::PP;
my %seen;
$module->add_handler(MX_ECHO, sub {
    my ($mod, $event) = @_;
    return unless $event->_message eq 'native-perl-echo';
    $seen{echo} = $event->_message;
});
$module->add_handler(MX_REPLY, sub {
    my ($mod, $event) = @_;
    return unless $event->_text eq 'native-perl-reply';
    $seen{reply} = $event->_text;
    open(my $out, '>', $ENV{FVWM_PROTOCOL_RESULT}) or die $!;
    print $out encode_json(\\%seen), "\\n";
    close($out) or die $!;
    $mod->terminate;
});
$module->send('Echo native-perl-echo');
$module->request_reply('native-perl-reply');
""")
            env = runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'FVWM_PROTOCOL_RESULT': str(perl_result),
                'PERL5LIB': os.pathsep.join([str(library), str(args.perllib),
                    os.environ.get('PERL5LIB', '')]),
                'FVWM_USERDIR': str(runtime.root)})
            core = runtime.start('fvwm', [str(args.fvwm), '-d', display,
                '-c', 'ModulePath ' + str(args.mfl.parent),
                '-c', 'AddToFunc StartFunction I Module FvwmMFL',
                '-c', 'AddToFunc StartFunction I Module ' + str(perl_module) +
                      ' --load ' + str(fixture) + ' --stay'], 'fvwm.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            def connect_when_listening():
                try:
                    return Connection(endpoint)
                except (FileNotFoundError, ConnectionRefusedError):
                    return None

            # bind creates the pathname before the listener is ready. Keep
            # one bounded readiness wait; do not retry commands or responses.
            healthy = wait_for_predicate('MFL listener', connect_when_listening,
                time.monotonic()+10, process=core)
            try:
                assert 'connection_profile' in healthy.record()
                healthy.send(b'set echo\n')
                healthy.echo('subscription-ready')
                client_env = dict(env, FVWMMFL_SOCKET=str(endpoint))
                for command in [['Echo', 'prompt-one-shot'], ['-p', 'custom> ', 'Echo', 'prompt-option']]:
                    result = subprocess.run([str(args.prompt), *command], env=client_env,
                                            capture_output=True, timeout=10)
                    assert result.returncode == 0, result.stderr
                    healthy.expect_echo(command[-1])
                evidence['real_mfl_one_shot_and_option'] = 'passed'
                result = subprocess.run([str(args.prompt)], env=client_env,
                    input=b'Echo prompt-first\nEcho prompt-second\nexit\n',
                    capture_output=True, timeout=10)
                assert result.returncode == 0, result.stderr
                healthy.expect_echo('prompt-first')
                healthy.expect_echo('prompt-second')
                assert result.stderr.count(b'*FvwmPrompt') == 1
                evidence['real_mfl_interactive_order'] = 'passed'
                wait_for_predicate('native Perl Echo/Reply consumers', perl_result.exists,
                                   time.monotonic()+10, process=core)
                evidence['installed_perl_echo_reply'] = json.loads(perl_result.read_text())
                assert evidence['installed_perl_echo_reply'] == {
                    'echo':'native-perl-echo','reply':'native-perl-reply'}
                assert core.poll() is None
            finally:
                healthy.close()
        finally:
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fvwm','mfl','prompt','perl-module','perllib',
                 'module-library','artifacts-dir'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--display')
    options = parser.parse_args()
    for name, value in vars(options).items():
        if isinstance(value, Path):
            setattr(options, name, value.resolve())
    run(options)
