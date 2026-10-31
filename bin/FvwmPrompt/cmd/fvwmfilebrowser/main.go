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
	"github.com/fvwmorg/FvwmPrompt/internal/runtimepath"
	"github.com/fvwmorg/FvwmPrompt/internal/viewbinding"
	"github.com/fvwmorg/FvwmPrompt/transport"
)

const idleLimit = 5 * time.Minute
const workerLimit = 10 * time.Second

type options struct {
	socket, dir, opener, terminal string
	view                          string
	watch                         string
	hidden, inspect               bool
	reuse                         bool
	fileManagerService            bool
	fileManagerReceipt            *os.File
	targets                       [][]byte
	properties                    bool
	actions                       actionFlags
}
type pendingWork struct {
	serial     uint64
	request    workRequest
	navigation string
	ticket     viewbinding.Ticket
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
	worker                     *fileWorkerProcess
	pending                    *pendingWork
	acks                       chan string
	results                    chan completedWork
	status                     string
	order                      string
	inspected                  []string
	actions                    actionFlags
	view                       string
	reverse                    bool
	native                     chan string
	nativeChanges              chan nativeChange
	nativeWatchReplies         *nativeWatchReplyGate
	feedbackSerial             uint64
	feedbackError              string
	feedbackLabels             map[string]string
	nativeGeneration           uint32
	viewEntries                []collection.Entry
	renderPath, renderView     string
	renderSerial, lastControl  uint64
	replaceSelection           bool
	revealFocus                bool
	renderDeadline             time.Time
	watch                      *directoryWatch
	watchTimerActive           bool
	providerWork               *providerWork
	intentBinding              *viewbinding.Binding
	projectionBinding          *viewbinding.Binding
	activeSerial               uint64

	details                        *detailView
	detailSerial, detailSendSerial uint64
	detailEvents                   chan detailCallback
	detailAckReplies               *nativeWatchReplyGate

	fileRoles                  *fileRoleView
	roles                      *directoryRoleView
	roleSerial, roleSendSerial uint64
	roleEvents                 chan directoryRoleCallback
	roleAckReplies             *nativeWatchReplyGate

	job                           *jobState
	jobs                          *jobView
	jobSerial, jobViewSerial      uint64
	jobSendSerial                 uint64
	jobEvents                     chan jobNotice
	jobProgress                   chan jobNotice
	jobCallbacks                  chan jobCallback
	jobAckReplies, jobListReplies *nativeWatchReplyGate
}

func (b *browser) viewing() string {
	// Provider state describes the observed projection. A requested mode is
	// private until Form acknowledges its native layout generation.
	if b.renderView != "" {
		return b.renderView
	}
	return b.requestedView()
}

