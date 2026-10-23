// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"context"
	"crypto/rand"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"net"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"
	"unicode"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/transport"
)

const idleLimit = 5 * time.Minute
const workerLimit = 10 * time.Second

type options struct {
	socket, dir, opener, terminal string
	view                          string
	hidden, inspect               bool
	targets                       [][]byte
	properties                    bool
}
type pendingWork struct {
	serial     uint64
	request    workRequest
	navigation string
}
type completedWork struct {
	work   pendingWork
	result workResult
}

type browser struct {
	session                    *transport.Session
	executable, instance, home string
	alias                      string
	opener, terminal           string
	model                      collection.Model
	snapshot                   *filemodel.Snapshot
	hidden                     bool
	history                    []string
	form, serial               uint64
	active                     bool
	cancel                     context.CancelFunc
	pending                    *pendingWork
	acks                       chan string
	results                    chan completedWork
	status                     string
	order, inspected           string
	view                       string
	reverse                    bool
}

func (b *browser) viewing() string {
	if b.view == "grid" {
		return "grid"
	}
	return "list"
}

func (b *browser) ordering() string {
	if b.order == "" {
		return "name"
	}
	return b.order
}

func (b *browser) reorder(key string, reverse bool) error {
	if b.snapshot == nil {
		return errors.New("folder is not ready")
	}
	entries, err := b.snapshot.OrderedEntries(key, reverse)
	if err != nil {
		return err
	}
	if err := b.model.Replace(entries); err != nil {
		return err
	}
	b.order, b.reverse = key, reverse
	return nil
}

// Text is presentation only. Prevent FVWM/Form expansions without changing
// the original bytes retained by filemodel and used for all actions.
func displayText(value string, limit int) string {
	var out strings.Builder
	for _, c := range value {
		text := string(c)
		if !unicode.IsPrint(c) || c > 126 || strings.ContainsRune("$%\\\"&", c) {
			text = fmt.Sprintf("[U+%04X]", c)
		}
		if out.Len()+len(text) > limit {
			out.WriteString("...")
			break
		}
		out.WriteString(text)
	}
	return out.String()
}

