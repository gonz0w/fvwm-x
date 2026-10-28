// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"net"
	"os"
	"os/signal"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/internal/pinboard"
	"github.com/fvwmorg/FvwmPrompt/internal/runtimepath"
	"github.com/fvwmorg/FvwmPrompt/internal/viewbinding"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/transport"
)

type job struct {
	serial  uint64
	request workRequest
	ticket  viewbinding.Ticket
	epoch   string
	form    uint64
	page    pinboard.CellPage
}
type completedJob struct {
	job    *job
	result workResult
}
type iconAcquisition struct {
	view         uint32
	epoch, cache string
	offset       int
	pictures     map[string]string
}
type board struct {
	session                     *transport.Session
	protocol                    pinboard.Protocol
	options                     options
	executable, home, cacheRoot string
	environment                 map[string]string
	state                       pinboard.State
	model                       collection.Model
	observed                    map[string]filemodel.Observation
	ready                       bool
	view, nextView              uint32
	epoch, window, status       string
	serial, tagSerial           uint64
	active                      *job
	cancel                      context.CancelFunc
	binding                     *viewbinding.Binding
	results                     chan completedJob
	nativeReplies               nativeReplyGate
	events                      chan pinboard.Event
	controls                    chan string
	finished                    chan error
	renew                       *time.Ticker
	ctx                         context.Context
	cacheActive                 string
	icons                       *iconAcquisition
	obsolete                    []string
	cleanupBlocked              bool
	nativeLost                  bool
	form                        *formView
	formSerial, contextSerial   uint64
	contextAlias, contextID     string
}

func main() {
	if len(os.Args) == 2 && os.Args[1] == "--worker" {
		if err := runWorker(os.Stdin, os.Stdout); err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		return
	}
	home, err := os.UserHomeDir()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	o, err := parseOptions(os.Args[1:], home, os.Stderr)
	if errors.Is(err, flag.ErrHelp) {
		return
	}
	if err == nil {
		executable, e := runtimepath.Executable(os.Args[0])
		err = e
		if err == nil {
			ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
			defer stop()
			if o.create {
				workCtx, cancel := context.WithTimeout(ctx, workerLimit)
				err = runDataEdit(workCtx, executable, o, os.Stdout)
				cancel()
			} else {
				err = runBoard(ctx, executable, home, o)
			}
		}
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, "fvwm-pinboard:", err)
		os.Exit(1)
	}
}

func runBoard(ctx context.Context, executable, home string, o options) error {
	connection, err := net.DialTimeout("unix", transport.SocketPath(o.socket, os.Getenv, os.Getuid()), transport.WriteTimeout)
	if err != nil {
		return errors.New("cannot connect to FvwmMFL; start Module FvwmMFL")
	}
	session := transport.New(connection)
	defer session.Close()
	token, err := pinboard.NewID()
	if err != nil {
		return err
	}
	binding, err := viewbinding.New("intent")
	if err != nil {
		return err
	}
	if o.browser == "" {
		o.browser = filepath.Join(filepath.Dir(executable), "fvwm-file-browser")
	}
	b := board{session: session, protocol: pinboard.Protocol{Alias: "FvwmPinboard-" + token, Token: token}, options: o,
		executable: executable, home: home, cacheRoot: filepath.Join(os.TempDir(), "fvwm-pinboard-"+token),
		state: pinboard.Empty(), observed: map[string]filemodel.Observation{}, binding: binding,
		results: make(chan completedJob, 1), events: make(chan pinboard.Event, 16), controls: make(chan string, 16), finished: make(chan error, 1), ctx: ctx,
		status: "Loading saved references; management remains available", environment: map[string]string{}}
	for _, name := range []string{"XDG_CONFIG_HOME", "XDG_CONFIG_DIRS", "XDG_DATA_HOME", "XDG_DATA_DIRS"} {
		b.environment[name] = os.Getenv(name)
	}
	b.renew = time.NewTicker(5 * time.Second)
	defer b.renew.Stop()
	defer b.shutdown()
	go b.read()
	if err := session.Send("set echo"); err != nil {
		return err
	}
	if err := b.configure(initialConfig(b.protocol.Alias, o)); err != nil {
		return err
	}
	if err := session.Send("Module FvwmPinboard " + b.protocol.Alias); err != nil {
		return err
	}
	if err := b.send("Owner"); err != nil {
		return err
	}
	// No filesystem acquisition precedes this complete native scene.
	if _, err := b.publish(b.state, b.observed, nil, ""); err != nil {
		return err
	}
	if err := b.start(workRequest{Operation: "load", Path: o.state}, pinboard.CellPage{}); err != nil {
		return err
	}
	continuation := time.NewTicker(100 * time.Millisecond)
	defer continuation.Stop()
	for {
		select {
		case <-ctx.Done():
			return nil
		case err := <-b.finished:
			if err == nil {
				err = errors.New("FvwmMFL disconnected")
			}
			return err
		case <-session.Done():
			return errors.New("FvwmMFL disconnected; native owner lease will retire the canvas")
		case <-b.renew.C:
			if err := b.send("Renew"); err != nil {
				return err
			}
			status, err := b.barrier()
			if err != nil {
				return err
			}
			if status.State != "ready" || status.Current != b.view {
				return fmt.Errorf("native canvas is %s; saved references retained", status.State)
			}
			if status.Epoch != b.epoch {
				if err := b.layout(status.Epoch); err != nil {
					b.feedback(err.Error())
				}
			}
		case event := <-b.events:
			close, err := b.nativeEvent(event)
			if close {
				return nil
			}
			if err != nil {
				b.feedback(err.Error())
			}
		case message := <-b.controls:
			close, err := b.control(message)
			if close {
				return nil
			}
			if err != nil {
				b.feedback(err.Error())
			}
		case event := <-b.results:
			if err := b.complete(event); err != nil {
				b.feedback(err.Error())
			}
		case <-continuation.C:
			if err := b.continueWork(); err != nil {
				b.feedback(err.Error())
			}
		}
		if b.nativeLost {
			return errors.New("native canvas unavailable; saved references retained")
		}
		b.updateWorkLabel()
	}
}

