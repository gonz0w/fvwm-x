package main

import (
	"context"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/internal/pinboard"
)

func TestWorkerExplicitMissingAndUnavailableStates(t *testing.T) {
	s := pinboard.Empty()
	s.Pins = []pinboard.Pin{{ID: strings.Repeat("a", 32), Target: []byte(filepath.Join(t.TempDir(), "absent")), Label: "Missing"}}
	result := perform(context.Background(), workRequest{Operation: "observe", State: s})
	if result.Status != "observed" || len(result.Observed) != 1 || result.Observed[0].Target.Status != "missing" {
		t.Fatal(result)
	}
	result = perform(context.Background(), workRequest{Operation: "open"})
	if result.Status != "failed" || result.Error == "" {
		t.Fatal(result)
	}
}

func TestLaunchPlanUsesLiteralArgvAndFolderReuse(t *testing.T) {
	path := "/literal ' $[x];\n.desktop"
	program, args, err := launchPlan(workRequest{Program: "/bin/echo"}, path, "file")
	if err != nil || program != "/bin/echo" || len(args) != 1 || args[0] != path {
		t.Fatal(program, args, err)
	}
	_, args, err = launchPlan(workRequest{Browser: "/bin/echo", Socket: "/literal socket"}, "/", "directory")
	if err != nil || strings.Join(args, "|") != "--reuse|--dir|/|--socket|/literal socket" {
		t.Fatal(args, err)
	}
	if _, _, err := launchPlan(workRequest{Program: "/absent/fvwm-pinboard-opener"}, path, "file"); err == nil {
		t.Fatal("unsupported opener accepted")
	}
}

func TestWorkerRefusesStaleObservationBeforeLaunching(t *testing.T) {
	path := filepath.Join(t.TempDir(), "file")
	if err := os.WriteFile(path, []byte("a"), 0600); err != nil {
		t.Fatal(err)
	}
	o, err := filemodel.ObserveTarget([]byte(path))
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, []byte("changed"), 0600); err != nil {
		t.Fatal(err)
	}
	result := perform(context.Background(), workRequest{Operation: "open", Observation: &o, Program: "/bin/echo"})
	if result.Status != "failed" || !strings.Contains(result.Error, "changed") {
		t.Fatal(result)
	}
}
