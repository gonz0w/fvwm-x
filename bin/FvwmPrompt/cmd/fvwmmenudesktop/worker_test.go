// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"context"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

func TestMain(m *testing.M) {
	if len(os.Args) > 2 && os.Args[1] == "--_worker" {
		fmt.Print("complete-or-partial\n")
		switch os.Args[2] {
		case "failure":
			fmt.Fprintln(os.Stderr, "fixture refusal")
			os.Exit(1)
		case "timeout":
			time.Sleep(time.Minute)
		}
		os.Exit(0)
	}
	if len(os.Args) > 2 && os.Args[1] == "--launch" {
		if err := launch(os.Args[2:]); err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		os.Exit(0)
	}
	if len(os.Args) > 2 && os.Args[1] == "--record-json" {
		cwd, _ := os.Getwd()
		data, _ := json.Marshal(struct {
			Args      []string
			Directory string
		}{os.Args[3:], cwd})
		if err := os.WriteFile(os.Args[2], data, 0600); err != nil {
			os.Exit(1)
		}
		os.Exit(0)
	}
	os.Exit(m.Run())
}
func TestWorkerCompletionBoundary(t *testing.T) {
	executable, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	for _, mode := range []string{"success", "failure", "timeout"} {
		t.Run(mode, func(t *testing.T) {
			deadline := 15 * time.Second
			if mode == "timeout" {
				deadline = 100 * time.Millisecond
			}
			ctx, cancel := context.WithTimeout(context.Background(), deadline)
			defer cancel()
			var out, diagnostics bytes.Buffer
			err := supervise(ctx, executable, []string{mode}, &out, &diagnostics)
			if mode == "success" {
				if err != nil || out.String() != "complete-or-partial\n" {
					t.Fatal(out.String(), err)
				}
			} else if err == nil || out.Len() != 0 {
				t.Fatal("exposed partial output", err, out.String())
			}
		})
	}
	var limited = boundedOutput{limit: 3}
	if _, err := limited.Write([]byte("four")); err == nil || limited.Len() != 0 {
		t.Fatal("accepted excess output")
	}
}
func TestLiteralApplicationLaunch(t *testing.T) {
	executable, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	for _, terminal := range []bool{false, true} {
		t.Run(fmt.Sprint(terminal), func(t *testing.T) {
			root := t.TempDir()
			output := filepath.Join(root, "argv.json")
			command := ""
			if terminal {
				command = "'" + strings.ReplaceAll(executable, "'", `'\''`) + "' --record-json '" + output + "'"
			}
			e, err := newActionEncoder(root, "test", command)
			if err != nil {
				t.Fatal(err)
			}
			values := []string{"two words", `quote'"$[literal]`, "line\nnext", "%"}
			argv := append([]string{executable, "--record-json", output}, values...)
			if terminal {
				argv = values
				argv = append([]string{"literal-program"}, argv...)
			}
			action, err := e.encode(xdgmenu.PresentedItem{Argv: argv, WorkingDir: root, Terminal: terminal})
			if err != nil {
				t.Fatal(err)
			}
			if err = e.publish(); err != nil {
				t.Fatal(err)
			}
			parts := strings.Fields(action)
			if parts[4] != base64.RawURLEncoding.EncodeToString([]byte(e.path)) {
				t.Fatal(parts)
			}
			cmd := exec.Command(executable, "--launch", parts[4], parts[5])
			if out, err := cmd.CombinedOutput(); err != nil {
				t.Fatal(err, string(out))
			}
			data, err := os.ReadFile(output)
			if err != nil {
				t.Fatal(err)
			}
			var record struct {
				Args      []string
				Directory string
			}
			if err = json.Unmarshal(data, &record); err != nil {
				t.Fatal(err)
			}
			expected := values
			if terminal {
				expected = argv
			}
			canonical, err := filepath.EvalSymlinks(root)
			if err != nil {
				t.Fatal(err)
			}
			if !reflect.DeepEqual(record.Args, expected) || record.Directory != canonical {
				t.Fatal(record, expected)
			}
		})
	}
}