func initialConfig(alias string, o options) []string {
	commands := []string{}
	if o.font != "" {
		commands = append(commands, "*"+alias+": Font "+xdgmenu.FVWMQuote(o.font))
	}
	if o.colorset >= 0 {
		commands = append(commands, fmt.Sprintf("*%s: Colorset %d", alias, o.colorset))
	}
	return commands
}

func (b *board) read() {
	b.finished <- b.session.Read(func(raw json.RawMessage) error {
		var record struct {
			Echo struct {
				Message string `json:"message"`
			} `json:"echo"`
		}
		if json.Unmarshal(raw, &record) != nil {
			return errors.New("invalid MFL event")
		}
		message := record.Echo.Message
		fields := strings.Fields(message)
		if len(fields) < 2 || fields[1] != b.protocol.Token {
			return nil
		}
		switch fields[0] {
		case "FVWM_PINBOARD_STATUS", "FVWM_PINBOARD_CELL", "FVWM_PINBOARD_CELLS_END", "FVWM_PINBOARD_SYNC":
			if len(fields) < 3 {
				return nil
			}
			return b.nativeReplies.offer(fields[2], message)
		case "FVWM_PINBOARD_EVENT":
			event, err := pinboard.ParseEvent(message, b.protocol.Token)
			if err != nil {
				return err
			}
			select {
			case b.events <- event:
			default:
				return errors.New("native event queue is full")
			}
		case "FVWM_PINBOARD_FORM", "FVWM_PINBOARD_CONTROL":
			select {
			case b.controls <- message:
			default:
				return errors.New("pinboard control queue is full")
			}
		}
		return nil
	})
}

func (b *board) start(request workRequest, page pinboard.CellPage) error {
	if b.active != nil {
		return errors.New("Pinboard worker is busy; Cancel work, then retry")
	}
	if b.serial == ^uint64(0) {
		return errors.New("worker generation exhausted")
	}
	b.serial++
	if err := b.binding.Observe("intent", viewbinding.Version{Producer: b.protocol.Token, Revision: strconv.FormatUint(b.serial, 10)}); err != nil {
		return err
	}
	ticket, ok := b.binding.Begin()
	if !ok {
		return errors.New("worker intent unavailable")
	}
	j := &job{serial: b.serial, request: request, ticket: ticket, epoch: b.epoch, page: page}
	if b.form != nil {
		j.form = b.form.serial
	}
	ctx, cancel := context.WithTimeout(b.ctx, workerLimit)
	b.cancel = cancel
	b.active = j
	go func() { defer cancel(); b.results <- completedJob{j, startWorker(ctx, b.executable, request)} }()
	return nil
}

