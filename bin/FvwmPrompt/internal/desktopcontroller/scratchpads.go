// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
package desktopcontroller

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"time"
	"unicode"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

type scratchpadDefinition struct {
	Name, Class, Instance, Fingerprint string
	Argv                               []string
}
type scratchpadRequest struct {
	serial   uint64
	verb     string
	parts    int
	data     []byte
	deadline time.Time
}
type scratchpad struct {
	definition                   scratchpadDefinition
	pending                      *scratchpadRequest
	queuedToggle                 bool
	state, reason, lease, target string
	desired                      bool
	nextStatus                   time.Time
}
type scratchpadService struct {
	cookie      string
	serial      uint64
	definitions []*scratchpad
	dirty       bool
	viewDirty   bool
	view, frame string
	page        int
	launch      func([]string) error
}

func scratchpadText(value string, maximum int, empty bool) bool {
	if (!empty && value == "") || len(value) > maximum || !utf8.ValidString(value) {
		return false
	}
	for _, c := range value {
		if unicode.IsControl(c) {
			return false
		}
	}
	return true
}
func parseScratchpad(text string) (scratchpadDefinition, error) {
	var definition scratchpadDefinition
	if len(text) > 8192 || !utf8.ValidString(text) {
		return definition, errors.New("Scratchpad definition exceeds its UTF-8 byte limit")
	}
	words, err := shellWords(text)
	if err != nil {
		return definition, err
	}
	if len(words) < 4 || len(words) > 67 {
		return definition, errors.New("Scratchpad requires NAME CLASS INSTANCE|- COMMAND [ARG...] (at most64 argv words)")
	}
	definition.Name, definition.Class, definition.Instance = words[0], words[1], words[2]
	if definition.Instance == "-" {
		definition.Instance = ""
	}
	if !identifier(definition.Name) || len(definition.Name) > 32 || !scratchpadText(definition.Class, 128, false) || !scratchpadText(definition.Instance, 128, true) {
		return scratchpadDefinition{}, errors.New("Invalid scratchpad name or exact class/instance")
	}
	for i, word := range words[3:] {
		if !scratchpadText(word, 1024, i != 0) {
			return scratchpadDefinition{}, errors.New("Invalid literal scratchpad argument")
		}
	}
	definition.Argv = append([]string(nil), words[3:]...)
	canonical, _ := json.Marshal(struct {
		Version   int      `json:"schema_version"`
		Name      string   `json:"name"`
		Class     string   `json:"class"`
		Instance  string   `json:"instance"`
		Argv      []string `json:"argv"`
		Placement string   `json:"placement"`
	}{1, definition.Name, definition.Class, definition.Instance, definition.Argv, "center-current-workarea-v1"})
	fingerprint := sha256.Sum256(canonical)
	definition.Fingerprint = hex.EncodeToString(fingerprint[:])
	return definition, nil
}
func (d *Controller) configureScratchpad(text string) error {
	definition, err := parseScratchpad(text)
	if err != nil {
		return err
	}
	if d.scratchpads.cookie == "" {
		d.scratchpads.cookie = newID(16)
	}
	for _, old := range d.scratchpads.definitions {
		if old.definition.Name == definition.Name {
			return errors.New("Duplicate scratchpad name")
		}
	}
	if len(d.scratchpads.definitions) >= 16 {
		return errors.New("At most16 scratchpads may be configured")
	}
	d.scratchpads.definitions = append(d.scratchpads.definitions, &scratchpad{definition: definition, state: "unmatched", reason: "not-observed"})
	return nil
}
func (d *Controller) scratchpadAction(name, verb string) error {
	if !d.ready || d.closed {
		return errors.New("Desktop is not ready")
	}
	if !oneOf(verb, "toggle", "retry", "status") {
		return errors.New("Invalid scratchpad action")
	}
	for _, s := range d.scratchpads.definitions {
		if s.definition.Name != name {
			continue
		}
		if s.pending != nil {
			if verb == "toggle" {
				s.queuedToggle = !s.queuedToggle
				d.scratchpads.viewDirty = true
				return nil
			}
			if verb == "status" {
				return nil
			}
			return errors.New("Scratchpad request is pending; no second launch was submitted")
		}
		if verb == "retry" {
			d.notice("Explicit scratchpad Retry performs a fresh scan but can duplicate an exceptionally late original application.")
		}
		if verb != "status" && d.scratchpads.view != "" {
			d.emit("Style SlateScratchpadsForm !FPReleaseFocus")
			d.emit("UpdateStyles")
		}
		return d.scratchpadSubmit(s, verb)
	}
	return errors.New("No configured scratchpad with that name")
}
func (d *Controller) scratchpadSubmit(s *scratchpad, verb string) error {
	service := &d.scratchpads
	if service.serial == ^uint64(0) {
		return errors.New("Scratchpad request identity exhausted; restart the helper")
	}
	service.serial++
	encode := func(text string) string {
		if text == "" {
			return "-"
		}
		return hex.EncodeToString([]byte(text))
	}
	lease := "-"
	if verb == "launched" || verb == "launch-failed" {
		lease = s.lease
	}
	command := fmt.Sprintf("Send_Scratchpad v1 %s %016x %s %s %s %s %s %s", service.cookie, service.serial, verb, encode(s.definition.Name), s.definition.Fingerprint, encode(s.definition.Class), encode(s.definition.Instance), lease)
	if len(command) > 1000 {
		return errors.New("Scratchpad command exceeds native limit")
	}
	s.pending = &scratchpadRequest{serial: service.serial, verb: verb, deadline: time.Now().Add(5 * time.Second)}
	service.viewDirty = true
	d.emit(command)
	return d.fatal
}
func (d *Controller) scratchpadUnknown(s *scratchpad, reason string) {
	s.pending, s.queuedToggle = nil, false
	s.state, s.reason = "uncertain", reason
	d.scratchpads.viewDirty = true
}
func (d *Controller) scratchpadReply(message string) bool {
	service := &d.scratchpads
	if service.cookie == "" || !strings.HasPrefix(message, "MFL_CORE_1 "+service.cookie+" ") {
		return false
	}
	fields := strings.Fields(strings.TrimPrefix(message, "MFL_CORE_1 "+service.cookie+" "))
	for _, s := range service.definitions {
		q := s.pending
		if q == nil || len(fields) == 0 || fields[0] != fmt.Sprintf("%016x", q.serial) {
			continue
		}
		if !time.Now().Before(q.deadline) {
			d.scratchpadUnknown(s, "core-reply-timeout")
			return true
		}
		done, err := coreFragment(fields[1:], &q.data, &q.parts, 8192)
		if err != nil {
			d.scratchpadUnknown(s, "invalid-core-reply")
			return true
		}
		if !done {
			return true
		}
		s.pending = nil
		if err := d.acceptScratchpad(s, q); err != nil {
			d.scratchpadUnknown(s, "invalid-core-reply")
			d.notice(err.Error())
		}
		return true
	}
	// Older replies from this service lifetime are consumed without effects.
	return true
}
func (d *Controller) acceptScratchpad(s *scratchpad, q *scratchpadRequest) error {
	decoded, err := desktopsettings.DecodeJSON(q.data)
	response, ok := decoded.(map[string]any)
	if err != nil || !ok || !exact(response, "result") {
		return errors.New("Scratchpad core request refused; refresh its manager before retrying")
	}
	result, ok := response["result"].(map[string]any)
	if !ok || !exact(result, "version", "name", "state", "desired_visible", "action", "lease", "target", "reason") {
		return errors.New("Invalid scratchpad result schema")
	}
	var r struct {
		Version int     `json:"version"`
		Name    string  `json:"name"`
		State   string  `json:"state"`
		Desired bool    `json:"desired_visible"`
		Action  string  `json:"action"`
		Lease   *string `json:"lease"`
		Target  *string `json:"target"`
		Reason  string  `json:"reason"`
	}
	data, _ := json.Marshal(result)
	if json.Unmarshal(data, &r) != nil || r.Version != 1 || r.Name != s.definition.Name {
		return errors.New("Invalid scratchpad result identity")
	}
	if _, ok := result["desired_visible"].(bool); !ok {
		return errors.New("Invalid scratchpad desired visibility")
	}
	if !oneOf(r.State, "claimed", "pending", "uncertain", "ambiguous", "unsupported", "unmatched") || !oneOf(r.Action, "none", "launch", "summoned", "dismissed") || !oneOf(r.Reason, "matched", "pending", "uncertain", "ambiguous", "unsupported", "unmatched", "replay", "owner-conflict", "definition-conflict", "lease-mismatch", "table-full", "allocation-failed", "stale-target", "invalid-request", "launch-failed") {
		return errors.New("Invalid scratchpad state/reason")
	}
	for _, token := range []*string{r.Lease, r.Target} {
		if token != nil && (!scratchpadText(*token, 128, false) || strings.ContainsAny(*token, " ")) {
			return errors.New("Invalid scratchpad opaque identity")
		}
	}
	if r.Lease != nil && !identity(*r.Lease, 48) {
		return errors.New("Invalid scratchpad lease identity")
	}
	s.state, s.reason, s.desired = r.State, r.Reason, r.Desired
	s.lease, s.target = "", ""
	if r.Lease != nil {
		s.lease = *r.Lease
	}
	if r.Target != nil {
		s.target = *r.Target
	}
	if r.Action == "launch" {
		if !oneOf(q.verb, "toggle", "retry") || s.lease == "" || r.State != "pending" {
			return errors.New("Unsolicited scratchpad launch permission refused")
		}
		launch := serviceLaunch(d)
		err := launch(append([]string(nil), s.definition.Argv...))
		verb := "launched"
		if err != nil {
			verb = "launch-failed"
			s.queuedToggle = false
			d.notice("Could not start scratchpad " + s.definition.Name + ": " + err.Error())
		}
		return d.scratchpadSubmit(s, verb)
	}
	d.scratchpads.viewDirty = true
	if s.queuedToggle {
		s.queuedToggle = false
		return d.scratchpadSubmit(s, "toggle")
	}
	if s.state == "pending" {
		s.nextStatus = time.Now().Add(time.Second)
	}
	return d.fatal
}
func oneOf(value string, allowed ...string) bool {
	for _, item := range allowed {
		if value == item {
			return true
		}
	}
	return false
}
func serviceLaunch(d *Controller) func([]string) error {
	if d.scratchpads.launch != nil {
		return d.scratchpads.launch
	}
	return func(argv []string) error {
		command, err := d.startDetached(argv, "")
		if err != nil {
			return err
		}
		slots := d.spawnSlots
		go func() { _ = command.Wait(); <-slots }()
		return nil
	}
}
func (d *Controller) scratchpadTick() {
	now := time.Now()
	for _, s := range d.scratchpads.definitions {
		if s.pending != nil && !now.Before(s.pending.deadline) {
			d.scratchpadUnknown(s, "core-reply-timeout")
		}
		if s.pending == nil && (d.scratchpads.dirty || s.state == "pending" && !now.Before(s.nextStatus)) {
			_ = d.scratchpadSubmit(s, "status")
		}
	}
	d.scratchpads.dirty = false
	if d.scratchpads.viewDirty {
		d.scratchpads.viewDirty = false
		d.publishScratchpadState()
	}
}
func (d *Controller) startScratchpads() error {
	if !d.ready || d.closed {
		return errors.New("Desktop is not ready")
	}
	if d.scratchpads.view == "" {
		d.scratchpads.view = newID(16)
		d.emit("Style SlateScratchpadsForm FPGrabFocus, FPReleaseFocus")
	}
	d.scratchpads.dirty = true
	d.showScratchpads()
	return d.fatal
}
func (d *Controller) showScratchpads() {
	service := &d.scratchpads
	if service.view == "" {
		return
	}
	service.frame = newID(16)
	prefix := "Command SendToModule FvwmDesktop scratchpad-control " + service.frame + " "
	lines := []string{`Title "Scratchpads"`, "Colorset 2", "ItemColorset 12", "CloseCommand SendToModule FvwmDesktop scratchpad-control " + service.view + " close"}
	lines = diagnosticText(lines, "Exact configured class/instance; the core owns claims and launch leases.")
	lines = diagnosticText(lines, "Retry can duplicate an exceptionally late application. It always rescans first.")
	if len(service.definitions) == 0 {
		lines = diagnosticText(lines, "No scratchpads configured. Add *FvwmDesktop: Scratchpad NAME CLASS INSTANCE|- COMMAND [ARG...], then restart FvwmDesktop.")
	}
	if len(service.definitions) > 0 {
		service.page = min(service.page, len(service.definitions)-1)
		s := service.definitions[service.page]
		lines = diagnosticText(lines, fmt.Sprintf("%d/%d %s", service.page+1, len(service.definitions), s.definition.Name))
		lines = append(lines, "Line left", "Label scratchpad_state 64 "+quote(scratchpadState(s)))
		lines = diagnosticText(lines, "Class: "+s.definition.Class+"; instance: "+s.definition.Instance)
		lines = append(lines, "Line left", "Button continue "+quote("Toggle "+s.definition.Name), prefix+"toggle "+s.definition.Name)
		lines = append(lines, "Button continue "+quote("Explicit Retry "+s.definition.Name), prefix+"retry "+s.definition.Name)
	}
	for _, step := range []struct {
		label, key string
		delta      int
	}{{"Previous (F1)", "F1", -1}, {"Next (F2)", "F2", 1}} {
		page := service.page + step.delta
		if page >= 0 && page < len(service.definitions) {
			lines = append(lines, "Button continue "+quote(step.label)+" "+step.key, prefix+"page "+strconv.Itoa(page))
		}
	}
	lines = append(lines, "Line expand", `Button continue "Refresh (F5)" F5`, prefix+"refresh", `Button quit "Close (Esc)" ^[`, "Command SendToModule FvwmDesktop scratchpad-control "+service.view+" close")
	d.form("SlateScratchpadsForm", lines, true)
}
func scratchpadState(s *scratchpad) string {
	text := s.state + "; " + s.reason + "; desired visible: " + strconv.FormatBool(s.desired)
	if s.pending != nil {
		text += "; awaiting " + s.pending.verb
	}
	if s.queuedToggle {
		text += "; toggle queued"
	}
	return text
}
func (d *Controller) publishScratchpadState() {
	service := &d.scratchpads
	if service.view == "" || len(service.definitions) == 0 {
		return
	}
	d.emit("SendToModule SlateScratchpadsForm SetLabel scratchpad_state " + quote(scratchpadState(service.definitions[service.page])))
}
func (d *Controller) scratchpadControl(argument string) error {
	words := strings.Fields(argument)
	if len(words) < 2 {
		return errors.New("Invalid scratchpad callback")
	}
	service := &d.scratchpads
	if words[1] == "close" && len(words) == 2 && words[0] == service.view {
		d.closeScratchpads()
		return d.fatal
	}
	if words[0] != service.frame || service.view == "" {
		return nil
	}
	if words[1] == "refresh" && len(words) == 2 {
		service.dirty = true
		return nil
	}
	if words[1] == "page" && len(words) == 3 {
		page, err := strconv.Atoi(words[2])
		if err != nil || page < 0 || page >= len(service.definitions) {
			return errors.New("Invalid scratchpad manager page")
		}
		service.page = page
		d.showScratchpads()
		return d.fatal
	}
	if len(words) == 3 && oneOf(words[1], "toggle", "retry") {
		return d.scratchpadAction(words[2], words[1])
	}
	return errors.New("Invalid scratchpad manager action")
}

func (d *Controller) closeScratchpads() {
	if d.scratchpads.view == "" {
		return
	}
	d.scratchpads.view, d.scratchpads.frame = "", ""
	d.emit("KillModule FvwmForm SlateScratchpadsForm")
}
