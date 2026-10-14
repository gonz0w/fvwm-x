#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Exercise bounded module selection requests against a private server."""
import argparse
import ctypes as C
import json
import shlex
import sys
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client
from mfl_stream_live import Connection
from live_x_harness import HarnessError, HarnessRuntime, start_display_server, wait_for_predicate


class SelectionRequest(C.Structure):
    _fields_ = [("type", C.c_int), ("serial", C.c_ulong),
                ("send_event", C.c_int), ("display", C.c_void_p)] + [
        (name, C.c_ulong) for name in
        ("owner", "requestor", "selection", "target", "property", "time")]


class SelectionNotify(C.Structure):
    _fields_ = SelectionRequest._fields_[:4] + [
        (name, C.c_ulong) for name in
        ("requestor", "selection", "target", "property", "time")]


class Button(C.Structure):
    _fields_ = [("type", C.c_int), ("serial", C.c_ulong),
                ("send_event", C.c_int), ("display", C.c_void_p)] + [
        (name, C.c_ulong) for name in ("window", "root", "subwindow", "time")] + [
        (name, C.c_int) for name in ("x", "y", "x_root", "y_root")] + [
        ("state", C.c_uint), ("button", C.c_uint), ("same_screen", C.c_int)]


class Key(C.Structure):
    _fields_ = Button._fields_[:-2] + [("keycode", C.c_uint), ("same_screen", C.c_int)]


class Event(C.Union):
    _fields_ = [("request", SelectionRequest), ("selection", SelectionNotify),
                ("button", Button), ("key", Key), ("pad", C.c_long * 24)]