func (b *board) obsoleteImage(path string) {
	if path == "" {
		return
	}
	for _, old := range b.obsolete {
		if old == path {
			return
		}
	}
	if len(b.obsolete) >= 2 {
		b.cleanupBlocked = true
		b.feedback("Image retirement limit reached; restart after cleaning private generations")
		return
	}
	b.obsolete = append(b.obsolete, path)
}
func (b *board) abandonIcons() {
	if b.icons != nil {
		b.obsoleteImage(b.icons.cache)
		b.icons = nil
	}
}

func (b *board) dataWork(request workRequest) error {
	if b.active != nil {
		return errors.New("Pinboard worker is busy; Cancel work, then retry")
	}
	if request.Operation != "load" && !b.ready {
		return errors.New("Reload saved references before editing or opening")
	}
	b.abandonIcons()
	if err := b.start(request, pinboard.CellPage{}); err != nil {
		return err
	}
	b.feedback("Checking or saving references; Cancel work remains available")
	return nil
}

func validateObserved(state pinboard.State, observed []observedPin) (map[string]filemodel.Observation, error) {
	if err := state.Validate(); err != nil {
		return nil, err
	}
	if len(observed) != len(state.Pins) {
		return nil, errors.New("incomplete target observations")
	}
	result := make(map[string]filemodel.Observation, len(observed))
	for i, o := range observed {
		if o.ID != state.Pins[i].ID || !bytes.Equal(o.Target.Path, state.Pins[i].Target) {
			return nil, errors.New("target observation identity mismatch")
		}
		switch o.Target.Status {
		case "missing", "unavailable":
		case "present":
			switch o.Target.Kind {
			case "file", "directory", "symlink", "special":
			default:
				return nil, errors.New("invalid observed target type")
			}
		default:
			return nil, errors.New("invalid target availability")
		}
		result[o.ID] = o.Target
	}
	return result, nil
}

func (b *board) complete(event completedJob) error {
	if b.active == nil || event.job != b.active {
		return nil
	}
	j := b.active
	b.active = nil
	b.cancel = nil
	result := event.result
	success := result.Status != "failed" && result.Status != "cancelled"
	current := b.binding.Complete(j.ticket, success)
	if j.request.Operation == "retire-images" {
		if !success {
			b.cleanupBlocked = true
			return errors.New("Image retirement failed; labels remain usable: " + result.Error)
		}
		if len(b.obsolete) > 0 && b.obsolete[0] == j.request.Cache {
			b.obsolete = b.obsolete[1:]
		}
		return nil
	}
	if !current || !success {
		if j.request.Operation == "save" {
			b.ready = false
			result.Error = "Save outcome unavailable; prior view retained. Reload before further edits"
		}
		if j.request.Operation == "icons" {
			b.abandonIcons()
		}
		if result.Error == "" {
			result.Error = "Stale worker result ignored; prior view retained"
		}
		return errors.New(result.Error)
	}
	if j.request.Operation == "icons" {
		return b.completeIcons(j, result)
	}
	if result.Status == "started" {
		b.feedback("Application process started; opened pixels and completion are not tracked")
		if b.cacheActive == "" {
			b.beginIcons()
		}
		return nil
	}
	state := b.state
	if j.request.Operation == "load" || j.request.Operation == "save" {
		if result.State == nil {
			return errors.New("worker returned no committed state")
		}
		state = *result.State
		if j.request.Operation == "save" && state.Revision != j.request.Expected+1 {
			return errors.New("worker returned wrong saved revision")
		}
		if j.request.Operation == "save" {
			expected := j.request.State.Clone()
			expected.Revision = state.Revision
			a, e1 := expected.Encode()
			actual, e2 := state.Encode()
			if e1 != nil || e2 != nil || !bytes.Equal(a, actual) {
				b.ready = false
				return errors.New("worker returned an unexpected saved model; Reload required")
			}
		}
	}
	observed, err := validateObserved(state, result.Observed)
	if err != nil {
		return err
	}
	if j.request.Operation == "load" && (b.options.layout.CellWidth != 0 || b.options.layout.CellHeight != 0 || b.options.layout.IconSize != 0) {
		next := state.Clone()
		if b.options.layout.CellWidth != 0 {
			next.Layout.CellWidth = b.options.layout.CellWidth
		}
		if b.options.layout.CellHeight != 0 {
			next.Layout.CellHeight = b.options.layout.CellHeight
		}
		if b.options.layout.IconSize != 0 {
			next.Layout.IconSize = b.options.layout.IconSize
		}
		b.options.layout = pinboard.Layout{}
		if next.Layout != state.Layout {
			return b.start(workRequest{Operation: "save", Path: b.options.state, Expected: state.Revision, State: next}, pinboard.CellPage{})
		}
	}
	if _, err := b.publish(state, observed, nil, ""); err != nil {
		if j.request.Operation == "save" {
			b.ready = false
		}
		return fmt.Errorf("Native publication failed; prior model retained; Reload: %w", err)
	}
	oldCache := b.cacheActive
	b.cacheActive = ""
	b.obsoleteImage(oldCache)
	b.state, b.observed, b.ready = state, observed, true
	if err := b.model.Replace(state.Entries()); err != nil {
		return err
	}
	b.beginIcons()
	b.feedback(fmt.Sprintf("Revision %d: %d references. Unpin removes only a reference", state.Revision, len(state.Pins)))
	if b.form != nil {
		if b.form.mode == "manager" || b.form.serial == j.form {
			return b.showManager(b.form.offset)
		}
	}
	return nil
}

