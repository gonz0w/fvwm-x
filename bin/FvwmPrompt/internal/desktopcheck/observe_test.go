// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcheck

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func TestObserveSharesCheckerAndTracksLiteralOrder(t *testing.T) {
	root := t.TempDir()
	path := filepath.Join(root, "config")
	if err := os.WriteFile(path, []byte("*FvwmDesktop: motion on\nRead child\n*FvwmDesktop: motion off\n"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "child"), []byte("*FvwmDesktop: motion on\n*FvwmDesktop: appearance dark\n"), 0600); err != nil {
		t.Fatal(err)
	}
	r := Request{path, root, root, root}
	checked, err := Check(context.Background(), r)
	if err != nil {
		t.Fatal(err)
	}
	observed, err := Observe(context.Background(), r)
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(checked, observed.Report) {
		t.Fatal("checker report changed in observing traversal")
	}
	if len(observed.Observations) != 4 || observed.Truncated {
		t.Fatalf("%+v", observed)
	}
	for i, o := range observed.Observations {
		if o.Order != i+1 {
			t.Fatal(o)
		}
	}
	if observed.Observations[0].Last || observed.Observations[1].Last || !observed.Observations[2].Last || !observed.Observations[3].Last {
		t.Fatal(observed.Observations)
	}
	child := observed.Observations[1]
	if len(child.IncludedFrom) != 1 || child.IncludedFrom[0].Line != 2 || child.Path != filepath.Join(root, "child") {
		t.Fatal(child)
	}
	data, _ := json.Marshal(checked)
	if strings.Contains(string(data), "observations") {
		t.Fatal("Check schema 1 changed")
	}
}
func TestObserveNeverClaimsLastOnUncheckedOrTruncatedTraversal(t *testing.T) {
	for _, text := range []string{"*FvwmDesktop: motion on\nExec touch marker\n", strings.Repeat("*FvwmDesktop: motion on\n", 130), "*FvwmDesktop: motion on\nRead missing\n"} {
		root := t.TempDir()
		path := filepath.Join(root, "config")
		if err := os.WriteFile(path, []byte(text), 0600); err != nil {
			t.Fatal(err)
		}
		observed, err := Observe(context.Background(), Request{path, root, root, root})
		if err != nil {
			t.Fatal(err)
		}
		for _, o := range observed.Observations {
			if o.Last {
				t.Fatal("last claimed across incomplete traversal")
			}
		}
		if len(observed.Observations) > 128 {
			t.Fatal("observation bound")
		}
		if strings.Count(text, "motion") > 128 && !observed.Truncated {
			t.Fatal("missing truncation")
		}
		if _, err := os.Stat(filepath.Join(root, "marker")); !os.IsNotExist(err) {
			t.Fatal("executed configuration")
		}
	}
}
