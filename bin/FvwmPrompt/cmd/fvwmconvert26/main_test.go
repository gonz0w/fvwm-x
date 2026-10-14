// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"errors"
	"io"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"syscall"
	"testing"
)

func TestSplitCaptureLimit(t *testing.T) {
	got := splitFields(spaceCapture, "Key Tab A M next [x] Raise", 5)
	want := []string{"Key", " ", "Tab", " ", "A", " ", "M", " ", "next [x] Raise"}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("split: %#v", got)
	}
	if got := splitFields(spaces, " I Echo  initial", 3); !reflect.DeepEqual(got, []string{"", "I", "Echo  initial"}) {
		t.Fatalf("split: %#v", got)
	}
}

func TestConditionalHeuristic(t *testing.T) {
	for input, want := range map[string]string{
		"Next [XTerm] Raise": "Next [XTerm] Raise",
		"next [XTerm] Raise": "(next) Raise",
		"next [*] Raise":     "next  Raise",
		"next (A B) Raise":   "next (,  , )  Raise",
	} {
		if got := conditional(input); got != want {
			t.Errorf("%q: %q, expected %q", input, got, want)
		}
	}
}

func TestFunctionOrder(t *testing.T) {
	c := &converter{}
	for _, line := range []string{"# retained", "AddToFunc RestartFunction", "+ I Echo restart", "AddToFunc InitFunction", "+ I Echo initial", "AddToFunc StartFunction", "+ I Echo start"} {
		c.dispatch(line)
	}
	var out bytes.Buffer
	if err := c.writeOutput(&out); err != nil {
		t.Fatal(err)
	}
	want := "# retained\n\nDestroyFunc StartFunction\nAddToFunc StartFunction\n+  I Test (Init)  Echo initial\n+  I Echo restart\n+  I Echo start\n"
	if out.String() != want {
		t.Fatalf("output: %q", out.String())
	}
}

type failedWriter struct{}

func (failedWriter) Write([]byte) (int, error) { return 0, io.ErrClosedPipe }

func TestOutputFailure(t *testing.T) {
	c := &converter{}
	c.dispatch("WindowFont fixed")
	if err := c.writeOutput(failedWriter{}); !errors.Is(err, io.ErrClosedPipe) {
		t.Fatal(err)
	}
}

func TestLogicalLineBoundary(t *testing.T) {
	directory := t.TempDir()
	path := filepath.Join(directory, "source")
	for _, size := range []int{logicalLimit - 1, logicalLimit, logicalLimit + 1} {
		if err := os.WriteFile(path, []byte(strings.Repeat("x", size)), 0600); err != nil {
			t.Fatal(err)
		}
		_, err := convertFile(path, "")
		if (err == nil) != (size <= logicalLimit) {
			t.Fatalf("size %d: %v", size, err)
		}
	}
	if err := os.WriteFile(path, []byte("# first \\\nsecond \\\nthird\n"), 0600); err != nil {
		t.Fatal(err)
	}
	c, err := convertFile(path, "")
	if err != nil {
		t.Fatal(err)
	}
	var out bytes.Buffer
	c.writeOutput(&out)
	if out.String() != "# first \\\nsecond \\\nthird" {
		t.Fatalf("continuation: %q", out.String())
	}
}

func TestReadBudgetBoundary(t *testing.T) {
	c := &converter{}
	for i := 0; i < readLimit; i++ {
		c.dispatch("Read literal")
	}
	if c.err != nil || len(c.reads) != readLimit {
		t.Fatal(c.err, len(c.reads))
	}
	c.dispatch("Read excess")
	if c.err == nil || len(c.reads) != readLimit {
		t.Fatal(c.err, len(c.reads))
	}
}

func TestSourceStatus(t *testing.T) {
	err := sourceError(&os.PathError{Op: "open", Path: "absent", Err: syscall.ENOENT})
	if errorStatus(err) != 2 || err.Error() != "Unable to open source file:  No such file or directory" {
		t.Fatal(err)
	}
}

func TestJobReadBudgetIncludesMissingReferences(t *testing.T) {
	directory := t.TempDir()
	source := filepath.Join(directory, "source")
	destination := filepath.Join(directory, "first-output")
	if err := os.WriteFile(source, []byte("Read absent-first\nRead absent-second\n"), 0600); err != nil {
		t.Fatal(err)
	}
	job := &conversionJob{cwd: directory, userdir: directory,
		home: directory, out: io.Discard, diagnostics: io.Discard,
		references: readLimit - 2}
	if err := job.one(source, destination); err != nil {
		t.Fatal(err)
	}
	if job.references != readLimit || len(job.queue) != 0 {
		t.Fatal(job.references, job.queue)
	}
	destination = filepath.Join(directory, "second-output")
	if err := job.one(source, destination); err == nil {
		t.Fatal("accepted references beyond the job limit")
	}
	if _, err := os.Stat(destination); !os.IsNotExist(err) {
		t.Fatal("published failed conversion", err)
	}
	if job.references != readLimit {
		t.Fatal("failed conversion changed reference count", job.references)
	}
}

func TestPublicationPreservesExistingFile(t *testing.T) {
	path := filepath.Join(t.TempDir(), "destination")
	if err := os.WriteFile(path, []byte("retained"), 0600); err != nil {
		t.Fatal(err)
	}
	c := &converter{}
	c.dispatch("WindowFont fixed")
	if err := publish(path, c); !os.IsExist(err) {
		t.Fatal(err)
	}
	data, err := os.ReadFile(path)
	if err != nil || string(data) != "retained" {
		t.Fatal(string(data), err)
	}
	matches, err := filepath.Glob(filepath.Join(filepath.Dir(path), ".fvwm-convert-2.6-*"))
	if err != nil || len(matches) != 0 {
		t.Fatal(matches, err)
	}
}