func (b *browser) show() error {
	if b.form == ^uint64(0) {
		return errors.New("form generation exhausted")
	}
	oldAlias := b.alias
	b.form++
	state := "ready"
	if b.active {
		state = "busy"
	}
	alias := "FvwmFiles-" + b.instance + "-" + strconv.FormatUint(b.form, 10) + "-" + state
	b.alias = alias
	callback := "Command Echo FVWM_FILES_2 " + b.instance + " " + strconv.FormatUint(b.form, 10) + " "
	commands := []string{}
	if oldAlias != "" {
		commands = append(commands, "KillModule FvwmForm "+oldAlias, "DestroyModuleConfig "+oldAlias+":*")
	}
	line := func(value string) { commands = append(commands, "*"+alias+": "+value) }
	button := func(label, key, action, argument string) {
		line("Button continue \"" + label + "\" " + key)
		line(callback + action + " " + argument + " x$(picks:Base64)")
	}
	line(`Title "File browser"`)
	line(`Line left`)
	path := "Loading folder"
	if b.snapshot != nil {
		path = string(b.snapshot.Path)
	}
	line(`Text "` + displayText(path, 90) + `"`)
	line(`Line left`)
	line(`Text "` + displayText(b.status, 120) + `"`)
	line(`Line left`)
	line(`Text "Filter: ` + displayText(b.model.Filter(), 60) + `"`)
	line(`Input filter 24 ""`)
	button("Apply filter", "", "filter", "x$(filter:Base64)")
	button("Clear filter", "", "unfilter", "-")
	line(`Line left`)
	button("Sort: "+b.ordering(), "", "sort", "-")
	direction := "ascending"
	if b.reverse {
		direction = "descending"
	}
	button(direction, "", "reverse", "-")
	button("View: "+b.viewing()+" (F7)", "F7", "view", "-")
	line("Line left")
	if b.viewing() == "grid" {
		line("List picks 84 4")
		line("ListGrid 4")
	} else {
		line("List picks 84 12")
	}
	entries := b.model.VisibleEntries()
	for _, item := range entries {
		selected := "off"
		if b.model.Selected[item.ID] {
			selected = "on"
		}
		icon := map[string]string{"directory": "directory", "file": "file", "symlink": "link"}[item.Kind]
		if icon == "" {
			icon = "other"
		}
		line(fmt.Sprintf("ListItem %s \"%s\" %s", selected,
			displayText(item.Label, 90), icon))
	}
	for index, entry := range entries {
		if entry.ID == b.model.Focus {
			line(fmt.Sprintf("ListFocus %d", index+1))
			break
		}
	}
	line("Line left")
	line(fmt.Sprintf("Text \"%d/%d visible; %d selected (%d filtered out, limit %d)\"", b.model.VisibleCount(), len(b.model.Entries), len(b.model.Selected), b.model.HiddenSelectedCount(), collection.MaxSelected))
	line("Line left")
	button("Select visible", "", "select-visible", "-")
	button("Clear selection", "", "clear-selection", "-")
	button("Properties (F2)", "F2", "properties", "-")
	if b.snapshot != nil && b.inspected != "" {
		if details, err := b.snapshot.ObservedDetails(b.inspected); err == nil {
			line("Line left")
			line(`Text "Observed metadata; Refresh updates it. No link is followed."`)
			for _, text := range details {
				line("Line left")
				line(`Text "` + displayText(text, 100) + `"`)
			}
		}
	}
	line("Line left")
	button("Home", "", "home", "-")
	button("Up", "", "up", "-")
	button("Back", "", "back", "-")
	button("Refresh (F5)", "F5", "refresh", "-")
	button("Hidden (F6)", "F6", "hidden", "-")
	line("Line left")
	button("Open focused (Enter)", "^M", "focused", "-")
	button("Open selected (F3)", "F3", "selected", "-")
	button("Terminal (F4)", "F4", "terminal", "-")
	line("Line left")
	line(`Text "Path:"`)
	line(`Input path 40 ""`)
	button("Go", "", "path", "x$(path:Base64)")
	button("Cancel work", "", "cancel", "-")
	button("Close (Esc)", "^[", "close", "-")
	line(`Line left`)
	line(`Text "Arrows/PgUp/PgDn or wheel; Shift extends selection; Space toggles; Tab moves fields. Idle close: 5 minutes."`)
	line("Timeout 3000 \"Echo FVWM_FILES_2 " + b.instance + " " + strconv.FormatUint(b.form, 10) + " close - x\" \"\"")
	commands = append(commands, "Module FvwmForm "+alias)
	deadline := time.NewTimer(workerLimit)
	defer deadline.Stop()
	for i, command := range commands {
		if err := b.session.Send(command); err != nil {
			return err
		}
		// Core acknowledgement bounds outstanding configuration traffic instead
		// of flooding MFL's finite output queue. Only the final command maps UI.
		if b.acks != nil && (i%32 == 31 || i == len(commands)-1) {
			tag := fmt.Sprintf("FVWM_FILES_SYNC %s %d %d", b.instance, b.form, i)
			if err := b.session.Send("Echo " + tag); err != nil {
				return err
			}
			for {
				select {
				case value := <-b.acks:
					if value == tag {
						goto acknowledged
					}
				case <-b.session.Done():
					return net.ErrClosed
				case <-deadline.C:
					return errors.New("native collection configuration timed out")
				}
			}
		acknowledged:
		}
	}
	return nil
}

func (b *browser) start(work pendingWork) {
	b.active = true
	ctx, cancel := context.WithTimeout(context.Background(), workerLimit)
	b.cancel = cancel
	go func() {
		defer cancel()
		b.results <- completedWork{work, startWorker(ctx, b.executable, work.request)}
	}()
}

func (b *browser) queue(request workRequest, navigation string) error {
	if b.serial == ^uint64(0) {
		return errors.New("request generation exhausted")
	}
	if b.active && request.Operation != "scan" {
		return errors.New("file worker is busy")
	}
	b.serial++
	work := pendingWork{b.serial, request, navigation}
	b.status = "Loading or checking target; Cancel work remains available"
	if b.active {
		b.cancel()
		b.pending = &work
	} else {
		b.start(work)
	}
	return nil
}