func (b *browser) requestedView() string {
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

func (b *browser) start(work pendingWork) error {
	ticket, ok := b.intentBinding.Begin()
	if !ok {
		return errors.New("browser intent is unavailable")
	}
	work.ticket = ticket
	b.activeSerial = work.serial
	b.active = true
	ctx, cancel := context.WithTimeout(context.Background(), workerLimit)
	b.cancel = cancel
	process := &fileWorkerProcess{done: make(chan struct{}), stop: make(chan struct{})}
	b.worker = process
	go func() {
		defer close(process.done)
		defer cancel()
		result := startWorker(ctx, b.executable, work.request)
		select {
		case b.results <- completedWork{work, result}:
		case <-process.stop:
		}
	}()
	return nil
}

func (b *browser) queue(request workRequest, navigation string) error {
	if b.serial == ^uint64(0) {
		return errors.New("request generation exhausted")
	}
	if b.active && request.Operation != "scan" {
		return errors.New("file worker is busy")
	}
	if request.Operation != "scan" && request.Operation != "locate" && request.Operation != "directory-roles" && request.Operation != "file-roles" && !b.dataReady() {
		return errors.New("file actions unavailable; refresh the folder first")
	}
	if request.Operation != "file-roles" && request.Operation != "file-role-launch" {
		if err := b.retireFileRoles(); err != nil {
			return err
		}
	}
	if request.Operation != "directory-roles" && request.Operation != "directory-role-launch" && b.roles != nil {
		if err := b.retireDirectoryRoles(); err != nil {
			return err
		}
	}
	if err := b.observeIntent(b.serial + 1); err != nil {
		return err
	}
	b.serial++
	work := pendingWork{serial: b.serial, request: request, navigation: navigation}
	b.status = "Loading or checking target; Cancel work remains available"
	if b.active {
		if b.providerWork != nil && b.providerWork.Status == "accepted" {
			b.providerWork.Status = "superseding"
		}
		b.cancel()
		b.pending = &work
	} else {
		return b.start(work)
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
	current := event.work.serial == b.serial
	if b.intentBinding != nil {
		if !b.active || event.work.serial != b.activeSerial {
			return nil
		}
		success := event.result.Status == "loaded" && event.result.Snapshot != nil || event.result.Status == "started"
		if event.work.request.Operation == "file-roles" || event.work.request.Operation == "file-role-launch" {
			success = b.fileRolePreservesCollection(event)
		}
		if event.work.request.Operation == "directory-roles" || event.work.request.Operation == "directory-role-launch" {
			success = b.directoryRolePreservesCollection(event)
		}
		current = b.intentBinding.Complete(event.work.ticket, success) && current
		b.activeSerial = 0
	}
	if b.worker != nil {
		<-b.worker.done
		b.worker = nil
	}
	b.active = false
	b.cancel = nil
	if b.intentBinding != nil && b.intentBinding.State() == viewbinding.Closed {
		return nil
	}
	// A current failed result may update diagnostics, but retains the last data.
	if current || event.work.serial == b.serial && b.intentBinding != nil && b.intentBinding.State() == viewbinding.Failed {
		result := event.result
		if event.work.request.Operation == "file-roles" || event.work.request.Operation == "file-role-launch" {
			b.status = result.Error
			if result.Status == "roles" && result.RoleFileValid && validFileRoleResult(result.FileRoles) {
				b.status = "File application choices loaded"
			}
			if result.Status == "started" {
				b.status = "File application started; completion is not tracked"
			}
			if err := b.finishFileRole(event); err != nil {
				return err
			}
		} else if event.work.request.Operation == "directory-roles" {
			if result.Status == "roles" && result.RoleFolderValid && validDirectoryRoleResult(result.Roles) {
				b.status = "Folder application choices loaded"
			} else {
				b.status = result.Error
			}
			if err := b.acceptDirectoryRoles(event); err != nil {
				return err
			}
		} else if result.Status == "loaded" && result.Snapshot != nil && event.work.request.Operation == "locate" {
			if err := b.publishLocated(result, event.work.navigation); err != nil {
				return err
			}
		} else if result.Status == "loaded" && result.Snapshot != nil {
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
				b.inspected = nil
				b.model.Focus = ""
			}
			status := "Folder loaded. Symlink/special-file actions unavailable."
			if old == string(snapshot.Path) {
				changes, err := b.model.Reconcile(b.model.Generation, entries)
				if err != nil {
					return err
				}
				status = fmt.Sprintf("Refreshed: %d added, %d updated, %d removed; %d selected entries lost.",
					changes.Added, changes.Updated, changes.Removed, changes.SelectionLost)
				if changes.Added+changes.Updated+changes.Removed == 0 {
					status = "Refreshed: no entry metadata changes."
				}
			} else if err := b.model.Replace(entries); err != nil {
				return err
			}
			b.snapshot, b.hidden = snapshot, snapshot.Hidden
			if len(b.inspected) != 0 {
				if _, err := snapshot.ObservedSelection(b.inspected); err != nil {
					b.inspected = nil
				}
			}
			if len(result.Selected) != 0 {
				if err := b.model.SelectIDs(b.model.Generation, result.Selected); err != nil {
					return err
				}
				if b.model.Filter() == "" {
					b.model.Focus = result.Selected[0]
				}
				if event.work.navigation == "properties" {
					b.inspected = append([]string(nil), result.Selected...)
				}
			}
			b.status = status
			if b.watch != nil {
				b.watch.desire(snapshot, event.work.navigation != "watch")
			}
		} else if result.Status == "started" {
			b.status = "Application started; the browser cannot confirm that the file opened"
			if event.work.request.Operation == "directory-role-launch" {
				b.status = "Folder application started; completion is not tracked"
			} else if event.work.request.Operation == "custom" {
				b.status = fmt.Sprintf("Application started with %d selected paths; completion is not tracked.", result.ActionCount)
			}
		} else {
			b.status = result.Error
			if b.watch != nil && event.work.navigation == "watch" {
				b.watch.fail("Automatic refresh failed")
			}
		}
	}
	if event.work.request.Operation == "directory-role-launch" && current {
		if err := b.finishDirectoryRole(event); err != nil {
			return err
		}
	}
	b.finishProviderWork(event)
	if b.pending != nil {
		work := *b.pending
		b.pending = nil
		if err := b.start(work); err != nil {
			return err
		}
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
	if len(parts) != 7 || parts[0] != "FVWM_FILES_3" || parts[1] != b.instance {
		return false, errors.New("invalid browser callback")
	}
	if parts[2] != strconv.FormatUint(b.form, 10) {
		return false, errors.New("stale browser control")
	}
	if parts[3] == "close" && parts[6] == "timeout" {
		return true, nil
	}
	ticket, err := strconv.ParseUint(parts[6], 16, 64)
	if err != nil || ticket == 0 || len(parts[6]) != 16 || fmt.Sprintf("%016x", ticket) != parts[6] || ticket <= b.lastControl {
		return false, errors.New("stale browser invocation")
	}
	if parts[3] == "close" {
		return true, nil
	}
	selected, err := nativePacket(parts[5], b.nativeGeneration)
	if err != nil {
		return false, err
	}
	if err := b.model.SelectNative(b.model.Generation, selected); err != nil {
		b.replaceSelection = true
		return false, err
	}
	if err := b.retireStaleFileRoles(); err != nil {
		return false, err
	}
	b.feedbackError = ""
	b.lastControl = ticket
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
		b.replaceSelection = true
		entries := b.model.VisibleEntries()
		err = b.model.EditSelection(b.model.Generation, "add", entryIDs(entries))
	case "clear-selection":
		b.replaceSelection = true
		err = b.model.ClearSelection(b.model.Generation)
		if err == nil {
			b.inspected = nil
		}
	case "jobs":
		err = b.openJobs()
	case "file-roles":
		err = b.beginFileRoles(true)
	case "directory-roles":
		err = b.beginDirectoryRoles(true)
	case "properties":
		ids := b.model.SelectedIDs()
		if b.snapshot == nil {
			err = errors.New("folder is not ready")
		} else if _, err = b.snapshot.ObservedSelection(ids); err == nil {
			b.inspected = append([]string(nil), ids...)
		}
	case "custom":
		index, parseErr := strconv.Atoi(parts[4])
		if parseErr != nil || index < 0 || index >= len(b.actions) {
			err = errors.New("unknown configured action")
		} else if ids := b.model.SelectedIDs(); len(ids) == 0 {
			err = errors.New("select at least one entry for the configured action")
		} else {
			err = b.queue(workRequest{Operation: "custom", Snapshot: b.snapshot,
				IDs: ids, Program: b.actions[index].Program}, "action")
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
	case "watch":
		if b.watch == nil {
			b.watch = &directoryWatch{events: make(chan watchEvent, 1)}
		}
		b.watch.enabled = !b.watch.enabled
		b.watch.desire(b.snapshot, true)
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
		if b.active || b.pending != nil {
			if err := b.invalidateIntent(); err != nil {
				return false, err
			}
		}
		b.serial++
		b.pending = nil
		if b.cancel != nil {
			b.cancel()
		}
		if b.providerWork != nil && b.providerWork.Status == "accepted" {
			b.providerWork.Status = "cancelling"
		}
		b.status = "Cancellation requested; waiting for current work to stop"
		if b.watch != nil {
			b.watch.enabled = false
			b.watch.desire(nil, false)
		}
	default:
		err = errors.New("unsupported browser action")
	}
	return false, err
}

func run(o options) error {
	executable, err := runtimepath.Executable(os.Args[0])
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
		opener: o.opener, terminal: o.terminal, actions: o.actions, view: o.view, hidden: o.hidden, acks: make(chan string, 1), native: make(chan string, 1), nativeChanges: make(chan nativeChange, 1), nativeWatchReplies: &nativeWatchReplyGate{}, results: make(chan completedWork, 1), detailEvents: make(chan detailCallback, 16), detailAckReplies: &nativeWatchReplyGate{}, jobEvents: make(chan jobNotice, 16), jobProgress: make(chan jobNotice, 1), jobCallbacks: make(chan jobCallback, 16), jobAckReplies: &nativeWatchReplyGate{}, jobListReplies: &nativeWatchReplyGate{}, roleEvents: make(chan directoryRoleCallback, 16), roleAckReplies: &nativeWatchReplyGate{}}
	b.watch = &directoryWatch{enabled: o.watch == "auto", events: make(chan watchEvent, 1)}
	defer func() {
		b.shutdownWorker()
		b.shutdownJob()
		b.retireJobs()
		b.closeBindings()
		b.retireDetails()
		b.retireDirectoryRoles()
		b.retireFileRoles()
		if b.watch.cancel != nil {
			b.watch.cancel()
		}
		if b.alias != "" {
			session.Send("KillModule FvwmForm " + b.alias)
			session.Send("DestroyModuleConfig " + b.alias + ":*")
		}
	}()
	events := make(chan string, 16)
	queries := make(chan fileQuery, 1)
	registered := make(chan providerRegistration, 1)
	finished := make(chan error, 1)
	go func() {
		finished <- session.Read(func(record json.RawMessage) error {
			var event struct {
				FileQuery *fileQuery            `json:"file_query"`
				Reply     *providerRegistration `json:"reply"`
				Echo      struct {
					Message string `json:"message"`
				} `json:"echo"`
			}
			if json.Unmarshal(record, &event) != nil {
				return errors.New("invalid MFL event")
			}
			if event.Reply != nil && event.Reply.ID == "file-register" {
				select {
				case registered <- *event.Reply:
				default:
					return errors.New("duplicate file provider registration reply")
				}
			}
			if event.FileQuery != nil {
				if !b.validFileQuery(*event.FileQuery) {
					return errors.New("invalid file provider query")
				}
				select {
				case queries <- *event.FileQuery:
				default:
					return errors.New("file provider query queue is full")
				}
			}
			b.routeJobCallback(event.Echo.Message)
			if b.routeJobReply(event.Echo.Message) {
				return nil
			}
			if err := b.routeDetailCallback(event.Echo.Message); err != nil {
				return err
			}
			b.routeDetailAck(event.Echo.Message)
			b.routeDirectoryRole(event.Echo.Message)
			if strings.HasPrefix(event.Echo.Message, "FVWM_FILES_SYNC "+b.instance+" ") {
				select {
				case b.acks <- event.Echo.Message:
				default:
					return errors.New("browser acknowledgement queue is full")
				}
			}
			if change, ok := parseNativeChange(event.Echo.Message, b.instance); ok {
				offerNativeChange(b.nativeChanges, change)
			}
			if err := b.routeNativeReply(event.Echo.Message); err != nil {
				return err
			}
			if strings.HasPrefix(event.Echo.Message, "FVWM_FILES_3 "+b.instance+" ") {
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
	if err := b.registerProvider(ctx, registered, finished); err != nil {
		return err
	}
	idle := time.NewTimer(idleLimit)
	defer idle.Stop()
	renewIdle := func() {
		if !idle.Stop() {
			select {
			case <-idle.C:
			default:
			}
		}
		idle.Reset(idleLimit)
	}

	var watchTick *time.Ticker
	var watchTicks <-chan time.Time
	defer func() {
		if watchTick != nil {
			watchTick.Stop()
		}
	}()
	var jobLease *time.Ticker
	var jobLeaseTicks <-chan time.Time
	defer func() {
		if jobLease != nil {
			jobLease.Stop()
		}
	}()
	navigation := "push"
	if o.properties {
		navigation = "properties"
	}
	if err := b.queue(o.initialRequest(), navigation); err != nil {
		return err
	}
	// This private receipt confirms provider/work admission, before filesystem
	// resolution or visible presentation. A lost reader must not close the UI.
	writeFileManagerReceipt(o.fileManagerReceipt, "accepted")
	if err := b.show(); err != nil {
		return err
	}
	for {
		jobActive := b.job != nil && !b.job.reaped
		if jobActive && jobLease == nil {
			jobLease = time.NewTicker(30 * time.Second)
			jobLeaseTicks = jobLease.C
		} else if !jobActive && jobLease != nil {
			jobLease.Stop()
			jobLease, jobLeaseTicks = nil, nil
		}
		needsTick := b.watch.needsTick()
		if needsTick && watchTick == nil {
			watchTick = time.NewTicker(100 * time.Millisecond)
			watchTicks = watchTick.C
		} else if !needsTick && watchTick != nil {
			watchTick.Stop()
			watchTick, watchTicks = nil, nil
		}
		b.watchTimerActive = watchTick != nil
		select {
		case <-ctx.Done():
			return nil
		case <-idle.C:
			if b.job != nil && !b.job.reaped {
				idle.Reset(idleLimit)
				continue
			}
			return nil
		case <-jobLeaseTicks:
			if err := b.renewNativeIdle(); err != nil {
				return err
			}
			if b.jobs != nil {
				if err := b.session.Send("SendToModule " + b.jobs.alias + " RenewTimeout 3000"); err != nil {
					return err
				}
			}
			renewIdle()
		case err := <-finished:
			return err
		case query := <-queries:
			applied, err := b.answerProvider(query)
			if err != nil {
				return err
			}
			if applied {
				if err := b.renewNativeIdle(); err != nil {
					return err
				}
				renewIdle()
			}
		case event := <-b.results:
			if err := b.completed(event); err != nil {
				return err
			}
		case event := <-b.watch.events:
			if b.watch.event(event, time.Now()) {
				if err := b.show(); err != nil {
					return err
				}
			}
		case now := <-watchTicks:
			changed := b.watch.tick(now, b.executable)
			if request := b.watch.takeRefresh(now, b.active || b.pending != nil); request != nil {
				if err := b.navigate(string(request.Path), "watch"); err != nil {
					b.watch.fail("Automatic refresh refused")
				}
				changed = true
			}
			if changed {
				if err := b.show(); err != nil {
					return err
				}
			}
		case change := <-b.nativeChanges:
			current, err := b.nativeFeedback(change)
			if err != nil {
				return err
			}
			if current {
				renewIdle()
			}
		case notice := <-b.jobEvents:
			if err := b.jobNotice(notice); err != nil {
				return err
			}
			if err := b.renewNativeIdle(); err != nil {
				return err
			}
			renewIdle()
		case notice := <-b.jobProgress:
			if err := b.jobNotice(notice); err != nil {
				return err
			}
			if err := b.renewNativeIdle(); err != nil {
				return err
			}
			renewIdle()
		case callback := <-b.jobCallbacks:
			accepted, err := b.jobAction(callback)
			if err != nil {
				return err
			}
			if accepted {
				renewIdle()
			}
		case callback := <-b.roleEvents:
			var accepted bool
			var err error
			if callback.file {
				accepted, err = b.fileRoleAction(callback)
			} else {
				accepted, err = b.directoryRoleAction(callback)
			}
			if err != nil {
				return err
			}
			if accepted {
				if err := b.renewNativeIdle(); err != nil {
					return err
				}
				renewIdle()
			}
		case callback := <-b.detailEvents:
			var accepted bool
			var err error
			if callback.action == "open" {
				accepted, err = b.acceptDetailOpen(callback)
			} else {
				accepted, err = b.detailAction(callback)
			}
			if err != nil {
				return err
			}
			if accepted {
				if err := b.renewNativeIdle(); err != nil {
					return err
				}
				renewIdle()
			}
		case message := <-events:
			previousControl := b.lastControl
			close, err := b.action(message)
			if close {
				return nil
			}
			if err != nil {
				b.status = err.Error()
			}
			if b.lastControl != previousControl {
				if err := b.renewNativeIdle(); err != nil {
					return err
				}
			}
			renewIdle()
			if err := b.show(); err != nil {
				return err
			}
		}
	}
}

func main() {
	if len(os.Args) == 2 && os.Args[1] == "--job-worker" {
		if err := runJobWorker(); err != nil {
			fmt.Fprintln(os.Stderr, "fvwm-file-browser: file job worker ended without a confirmed result")
			os.Exit(1)
		}
		return
	}
	if len(os.Args) == 2 && os.Args[1] == "--watch-worker" {
		if err := runWatchWorker(os.Stdin, os.Stdout, watchLease); err != nil {
			fmt.Fprintln(os.Stderr, "fvwm-file-browser: invalid watch request")
			os.Exit(1)
		}
		return
	}
	if len(os.Args) == 2 && os.Args[1] == "--worker" {
		if err := runWorker(os.Stdin, os.Stdout); err != nil {
			fmt.Fprintln(os.Stderr, "fvwm-file-browser: invalid worker request")
			os.Exit(1)
		}
		return
	}
	args := os.Args[1:]
	var admission *os.File
	if len(args) != 0 && args[0] == "--_filemanager-child" {
		var err error
		admission, err = openFileManagerReceipt()
		if err != nil {
			fmt.Fprintln(os.Stderr, "fvwm-file-browser: invalid private admission pipe")
			os.Exit(2)
		}
		defer admission.Close()
		args = args[1:]
	}
	home, _ := os.UserHomeDir()
	o, err := parseOptions(args, home, os.Stderr)
	if errors.Is(err, flag.ErrHelp) {
		return
	}
	if err != nil {
		writeFileManagerReceipt(admission, "refused")
		fmt.Fprintln(os.Stderr, "fvwm-file-browser:", err)
		os.Exit(2)
	}
	o.fileManagerReceipt = admission
	if o.fileManagerService {
		if admission != nil {
			writeFileManagerReceipt(admission, "refused")
			os.Exit(2)
		}
		if err := runFileManagerService(o); err != nil {
			fmt.Fprintln(os.Stderr, "fvwm-file-browser:", err)
			os.Exit(1)
		}
		return
	}
	if o.reuse {
		endpoint := transport.SocketPath(o.socket, os.Getenv, os.Getuid())
		ctx := context.Background()
		if admission != nil {
			var cancel context.CancelFunc
			ctx, cancel = context.WithTimeout(ctx, fileManagerAdmissionWait)
			defer cancel()
		}
		receipt, reused, err := reuseBrowser(ctx, endpoint, o)
		if err != nil {
			writeFileManagerReceipt(admission, "unknown")
			fmt.Fprintln(os.Stderr, "fvwm-file-browser:", err)
			os.Exit(1)
		}
		if reused {
			writeFileManagerReceipt(admission, "accepted")
			fmt.Println(string(receipt))
			return
		}
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
		writeFileManagerReceipt(admission, "unknown")
		fmt.Fprintln(os.Stderr, "fvwm-file-browser:", err)
		os.Exit(1)
	}
}