def widgets(runtime, display, args):
    client = Client(display)
    connection = None
    trace = []
    evidence = {}
    form_state = runtime.root / 'form-state'
    try:
        x, d = client.x, client.display
        x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
        x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
        x.XInternAtom.restype = C.c_ulong
        x.XChangeProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
            C.c_ulong, C.c_int, C.c_int, C.c_void_p, C.c_int]
        x.XGetSelectionOwner.argtypes = [C.c_void_p, C.c_ulong]
        x.XGetSelectionOwner.restype = C.c_ulong
        x.XSetSelectionOwner.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong, C.c_ulong]
        x.XCheckTypedEvent.argtypes = [C.c_void_p, C.c_int, C.POINTER(Event)]
        x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.POINTER(Event)]
        x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
        x.XKeysymToKeycode.restype = C.c_uint
        atom = lambda name: x.XInternAtom(d, name.encode(), 0)
        primary, string = atom('PRIMARY'), atom('STRING')
        cut = atom('CUT_BUFFER0')
        child_script = runtime.root / 'MessageProbe'
        child_script.write_text('WindowTitle {MessageProbe}\nWindowSize 80 40\n'
            'Init\nBegin\n Set $sent=0\nEnd\n'
            'PeriodicTasks\nBegin\n If $sent==0 Then\n Begin\n'
            ' SendToScript (GetScriptFather) {hello-message}\n'
            ' Do {Echo child-argument:} (GetScriptArgument 19)\n'
            ' Set $sent=1\n End\nEnd\n'
            'Widget 1\nProperty\n Type ItemDraw\n Position 5 5\n'
            ' Size 60 20\n Title {Message}\nEnd\n')
        long_directory = runtime.root / ('long-path-' + 'p' * 130)
        long_directory.mkdir()
        once_script = long_directory / 'OnceProbe'
        once_script.write_text('WindowTitle {SingleLaunchProbe}\nWindowSize 80 40\n'
            'Widget 1\nProperty\n Type ItemDraw\n Position 5 5\n'
            ' Size 60 20\n Title {Once}\nEnd\n')
        script = runtime.root / 'SelectionProbe'
        script.write_text('WindowTitle {SelectionProbe}\nWindowSize 240 80\n'
            'Init\nBegin\n Set $received=0\n Set $launched=0\n'
            ' Set $child=(LaunchScript {'+str(child_script)+' '+
            ' '.join('arg'+str(i) for i in range(1,26))+'})\nEnd\n'
            'PeriodicTasks\nBegin\n If $launched==0 Then\n Begin\n'
            ' Do {Echo single-launch-id:} (LaunchScript {'+str(once_script)+'})\n'
            ' Set $launched=1\n End\n If $received==0 Then\n Begin\n'
            ' Set $reply=(ReceivFromScript $child)\n'
            ' If $reply <> {No message} Then\n Begin\n'
            ' Do {Echo script-message:} $reply\n Set $received=1\n End\n End\n'
            ' Do {Echo script-state:} (GetTitle 1)\nEnd\n'
            'Widget 1\nProperty\n Type TextField\n Position 10 10\n Size 200 24\n'
            ' Title {seed}\nEnd\n')
        script_output = runtime.root / 'script-output'
        malformed_output = runtime.root / 'script-malformed'
        malformed = '#'+str(script)+',1\ntruncated-section\n'
        malformed_output.write_text(malformed)
        valid_existing = 'prefix\n#'+str(script)+',1\nold\n#end\nsuffix\n'
        script_output.write_text(valid_existing)
        init_commands = ' Set $long={0123456789abcdef}\n'
        init_commands += ' Set $long=$long $long\n' * 8
        init_commands += (' Set $file={'+str(script_output)+'}\n WriteToFile $file $long\n'
            ' WriteToFile $file {}\n'
            ' Set $file={'+str(malformed_output)+'}\n WriteToFile $file {must-not-change}\n'
            ' Set $file={'+str(runtime.root/'missing'/'file')+'}\n WriteToFile $file {no}\n'
            ' Set $invalid={-1}\n Set $bad=(GetScriptArgument $invalid)\n'
            ' Set $invalid={999999999999999999999999}\n Set $bad=(GetScriptArgument $invalid)\n'
            ' Set $invalid={-1}\n SendToScript $invalid {rejected}\n'
            ' Set $invalid={999999999999999999999999}\n SendToScript $invalid {rejected}\n')
        output_data = runtime.root / 'output-data'
        output_data.write_text('w' * 32000)
        extracted_output = runtime.root / 'extracted-output'
        output_command = 'cat' + (' ' * 300) + shlex.quote(str(output_data))
        init_commands += (' Set $command={'+output_command+'}\n'
            ' Set $output=(GetOutput $command 0 0)\n'
            ' Set $file={'+str(extracted_output)+'}\n'
            ' WriteToFile $file $output\n'
            ' Set $output=(GetOutput $command 1 -1)\n'
            ' WriteToFile $file $output\n')
        descriptor_probe = runtime.root / 'descriptor-probe.py'
        descriptor_probe.write_text(
            'import os,stat\n'
            'leaks=[]\n'
            'for fd in range(3,1024):\n'
            ' try:\n'
            '  if stat.S_ISFIFO(os.fstat(fd).st_mode): leaks.append(fd)\n'
            ' except OSError: pass\n'
            'print("clean" if not leaks else "leaked:"+str(leaks))\n')
        descriptor_report = runtime.root / 'descriptor-report'
        descriptor_command = (shlex.quote(sys.executable)+' '+
            shlex.quote(str(descriptor_probe)))
        init_commands += (' Set $command={'+descriptor_command+'}\n'
            ' Set $output=(GetOutput $command 1 -1)\n'
            ' Set $file={'+str(descriptor_report)+'}\n'
            ' WriteToFile $file $output\n')
        widget_report = runtime.root / 'widget-index-report'
        invalid_widgets = ['-1', '0', '1001', '2',
                           '999999999999999999999999', '1junk']
        getters = ['GetTitle', 'GetValue', 'GetMinValue', 'GetMaxValue',
                   'GetFore', 'GetBack', 'GetHilight', 'GetShadow']
        mutations = ['HideWidget $invalid', 'ShowWidget $invalid',
            'ChangeValue $invalid 55', 'ChangeMaxValue $invalid 55',
            'ChangeMinValue $invalid 55', 'ChangePosition $invalid 20 20',
            'ChangeSize $invalid 20 20', 'ChangeTitle $invalid {changed}',
            'ChangeLocaleTitle $invalid {changed}', 'ChangeIcon $invalid $empty',
            'ChangeFont $invalid {fixed}', 'ChangeForeColor $invalid {red}',
            'ChangeBackColor $invalid {red}', 'SendSignal $invalid 1',
            'WarpPointer $invalid']
        init_commands += ' Set $empty={}\n'
        for invalid in invalid_widgets:
            init_commands += ' Set $invalid={'+invalid+'}\n'
            init_commands += ' Set $results=' + ' '.join(
                '('+getter+' $invalid)' for getter in getters) + '\n'
            init_commands += (' Set $file={'+str(widget_report)+'}\n'
                ' WriteToFile $file {case:} $invalid {:} $results\n')
            init_commands += ''.join(' '+command+'\n' for command in mutations)
            if invalid != '0':
                init_commands += ' ChangeColorset $invalid 0\n'
            init_commands += ' ChangeWindowTitleFromArg $invalid\n'
        init_commands += (' ChangeWindowTitle {SelectionProbe}\n'
            ' ChangeColorset 0 -1\n ChangeColorset 1 -1\n'
            ' Set $oversized={2147483647}\n ChangeColorset 0 $oversized\n'
            ' ChangeColorset 0 0\n'
            ' ChangeTitle 1000 {edge}\n ChangeValue 1000 23\n'
            ' HideWidget 1000\n ShowWidget 1000\n'
            ' Set $edge=(GetTitle 1000)\n Set $value=(GetValue 1000)\n'
            ' Set $file={'+str(widget_report)+'}\n'
            ' WriteToFile $file {edge:} $edge {:} $value\n')
        process_report = runtime.root / 'process-default-report'
        init_commands += (' Set $output=(GetOutput {sleep 6} 1 -1)\n'
            ' Set $file={'+str(process_report)+'}\n'
            ' WriteToFile $file {timeout:} $output\n'
            ' Set $output=(GetOutput {printf recovery} 1 -1)\n'
            ' WriteToFile $file {recovery:} $output\n')
        value_report = runtime.root / 'value-report'
        value_cases = [
            ('add', '(Add 7 9)', '16'),
            ('multiply', '(Mult -7 3)', '-21'),
            ('divide', '(Div -7 3)', '-2'),
            ('remainder', '(RemainderOfDiv -7 3)', '-1'),
            ('prefix', '(Add $prefix 2)', '25'),
            ('empty-number', '(Add $empty 2)', '2'),
            ('hex-padding', '(NumToHex 15 4)', '000F'),
            ('hex-negative', '(NumToHex -1 16)', '00000000FFFFFFFF'),
            ('hex-color', '(HexToNum {#0xFF})', '255'),
            ('hex-pattern', '(HexToNum {#FFFFFFFF})', '-1'),
            ('copy', '(StrCopy {Hello} 1 2)', 'He'),
            ('copy-clamp', '(StrCopy {Hello} -1 3)', 'Hel'),
            ('copy-tail', '(StrCopy {Hello} 3 $max)', 'llo'),
            ('copy-missing', '(StrCopy {Hello} 9 9)', ''),
            ('parse-record', '(Parse {0003one00000004four} 3)', 'four'),
            ('parse-empty', '(Parse {0003one00000004four} 2)', ''),
            ('parse-missing', '(Parse {0003one} 2)', ''),
            ('parse-header', '(Parse {000} 1)', ''),
            ('parse-body', '(Parse {0005abc} 1)', ''),
            ('parse-digit', '(Parse {x001x} 1)', ''),
            ('parse-tail', '(Parse {0003one000} 1)', ''),
            ('add-overflow', '(Add $max 1)', ''),
            ('multiply-overflow', '(Mult $max 2)', ''),
            ('zero-divisor', '(Div 9 0)', ''),
            ('min-divisor', '(Div $min -1)', ''),
            ('zero-remainder', '(RemainderOfDiv 9 0)', ''),
            ('min-remainder', '(RemainderOfDiv $min -1)', ''),
            ('number-overflow', '(Add $huge 1)', ''),
            ('hex-overflow', '(HexToNum {100000000})', ''),
            ('padding-overflow', '(NumToHex 15 $width)', ''),
            ('copy-overflow', '(StrCopy {Hello} $huge 3)', ''),
        ]
        init_commands += (' Set $prefix={23tail}\n'
            ' Set $max={2147483647}\n Set $min={-2147483648}\n'
            ' Set $huge={999999999999999999999999}\n'
            ' Set $width={65537}\n'
            ' Set $file={'+str(value_report)+'}\n')
        for label, expression, expected in value_cases:
            init_commands += (' Set $value='+expression+'\n'
                ' WriteToFile $file {'+label+':} $value\n')
        init_commands += (' If $prefix>2 Then\n'
            ' WriteToFile $file {comparison-prefix:yes}\n'
            ' If $huge=={999999999999999999999999} Then\n'
            ' WriteToFile $file {comparison-string:yes}\n'
            ' If $huge>2 Then\n WriteToFile $file {comparison-overflow:bad}\n'
            ' Else\n WriteToFile $file {comparison-overflow:safe}\n')
        loop_report = runtime.root / 'loop-report'
        init_commands += (' Set $file={'+str(loop_report)+'}\n'
            ' Set $first={2147483646}\n Set $last={2147483647}\n'
            ' For $counter=$first To $last Do\n Begin\n'
            ' WriteToFile $file {high:} $counter\n Set $counter={}\n End\n'
            ' Set $first={-2147483648}\n Set $last={-2147483647}\n'
            ' For $counter=$first To $last Do\n Begin\n'
            ' WriteToFile $file {low:} $counter\n Set $counter={}\n End\n'
            ' For $counter=(Add 1 1) To (Mult 2 2) Do\n Begin\n'
            ' WriteToFile $file {normal:} $counter\n End\n'
            ' For $counter=3 To 1 Do\n WriteToFile $file {descending:bad}\n'
            ' Set $counter={keep}\n'
            ' For $counter=$huge To 2 Do\n WriteToFile $file {invalid:bad}\n'
            ' WriteToFile $file {invalid-kept:} $counter\n')
        source = script.read_text()
        source += ('Widget 1000\nProperty\n Type ItemDraw\n'
                   ' Position 10 45\n Size 60 16\n Title {edge-seed}\nEnd\n')
        script.write_text(source.replace(' Set $received=0\n',
            init_commands+' Set $received=0\n', 1))
        config = runtime.root / 'config'
        paths = ':'.join(str(p.parent) for p in (args.mfl, args.script, args.form))
        config.write_text('ModulePath ' + paths + '\n'
            'Style * TileCascadePlacement\n'
            '*ProbeForm: Title "FormProbe"\n*ProbeForm: Line left\n'
            '*ProbeForm: Input entry 20 ""\n*ProbeForm: Line left\n'
            '*ProbeForm: Button continue "Submit" F1\n'
            "*ProbeForm: Command !printf '%s' '$(entry)' > " + shlex.quote(str(form_state)) + '\n'
            '*ProbeForm: Command Echo form-submitted\n'
            'AddToFunc StartFunction\n+ I Module FvwmMFL\n'
            '+ I Module FvwmScript ' + str(script) + '\n'
            '+ I Module FvwmForm ProbeForm\n')
        core = runtime.start('widget-core', [str(args.fvwm), '-d', display,
            '-f', str(config)], 'widget-core.log', runtime.environment({
                'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate('widget endpoint', endpoint.exists,
            time.monotonic() + 10, process=core)
        connection = Connection(endpoint)
        original_record = connection.record
        def traced_record():
            record = original_record()
            trace.append(record)
            return record
        connection.record = traced_record
        connection.record()
        connection.send(b'set echo\n')
        connection.echo('widget-ready')

        def children(window):
            root, parent, items, count = C.c_ulong(), C.c_ulong(), C.POINTER(C.c_ulong)(), C.c_uint()
            if not x.XQueryTree(d, window, C.byref(root), C.byref(parent), C.byref(items), C.byref(count)):
                return []
            result = [items[i] for i in range(min(count.value, 64))]
            if items:
                x.XFree(items)
            return result

        def named(title):
            queue = [(client.root, 0)]
            while queue:
                window, depth = queue.pop()
                value = C.c_void_p()
                if x.XFetchName(d, window, C.byref(value)) and value:
                    name = C.string_at(value).decode('utf-8', 'replace')
                    x.XFree(value)
                    if name == title:
                        return window
                if depth < 5:
                    queue.extend((w, depth + 1) for w in children(window))
            return None

        wait_for_predicate('script and form windows',
            lambda: named('SelectionProbe') and named('FormProbe') and named('MessageProbe'),
            time.monotonic() + 10, process=core)
        script_window, form_window = named('SelectionProbe'), named('FormProbe')
        script_field = min(children(script_window),
                           key=lambda w: client.geometry(w)[1])
        form_field = min(children(form_window), key=lambda w: client.geometry(w)[1])

        def click(window):
            event = Event()
            parent = script_window if window == script_field else window
            event.button = Button(type=4, display=d, window=parent,
                subwindow=window if parent != window else 0,
                root=client.root, x=5, y=5, button=2, same_screen=1)
            assert x.XSendEvent(d, parent, 0, 1 << 2, C.byref(event))
            x.XSync(d, 0)

        def script_value(value):
            connection.expect_echo('script-state:' + value)

        # The hidden selection-owner window belongs to our test client.
        client.create()
        x.XSetSelectionOwner(d, primary, client.window, 0)
        x.XSync(d, 0)
        connection.expect_echo('single-launch-id:3')
        connection.expect_echo('script-message:hello-message')
        script_value('seed')
        if not any('child-argument:arg19' in str(record) for record in trace):
            connection.expect_echo('child-argument:arg19')
        wait_for_predicate('long-path child window',
            lambda: named('SingleLaunchProbe'),
            time.monotonic() + 10, process=core)
        evidence = {'script-message-exchange': 'passed',
                    'script-single-evaluation-long-launch': 'passed'}
        output = script_output.read_text()
        assert output.startswith('prefix\n#'+str(script)+',')
        assert output.endswith('\n'+('0123456789abcdef'*256)+'\n\n#end\nsuffix\n')
        assert malformed_output.read_text() == malformed
        assert not (runtime.root/'missing'/'file').exists()
        evidence['script-file-and-index-bounds'] = 'passed'
        assert extracted_output.read_text().endswith(
            '\n'+('w'*32000)+'\n'+('w'*32000)+'\n#end\n\n')
        evidence['script-exact-output-long-word-and-command'] = 'passed'
        descriptor_text = descriptor_report.read_text()
        (args.artifacts_dir / 'script-subprocess-descriptors.txt').write_text(
            descriptor_text)
        assert descriptor_text.endswith('clean\n#end\n\n'), descriptor_text
        evidence['script-subprocess-protocol-pipes-closed'] = 'passed'
        widget_text = widget_report.read_text()
        (args.artifacts_dir / 'script-widget-indices.txt').write_text(widget_text)
        for invalid in invalid_widgets:
            assert 'case:'+invalid+':\n' in widget_text, widget_text
        assert widget_text.endswith('edge:edge:23\n#end\n\n'), widget_text
        evidence['script-widget-indices-and-edge-id'] = 'passed'
        process_text = process_report.read_text()
        assert process_text.endswith('timeout:\nrecovery:recovery\n#end\n\n')
        (args.artifacts_dir / 'script-process-default.txt').write_text(process_text)
        evidence['script-default-process-timeout-and-recovery'] = 'passed'
        value_text = value_report.read_text()
        expected_values = ''.join(label+':'+expected+'\n'
            for label, expression, expected in value_cases)
        expected_values += ('comparison-prefix:yes\ncomparison-string:yes\n'
                            'comparison-overflow:safe\n')
        assert value_text.endswith(expected_values+'#end\n\n'), value_text
        (args.artifacts_dir / 'script-values.txt').write_text(value_text)
        evidence['script-value-bounds-and-compatibility'] = 'passed'
        loop_text = loop_report.read_text()
        expected_loops = ('high:2147483646\nhigh:2147483647\n'
            'low:-2147483648\nlow:-2147483647\n'
            'normal:2\nnormal:3\nnormal:4\ninvalid-kept:keep\n')
        assert loop_text.endswith(expected_loops+'#end\n\n'), loop_text
        (args.artifacts_dir / 'script-loops.txt').write_text(loop_text)
        evidence['script-loop-range-and-owned-value'] = 'passed'
        override_report = runtime.root / 'process-override-report'
        timeout_script = runtime.root / 'TimeoutProbe'
        timeout_script.write_text('WindowTitle {TimeoutProbe}\nWindowSize 80 40\n'
            'Init\nBegin\n'
            ' Set $output=(GetOutput {sleep 1.5; printf slow} 1 -1)\n'
            ' Set $file={'+str(override_report)+'}\n'
            ' WriteToFile $file {timeout:} $output\n'
            ' Set $output=(GetOutput {printf good} 1 -1)\n'
            ' WriteToFile $file {recovery:} $output\nEnd\n'
            'Widget 1\nProperty\n Type ItemDraw\n Position 5 5\n'
            ' Size 60 20\n Title {timeout}\nEnd\n')
        connection.send(('*FvwmScript: CommandTimeout 500\nModule '+
            args.script.name+' '+str(timeout_script)+'\n').encode())
        wait_for_predicate('configured process timeout and recovery',
            lambda: override_report.exists() and 'recovery:good' in
                override_report.read_text(), time.monotonic()+15)
        override_text = override_report.read_text()
        assert override_text.endswith('timeout:\nrecovery:good\n#end\n\n')
        (args.artifacts_dir / 'script-process-override.txt').write_text(override_text)
        evidence['script-configured-process-timeout-and-recovery'] = 'passed'
        # Exercise the production sender with an empty queue, including a
        # forged None target that previously selected queue element -1.
        owner = named('MessageProbe')
        selection = next((value for index in range(16, 256)
            if x.XGetSelectionOwner(d, value := x.XInternAtom(d,
                b'\xa1' + format(index, 'x').encode() + b'FvwmScript10', 1)) == owner), None)
        assert selection, 'child selection owner not found'
        for target, prop in ((0, 0), (atom('absent-recipient'), 0),
                             (atom('absent-recipient'), atom('probe-property'))):
            request = Event()
            request.request = SelectionRequest(type=30, display=d,
                owner=owner, requestor=client.window, selection=selection,
                target=target, property=prop, time=0)
            assert x.XSendEvent(d, owner, 0, 0, C.byref(request))
            x.XSync(d, 0)
            reply = Event()
            wait_for_predicate('empty script queue refusal',
                lambda: x.XCheckTypedEvent(d, 31, C.byref(reply)),
                time.monotonic() + 3, process=core)
            assert (reply.selection.requestor, reply.selection.selection,
                    reply.selection.target, reply.selection.property) == (
                        client.window, selection, target, 0)
        evidence['script-empty-and-invalid-target'] = 'passed'
        # All eight historical cut buffers reach the production script watcher.
        for index in range(8):
            target = atom('CUT_BUFFER' + str(index))
            for kind, fmt, data, length in (
                (string, 8, C.create_string_buffer(b'clipboard'), 9),
                (string, 32, (C.c_ulong * 2)(0x41414141, 0x42424242), 2),
                (atom('ATOM'), 8, C.create_string_buffer(b'wrong'), 5),
                (string, 8, C.create_string_buffer(b'x' * 65537), 65537)):
                x.XChangeProperty(d, client.root, target, kind, fmt, 0, data, length)
        x.XSync(d, 0)
        connection.echo('cut-buffer-updates-sent')
        script_value('seed')
        evidence['script-eight-cut-buffers'] = 'passed'
        for mode in ('valid', 'format32', 'type', 'oversize', 'silent', 'recovery'):
            click(script_field)
            event = Event()
            wait_for_predicate('selection request ' + mode,
                lambda: x.XCheckTypedEvent(d, 30, C.byref(event)),
                time.monotonic() + 3, process=core)
            req = event.request
            connection.echo("request-captured-" + mode)
            if mode != 'silent':
                data = C.create_string_buffer(b'hello')
                fmt, kind, length = 8, req.target, 5
                if mode == 'format32':
                    data, fmt, length = (C.c_ulong * 2)(0x41414141, 0x42424242), 32, 2
                elif mode == 'type':
                    kind = atom('ATOM')
                elif mode == 'oversize':
                    data, length = C.create_string_buffer(b'x' * 32769), 32769
                x.XChangeProperty(d, req.requestor, req.property, kind, fmt, 0, data, length)
                reply = Event()
                reply.selection = SelectionNotify(type=31, display=d, requestor=req.requestor,
                    selection=req.selection, target=req.target, property=req.property, time=req.time)
                x.XSendEvent(d, req.requestor, 0, 0, C.byref(reply))
                x.XSync(d, 0)
            script_value('hellohelloseed' if mode == 'recovery' else 'helloseed')
            connection.echo('script-' + mode)
            evidence['script-' + mode] = 'passed'
            print('script-' + mode + ' passed', flush=True)

        for mode in ('valid', 'format32', 'type', 'oversize', 'recovery', 'utf8-boundary'):
            data = C.create_string_buffer(b'hello')
            fmt, kind, length = 8, string, 5
            if mode == 'format32':
                data, fmt, length = (C.c_ulong * 2)(0x41414141, 0x42424242), 32, 2
            elif mode == 'type':
                kind = atom('ATOM')
            elif mode == 'oversize':
                data, length = C.create_string_buffer(b'x' * 65537), 65537
            if mode == 'utf8-boundary':
                text = 'a' * 4095 + 'é' + 'z'
                payload = text.encode('utf-8')
                data, length = C.create_string_buffer(payload), len(payload)
            # Core echo follows process launch, not shell-file completion.
            # A fresh path prevents a prior matching value satisfying the wait.
            form_state.unlink(missing_ok=True)
            x.XChangeProperty(d, client.root, cut, kind, fmt, 0, data, length)
            click(form_field)
            event = Event()
            event.key = Key(type=2, display=d, window=form_window,
                root=client.root, keycode=x.XKeysymToKeycode(d, 0xffbe), same_screen=1)
            x.XSendEvent(d, form_window, 0, 1, C.byref(event))
            x.XSync(d, 0)
            value = 'hellohello' if mode in ('recovery', 'utf8-boundary') else 'hello'
            if mode == 'utf8-boundary':
                value = text + value
            connection.expect_echo('form-submitted')
            wait_for_predicate('form output '+mode,
                lambda: form_state.exists() and form_state.read_text() == value,
                time.monotonic()+5)
            assert form_state.read_text() == value, 'form text changed unexpectedly'
            evidence['form-' + mode] = 'passed'
            print('form-' + mode + ' passed', flush=True)
        (args.artifacts_dir / 'widget-results.json').write_text(json.dumps(evidence, indent=2) + '\n')
        print('Actual FvwmScript/FvwmForm rejection, timeout and recovery passed')
    finally:
        if form_state.exists():
            (args.artifacts_dir / 'form-state.txt').write_text(form_state.read_text())
        (args.artifacts_dir / 'widget-results-partial.json').write_text(
            json.dumps(evidence, indent=2) + '\n')
        (args.artifacts_dir / 'widget-ipc.json').write_text(json.dumps(trace, indent=2) + '\n')
        if connection:
            connection.close()
        client.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', required=True)
    parser.add_argument('--display')
    for name in ('fvwm', 'mfl', 'script', 'form'):
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    args = parser.parse_args()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix='fvwm-selection-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24'], 'xserver.log',
                    time.monotonic() + 15)
            probe = runtime.start('selection', [args.probe], 'probe.log',
                                  runtime.environment({'DISPLAY':display}))
            if probe.wait_until_exit(time.monotonic() + 15) != 0:
                raise HarnessError('Selection probe failed')
            print(probe.log_path.read_text())
            if args.fvwm:
                widgets(runtime, display, args)
        finally:
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, args.artifacts_dir / log.name)


if __name__ == '__main__':
    main()