func (b *browser) navigate(path, navigation string) error {
	if !filepath.IsAbs(path) && b.snapshot != nil {
		path = filepath.Join(string(b.snapshot.Path), path)
	}
	if !filepath.IsAbs(path) || len(path) > filemodel.MaxPath || strings.ContainsRune(path, 0) {
		return errors.New("invalid folder path")
	}
	return b.queue(workRequest{Operation: "scan", Path: []byte(filepath.Clean(path)), Hidden: b.hidden}, navigation)
}

func (b *browser) completed(event completedWork) error {
	b.active = false
	b.cancel = nil
	if event.work.serial == b.serial {
		result := event.result
		if result.Status == "loaded" && result.Snapshot != nil {
			snapshot := result.Snapshot
			if snapshot.Schema != 1 {
				return errors.New("invalid directory schema")
			}
			old := ""
			if b.snapshot != nil {
				old = string(b.snapshot.Path)
			}
			entries, err := snapshot.OrderedEntries(b.ordering(), b.reverse)
			if err != nil {
				return err
			}
			if old != string(snapshot.Path) {
				if event.work.navigation == "push" && old != "" {
					b.history = append(b.history, old)
					if len(b.history) > 32 {
						b.history = b.history[1:]
					}
				} else if event.work.navigation == "back" && len(b.history) != 0 {
					b.history = b.history[:len(b.history)-1]
				}
				b.model.Selected = nil
				b.inspected = ""
				b.model.Focus = ""
			}
			if err := b.model.Replace(entries); err != nil {
				return err
			}
			b.snapshot, b.hidden = snapshot, snapshot.Hidden
			if len(result.Selected) != 0 {
				if err := b.model.SelectIDs(b.model.Generation, result.Selected); err != nil {
					return err
				}
				if b.model.Filter() == "" {
					b.model.Focus = result.Selected[0]
				}
				if event.work.navigation == "properties" {
					if len(result.Selected) != 1 {
						return errors.New("properties requires one target")
					}
					b.inspected = result.Selected[0]
				}
			}
			b.status = "Folder loaded. Symlink/special-file actions unavailable."
		} else if result.Status == "started" {
			b.status = "Application started; the browser cannot confirm that the file opened"
		} else {
			b.status = result.Error
		}
	}
	if b.pending != nil {
		work := *b.pending
		b.pending = nil
		b.start(work)
	}
	return b.show()
}

func decodeField(value string) ([]byte, error) {
	if !strings.HasPrefix(value, "x") || len(value) > 6000 {
		return nil, errors.New("invalid native field")
	}
	return base64.RawURLEncoding.DecodeString(value[1:])
}

