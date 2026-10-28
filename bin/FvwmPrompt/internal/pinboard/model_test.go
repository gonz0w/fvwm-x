package pinboard

import (
	"bytes"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"syscall"
	"testing"
)

func testPin() Pin {
	return Pin{ID: strings.Repeat("a", 32), Target: []byte("/literal\xff $[x];\n"), Label: "Literal reference"}
}

func TestBoundedStatePreservesLiteralBytesAndStableIndependentIDs(t *testing.T) {
	s := Empty()
	s.Pins = []Pin{testPin(), {ID: strings.Repeat("b", 32), Target: []byte("/"), Label: "Root"}}
	data, err := s.Encode()
	if err != nil {
		t.Fatal(err)
	}
	got, err := Decode(data)
	if err != nil || !bytes.Equal(got.Pins[0].Target, s.Pins[0].Target) {
		t.Fatal(got, err)
	}
	move := got.Pins[0]
	move.Row = 42
	move.Monitor = []byte("missing-monitor")
	next, err := got.Change("move", move)
	if err != nil || next.Pins[0].ID != got.Pins[0].ID || got.Pins[0].Row != 0 {
		t.Fatal(next, got, err)
	}
	seen := map[string]bool{}
	for i := 0; i < 256; i++ {
		id, err := NewID()
		if err != nil || !Hex(id, 32) || seen[id] {
			t.Fatal(id, err)
		}
		seen[id] = true
	}
}

func TestInvalidStateNeverPublishesPartialModel(t *testing.T) {
	for _, mutate := range []func(*State){
		func(s *State) { s.Schema = 2 }, func(s *State) { s.Pins = append(s.Pins, s.Pins[0]) },
		func(s *State) { s.Pins[0].Target = []byte("file:///tmp/a") }, func(s *State) { s.Pins[0].Target = []byte("/bad\x00") },
		func(s *State) { s.Pins[0].Target = []byte("/tmp/../target") },
		func(s *State) { s.Pins[0].Label = "bad\nlabel" }, func(s *State) { s.Pins[0].Icon = "../bad" },
		func(s *State) { s.Layout.CellWidth = 63 }, func(s *State) { s.Pins[0].Row = -1 },
		func(s *State) { s.Pins[0].Monitor = make([]byte, 257) }, func(s *State) { s.Pins[0].Label = strings.Repeat("x", 257) },
	} {
		s := Empty()
		s.Pins = []Pin{testPin()}
		mutate(&s)
		data, _ := json.Marshal(s)
		if _, err := Decode(data); err == nil {
			t.Fatal("accepted", string(data))
		}
	}
	for _, data := range []string{`{"schema_version":1,"schema_version":1}`, `{} {}`, "{\"label\":\"\xff\"}", strings.Repeat("x", MaxStateBytes+1)} {
		if _, err := Decode([]byte(data)); err == nil {
			t.Fatal("accepted malformed record")
		}
	}
}

func TestSaveRevisionLockCorruptionAndNoTargetMutation(t *testing.T) {
	dir := t.TempDir()
	if err := os.Chmod(dir, 0700); err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(dir, "pins.json")
	target := filepath.Join(dir, "untouched")
	if err := os.WriteFile(target, []byte("target"), 0600); err != nil {
		t.Fatal(err)
	}
	s := Empty()
	p := testPin()
	p.Target = []byte(target)
	s.Pins = []Pin{p}
	s, err := Save(path, 0, s)
	if err != nil || s.Revision != 1 {
		t.Fatal(s, err)
	}
	prior, _ := os.ReadFile(path)
	if _, err := Save(path, 0, Empty()); err == nil {
		t.Fatal("stale save accepted")
	}
	fd, err := syscall.Open(path+".lock", syscall.O_RDWR, 0)
	if err != nil {
		t.Fatal(err)
	}
	if err := syscall.Flock(fd, syscall.LOCK_EX|syscall.LOCK_NB); err != nil {
		t.Fatal(err)
	}
	if _, err := Save(path, 1, s); err == nil {
		t.Fatal("lock contention accepted")
	}
	syscall.Close(fd)
	after, _ := os.ReadFile(path)
	if !bytes.Equal(prior, after) {
		t.Fatal("failed save modified state")
	}
	next, err := s.Change("unpin", p)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := Save(path, 1, next); err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(target)
	if err != nil || string(data) != "target" {
		t.Fatal("unpin mutated target", err)
	}
	if err := os.WriteFile(path, []byte("corrupt"), 0600); err != nil {
		t.Fatal(err)
	}
	retry := Empty()
	retry.Revision = 2
	if _, err := Save(path, 2, retry); err == nil {
		t.Fatal("corruption overwritten")
	}
	data, _ = os.ReadFile(path)
	if string(data) != "corrupt" {
		t.Fatal("corrupt state replaced")
	}
}

func TestMaximumCountAndRequiredTypedStateFields(t *testing.T) {
	s := Empty()
	for i := 0; i < MaxPins; i++ {
		p := testPin()
		p.ID = fmt.Sprintf("%032x", i)
		s.Pins = append(s.Pins, p)
	}
	data, err := s.Encode()
	if err != nil {
		t.Fatal(err)
	}
	if _, err := Decode(data); err != nil {
		t.Fatal(err)
	}
	p := testPin()
	p.ID = strings.Repeat("f", 32)
	if _, err := s.Change("create", p); err == nil {
		t.Fatal("257th pin accepted")
	}
	valid, _ := Empty().Encode()
	for _, data := range []string{
		strings.Replace(string(valid), `"revision":0,`, "", 1),
		strings.Replace(string(valid), `"revision":0`, `"revision":null`, 1),
		strings.Replace(string(valid), `"pins":[]`, `"pins":null`, 1),
		strings.Replace(string(valid), `"cell_width":96,`, "", 1),
	} {
		if _, err := Decode([]byte(data)); err == nil {
			t.Fatal("missing/null typed field accepted", data)
		}
	}
}

func TestPrivateStorageRefusesSymlinkHardlinkAndPublicDirectory(t *testing.T) {
	dir := t.TempDir()
	if err := os.Chmod(dir, 0700); err != nil {
		t.Fatal(err)
	}
	real := filepath.Join(dir, "real")
	data, _ := Empty().Encode()
	if err := os.WriteFile(real, data, 0600); err != nil {
		t.Fatal(err)
	}
	link := filepath.Join(dir, "link")
	if err := os.Symlink(real, link); err != nil {
		t.Fatal(err)
	}
	if _, err := Load(link); err == nil {
		t.Fatal("symlink read")
	}
	hard := filepath.Join(dir, "hard")
	if err := os.Link(real, hard); err != nil {
		t.Fatal(err)
	}
	if _, err := Load(hard); err == nil {
		t.Fatal("hardlink read")
	}
	if err := os.Chmod(dir, 0755); err != nil {
		t.Fatal(err)
	}
	if _, err := Save(filepath.Join(dir, "absent"), 0, Empty()); err == nil {
		t.Fatal("public directory used")
	}
}