func (b *board) beginIcons() {
	if b.cleanupBlocked || b.options.formats == "" || len(b.state.Pins) == 0 {
		return
	}
	b.icons = &iconAcquisition{view: b.view, epoch: b.epoch, cache: filepath.Join(b.cacheRoot, fmt.Sprintf("g-%d", b.view)), pictures: map[string]string{}}
}

func (b *board) continueWork() error {
	if b.active != nil {
		return nil
	}
	if len(b.obsolete) > 0 && !b.cleanupBlocked {
		return b.start(workRequest{Operation: "retire-images", Cache: b.obsolete[0], Owner: b.protocol.Token}, pinboard.CellPage{})
	}
	if b.icons == nil || !b.ready || b.cleanupBlocked {
		return nil
	}
	page, err := b.cells(b.icons.offset)
	if err != nil {
		b.abandonIcons()
		return err
	}
	return b.start(workRequest{Operation: "icons", State: b.state, Cells: page.Cells, Cache: b.icons.cache, Owner: b.protocol.Token,
		Home: b.home, Environment: b.environment, Theme: b.options.theme, Extensions: strings.Split(b.options.formats, ",")}, page)
}

func (b *board) completeIcons(j *job, result workResult) error {
	icons := b.icons
	if icons == nil || icons.view != b.view || icons.epoch != b.epoch || j.page.View != b.view || j.page.Epoch != b.epoch {
		b.abandonIcons()
		return errors.New("Stale icon acquisition ignored")
	}
	allowed := map[string]bool{}
	for _, cell := range j.page.Cells {
		allowed[cell.ID] = true
	}
	if len(result.Pictures) > len(allowed) {
		b.abandonIcons()
		return errors.New("invalid icon result count")
	}
	for id, picture := range result.Pictures {
		if !allowed[id] || !validPicture(picture, icons.cache) {
			b.abandonIcons()
			return errors.New("invalid private picture result")
		}
		if picture != "" {
			icons.pictures[id] = picture
		}
	}
	icons.offset += j.page.Count
	if icons.offset < len(b.state.Pins) {
		return nil
	}
	committed, err := b.publish(b.state, b.observed, icons.pictures, icons.epoch)
	if committed {
		b.cacheActive = icons.cache
	}
	b.icons = nil
	if err != nil {
		if committed {
			if _, fallbackErr := b.publish(b.state, b.observed, nil, ""); fallbackErr != nil {
				b.ready = false
				return fallbackErr
			}
			b.cacheActive = ""
		}
		b.obsoleteImage(icons.cache)
		if b.epoch != icons.epoch {
			b.beginIcons()
		}
		return err
	}
	if len(icons.pictures) == 0 {
		b.obsoleteImage(icons.cache)
		b.cacheActive = ""
	}
	if len(result.Warnings) > 0 {
		b.feedback("Some named icons are unavailable; labels remain usable")
	}
	return nil
}

func (b *board) cancelWork() {
	b.abandonIcons()
	if b.cancel != nil {
		_ = b.binding.Invalidate()
		b.cancel()
		b.feedback("Cancellation requested; waiting for the owned worker to be reaped")
	}
}

func (b *board) layout(epoch string) error {
	if !pinboard.Hex(epoch, 16) || epoch == "0000000000000000" {
		return errors.New("native monitor layout is unavailable")
	}
	if epoch == b.epoch {
		return nil
	}
	b.epoch = epoch
	if b.active != nil && (b.active.request.Operation == "icons" || b.active.request.Operation == "save") {
		b.cancelWork()
	}
	b.abandonIcons()
	if b.cacheActive != "" {
		if _, err := b.publish(b.state, b.observed, nil, ""); err != nil {
			return err
		}
		b.obsoleteImage(b.cacheActive)
		b.cacheActive = ""
	}
	if b.ready {
		b.beginIcons()
	}
	b.feedback("Layout changed; preferred placements retained and named icons refreshed")
	return nil
}