// Only controls from this exact rendered view can act. These tokens prevent
// stale callbacks, not hostile same-user/X11 clients from accessing the WM.
func (b *browser) action(message string) (bool, error) {
	parts := strings.Fields(message)
	if len(parts) != 6 || parts[0] != "FVWM_FILES_2" || parts[1] != b.instance {
		return false, errors.New("invalid browser callback")
	}
	if parts[2] != strconv.FormatUint(b.form, 10) {
		return false, errors.New("stale browser control")
	}
	if parts[3] == "close" {
		return true, nil
	}
	selected, err := decodeField(parts[5])
	if err != nil {
		return false, err
	}
	if err := b.model.SelectNative(b.model.Generation, selected); err != nil {
		return false, err
	}
	path := ""
	if b.snapshot != nil {
		path = string(b.snapshot.Path)
	}
	switch parts[3] {
	case "filter", "unfilter":
		var value []byte
		if parts[3] == "filter" {
			value, err = decodeField(parts[4])
		}
		if err == nil {
			err = b.model.SetFilter(string(value))
		}
	case "view":
		if b.viewing() == "list" {
			b.view = "grid"
		} else {
			b.view = "list"
		}
	case "sort", "reverse":
		key, reverse := b.ordering(), !b.reverse
		if parts[3] == "sort" {
			key = map[string]string{"name": "kind", "kind": "size", "size": "modified", "modified": "name"}[key]
			reverse = b.reverse
		}
		err = b.reorder(key, reverse)
	case "select-visible":
		entries := b.model.VisibleEntries()
		ids := make([]string, 0, len(entries))
		for id := range b.model.Selected {
			ids = append(ids, id)
		}
		known := make(map[string]bool, len(ids))
		for _, id := range ids {
			known[id] = true
		}
		for _, entry := range entries {
			if !known[entry.ID] {
				ids = append(ids, entry.ID)
			}
		}
		err = b.model.SelectIDs(b.model.Generation, ids)
	case "clear-selection":
		err = b.model.ClearSelection(b.model.Generation)
		if err == nil {
			b.inspected = ""
		}
	case "properties":
		ids := b.model.SelectedIDs()
		if len(ids) != 1 || b.snapshot == nil {
			err = errors.New("select exactly one item for observed properties")
		} else if _, err = b.snapshot.ObservedDetails(ids[0]); err == nil {
			b.inspected = ids[0]
		}
	case "home":
		err = b.navigate(b.home, "push")
	case "up":
		if path == "" {
			err = errors.New("folder is not ready")
		} else {
			err = b.navigate(filepath.Dir(path), "push")
		}
	case "back":
		if len(b.history) == 0 {
			err = errors.New("no previous folder")
		} else {
			err = b.navigate(b.history[len(b.history)-1], "back")
		}
	case "path":
		var value []byte
		value, err = decodeField(parts[4])
		if err == nil && len(value) == 0 {
			err = errors.New("enter a folder path")
		}
		if err == nil {
			err = b.navigate(string(value), "push")
		}
	case "refresh", "hidden":
		if parts[3] == "hidden" {
			b.hidden = !b.hidden
		}
		err = b.navigate(path, "refresh")
	case "selected", "activate", "terminal", "focused":
		operation, id, program := "activate", parts[4], b.opener
		if parts[3] == "focused" {
			id = b.model.Focus
		}
		if parts[3] == "selected" {
			ids := b.model.SelectedIDs()
			if len(ids) != 1 {
				return false, errors.New("select exactly one item to open")
			}
			id = ids[0]
		}
		if parts[3] == "terminal" {
			operation, id, program = "terminal", "", b.terminal
		}
		err = b.queue(workRequest{Operation: operation, Snapshot: b.snapshot, Hidden: b.hidden, ID: id, Program: program}, "push")
	case "cancel":
		if b.serial == ^uint64(0) {
			return false, errors.New("request generation exhausted")
		}
		b.serial++
		b.pending = nil
		if b.cancel != nil {
			b.cancel()
		}
		b.status = "Cancellation requested; waiting for current work to stop"
	default:
		err = errors.New("unsupported browser action")
	}
	return false, err
}

func run(o options) error {
	executable, err := os.Executable()
	if err != nil {
		return err
	}
	if o.inspect {
		ctx, cancel := context.WithTimeout(context.Background(), workerLimit)
		defer cancel()
		result := startWorker(ctx, executable, o.initialRequest())
		if err := json.NewEncoder(os.Stdout).Encode(result); err != nil {
			return err
		}
		if result.Status != "loaded" {
			return errors.New("directory inspection failed")
		}
		return nil
	}
	connection, err := net.DialTimeout("unix", transport.SocketPath(o.socket, os.Getenv, os.Getuid()), transport.WriteTimeout)
	if err != nil {
		return errors.New("cannot connect to FvwmMFL; start Module FvwmMFL")
	}
	session := transport.New(connection)
	defer session.Close()
	if err := session.Send("set echo"); err != nil {
		return err
	}
	nonce := make([]byte, 16)
	if _, err := rand.Read(nonce); err != nil {
		return err
	}
	home, err := os.UserHomeDir()
	if err != nil {
		return err
	}
	b := browser{session: session, executable: executable, instance: hex.EncodeToString(nonce), home: home,
		opener: o.opener, terminal: o.terminal, view: o.view, hidden: o.hidden, acks: make(chan string, 1), results: make(chan completedWork, 1)}
	defer func() {
		if b.cancel != nil {
			b.cancel()
		}
		session.Send("KillModule FvwmForm " + b.alias)
		session.Send("DestroyModuleConfig " + b.alias + ":*")
	}()
	events := make(chan string, 16)
	finished := make(chan error, 1)
	go func() {
		finished <- session.Read(func(record json.RawMessage) error {
			var event struct {
				Echo struct {
					Message string `json:"message"`
				} `json:"echo"`
			}
			if json.Unmarshal(record, &event) != nil {
				return errors.New("invalid MFL event")
			}
			if strings.HasPrefix(event.Echo.Message, "FVWM_FILES_SYNC "+b.instance+" ") {
				select {
				case b.acks <- event.Echo.Message:
				default:
					return errors.New("browser acknowledgement queue is full")
				}
			}
			if strings.HasPrefix(event.Echo.Message, "FVWM_FILES_2 "+b.instance+" ") {
				select {
				case events <- event.Echo.Message:
				default:
					return errors.New("browser control queue is full")
				}
			}
			return nil
		})
	}()
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	idle := time.NewTimer(idleLimit)
	defer idle.Stop()
	navigation := "push"
	if o.properties {
		navigation = "properties"
	}
	if err := b.queue(o.initialRequest(), navigation); err != nil {
		return err
	}
	if err := b.show(); err != nil {
		return err
	}
	for {
		select {
		case <-ctx.Done():
			return nil
		case <-idle.C:
			return nil
		case err := <-finished:
			return err
		case event := <-b.results:
			if err := b.completed(event); err != nil {
				return err
			}
		case message := <-events:
			close, err := b.action(message)
			if close {
				return nil
			}
			if err != nil {
				b.status = err.Error()
			}
			if !idle.Stop() {
				select {
				case <-idle.C:
				default:
				}
			}
			idle.Reset(idleLimit)
			if err := b.show(); err != nil {
				return err
			}
		}
	}
}

func main() {
	if len(os.Args) == 2 && os.Args[1] == "--worker" {
		if err := runWorker(os.Stdin, os.Stdout); err != nil {
			fmt.Fprintln(os.Stderr, "fvwm-file-browser: invalid worker request")
			os.Exit(1)
		}
		return
	}
	var o options
	home, _ := os.UserHomeDir()
	flag.StringVar(&o.dir, "dir", home, "initial absolute local directory or local file URI")
	var reveal, properties targetFlags
	flag.Var(&reveal, "reveal", "select a literal local target in its parent; repeat for one folder")
	flag.Var(&properties, "properties", "show one local target in its parent with observed properties")
	flag.StringVar(&o.view, "view", "list", "initial collection view: list or grid")
	flag.StringVar(&o.socket, "socket", "", "FvwmMFL socket (default: standard private discovery)")
	flag.StringVar(&o.opener, "opener", "", "literal opener executable; default: xdg-open when available")
	flag.StringVar(&o.terminal, "terminal", "", "literal terminal executable; default: x-terminal-emulator or xterm")
	flag.BoolVar(&o.hidden, "hidden", false, "include hidden entries")
	flag.BoolVar(&o.inspect, "inspect", false, "emit bounded schema-1 directory JSON without X")
	flag.Parse()
	if flag.NArg() != 0 {
		fmt.Fprintln(os.Stderr, "fvwm-file-browser: unexpected arguments")
		os.Exit(2)
	}
	dirSet := false
	flag.Visit(func(f *flag.Flag) { dirSet = dirSet || f.Name == "dir" })
	if err := prepareOptions(&o, dirSet, reveal, properties); err != nil {
		fmt.Fprintln(os.Stderr, "fvwm-file-browser:", err)
		os.Exit(2)
	}
	if o.opener == "" {
		o.opener, _ = exec.LookPath("xdg-open")
	}
	if o.terminal == "" {
		o.terminal, _ = exec.LookPath("x-terminal-emulator")
		if o.terminal == "" {
			o.terminal, _ = exec.LookPath("xterm")
		}
	}
	if err := run(o); err != nil {
		fmt.Fprintln(os.Stderr, "fvwm-file-browser:", err)
		os.Exit(1)
	}
}