func (b *board) nativeEvent(event pinboard.Event) (bool, error) {
	if event.View != b.view {
		return false, nil
	}
	if event.Verb == "layout" {
		return false, b.layout(event.Epoch)
	}
	if event.Epoch != b.epoch {
		return false, nil
	}
	switch event.Verb {
	case "close":
		return true, nil
	case "manage":
		return false, b.showManager(0)
	case "refresh":
		return false, b.dataWork(workRequest{Operation: "observe", State: b.state})
	case "focus":
		if b.state.Index(event.ID) >= 0 {
			b.model.Focus = event.ID
		}
		return false, nil
	case "open":
		return false, b.open(event.ID)
	case "context":
		return false, b.showContext(event.ID)
	case "move":
		i := b.state.Index(event.ID)
		if i < 0 {
			return false, errors.New("stale pin move")
		}
		p := b.state.Pins[i]
		p.Monitor, p.Column, p.Row = event.Monitor, event.Column, event.Row
		return false, b.change("move", p)
	}
	return false, nil
}

func (b *board) change(verb string, p pinboard.Pin) error {
	next, err := b.state.Change(verb, p)
	if err != nil {
		return err
	}
	return b.dataWork(workRequest{Operation: "save", Path: b.options.state, Expected: b.state.Revision, State: next})
}
func (b *board) open(id string) error {
	if b.state.Index(id) < 0 {
		return errors.New("stale pin")
	}
	observation, ok := b.observed[id]
	if !ok {
		return errors.New("target observation unavailable; Refresh")
	}
	return b.dataWork(workRequest{Operation: "open", Observation: &observation, Program: b.options.opener, Browser: b.options.browser, Socket: b.options.socket})
}

func (b *board) shutdown() {
	b.nativeReplies.clear()
	b.binding.Close()
	if b.cancel != nil {
		b.cancel()
		<-b.results
		b.active = nil
		b.cancel = nil
	}
	_ = b.closeForm()
	if b.contextAlias != "" {
		_ = b.session.Send("DestroyMenu " + b.contextAlias)
	}
	acknowledged := false
	b.ctx = context.Background()
	if b.session != nil {
		if !b.nativeLost && b.send("Retire") == nil {
			if status, err := b.barrier(); err == nil && status.State == "retired" {
				acknowledged = true
			}
		}
		_ = b.session.Send("KillModule FvwmPinboard " + b.protocol.Alias)
		_ = b.session.Send("DestroyModuleConfig " + b.protocol.Alias + ":*")
	}
	if acknowledged {
		b.obsoleteImage(b.cacheActive)
		b.abandonIcons()
		for _, cache := range b.obsolete {
			ctx, cancel := context.WithTimeout(context.Background(), workerLimit)
			result := startWorker(ctx, b.executable, workRequest{Operation: "retire-images", Cache: cache, Owner: b.protocol.Token})
			cancel()
			if result.Status != "retired-images" {
				fmt.Fprintln(os.Stderr, "fvwm-pinboard: private image retirement failed:", cache, result.Error)
			}
		}
	} else if b.cacheActive != "" || b.icons != nil || len(b.obsolete) > 0 {
		fmt.Fprintln(os.Stderr, "fvwm-pinboard: native retirement unacknowledged; private image generations retained:", b.cacheRoot)
	}
}

func validPicture(picture, cache string) bool {
	if picture == "" {
		return true
	}
	if len(picture) > 384 || filepath.Dir(picture) != cache || strings.ContainsAny(picture, "\x00\r\n%*") {
		return false
	}
	name := filepath.Base(picture)
	prefix := ".fvwm-xdg-"
	if !strings.HasPrefix(name, prefix) || len(name) < len(prefix)+64+4 || !pinboard.Hex(name[len(prefix):len(prefix)+64], 64) {
		return false
	}
	suffix := name[len(prefix)+64:]
	if suffix == ".png" || suffix == ".xpm" {
		return true
	}
	if strings.HasPrefix(suffix, ".svg:") {
		width, height, ok := strings.Cut(strings.TrimPrefix(suffix, ".svg:"), "x")
		if !ok {
			return false
		}
		w, e1 := pinboard.Decimal(width, 4096)
		h, e2 := pinboard.Decimal(height, 4096)
		return e1 == nil && e2 == nil && w > 0 && h > 0
	}
	return false
}
