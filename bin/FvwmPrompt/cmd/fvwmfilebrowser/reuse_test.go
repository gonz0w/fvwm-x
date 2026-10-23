package main

import (
	"bufio"
	"context"
	"encoding/json"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"io"
	"net"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestReuseOptionsAndLiteralPlans(t *testing.T) {
	for _, args := range [][]string{{"--reuse", "--inspect"}, {"--reuse", "--view", "grid"}, {"--reuse", "--action", "x=program"}, {"--dir", "/a", "/b"}, {"--reveal", "/a", "/b"}, {"/a", "/b"}, {"https://remote/a"}} {
		if _, err := parseOptions(args, "/home", io.Discard); err == nil {
			t.Fatal("invalid options", args)
		}
	}
	for _, args := range [][]string{{"--reuse"}, {"--reuse", "file:///tmp/a%20b"}, {"--reuse", "--reveal", "/tmp/a"}, {"--properties", "/tmp/a", "--properties", "/tmp/b"}, {"--dir", "/tmp", "--view", "grid"}} {
		o, err := parseOptions(args, "/home", io.Discard)
		if err != nil {
			t.Fatal(args, err)
		}
		if !o.reuse {
			continue
		}
		_, _, _, eligible, err := o.reusePlan()
		if err != nil || !eligible {
			t.Fatal(args, err)
		}
	}
	o := options{targets: [][]byte{[]byte("/tmp/literal \n\xff")}, properties: true}
	folder, op, values, eligible, err := o.reusePlan()
	data, _ := json.Marshal(values)
	if folder != "/tmp" || op != "inspect" || !eligible || err != nil || strings.Contains(string(data), "literal") {
		t.Fatal(folder, op, values, err)
	}
	o.targets = [][]byte{[]byte("/tmp/a"), []byte("/other/b")}
	if _, _, _, _, err = o.reusePlan(); err == nil {
		t.Fatal("mixed parents admitted")
	}
	o.targets = [][]byte{[]byte("/" + strings.Repeat("x", 193))}
	if _, _, _, eligible, err = o.reusePlan(); eligible || err != nil {
		t.Fatal("long target did not retain launch fallback")
	}
}

func TestReuseRoutingAdmissionAndUnknownOutcome(t *testing.T) {
	for _, mode := range []string{"success", "busy", "nonmatching", "old-capabilities", "stale-once", "lost-reply", "present-stale-once"} {
		t.Run(mode, func(t *testing.T) {
			root, err := os.MkdirTemp("", "mflrpc-")
			if err != nil {
				t.Fatal(err)
			}
			defer os.RemoveAll(root)
			endpoint := filepath.Join(root, "rpc")
			listener, err := net.Listen("unix", endpoint)
			if err != nil {
				t.Fatal(err)
			}
			provider, generation := strings.Repeat("a", 32), strings.Repeat("b", 32)
			operations := make(chan string, 16)
			done := make(chan struct{})
			calls := 0
			go func() {
				defer close(done)
				for {
					conn, err := listener.Accept()
					if err != nil {
						return
					}
					line, err := bufio.NewReader(conn).ReadString('\n')
					if err != nil {
						conn.Close()
						continue
					}
					var request map[string]json.RawMessage
					if !strings.HasPrefix(line, "request ") || json.Unmarshal([]byte(strings.TrimPrefix(line, "request ")), &request) != nil {
						conn.Close()
						continue
					}
					var id, method, operation string
					json.Unmarshal(request["id"], &id)
					json.Unmarshal(request["method"], &method)
					json.Unmarshal(request["operation"], &operation)
					var result any
					code := ""
					switch {
					case request["provider"] == nil:
						result = map[string]any{"providers": []any{map[string]any{"provider": provider, "busy": mode == "busy"}}}
					case method == "query":
						path := "/folder"
						if mode == "nonmatching" {
							path = "/other"
						}
						actions := []string{"reveal", "present"}
						if mode == "old-capabilities" {
							actions = []string{"reveal"}
						}
						result = map[string]any{"path_bytes": []byte(path), "generation": generation, "busy": false, "action_operations": actions}
					case operation == "reveal":
						calls++
						operations <- operation
						if mode == "lost-reply" {
							conn.Close()
							continue
						}
						if mode == "stale-once" && calls == 1 {
							code = "stale_generation"
						} else {
							result = map[string]any{"provider": provider, "generation": generation, "applied": true, "work": map[string]any{"id": "0000000000000001", "operation": "reveal", "status": "accepted"}}
						}
					case operation == "present":
						operations <- operation
						if mode == "present-stale-once" && calls == 1 {
							calls++
							code = "stale_generation"
						} else {
							result = map[string]any{"applied": true}
						}
					}
					reply := map[string]any{"version": 1, "id": id, "status": "ok", "result": result}
					if code != "" {
						reply = map[string]any{"version": 1, "id": id, "status": "error", "error": map[string]string{"code": code}}
					}
					json.NewEncoder(conn).Encode(map[string]any{"reply": reply})
					conn.Close()
				}
			}()
			ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
			defer cancel()
			data, reused, err := reuseBrowser(ctx, endpoint, options{targets: [][]byte{[]byte("/folder/item")}})
			listener.Close()
			<-done
			close(operations)
			var got []string
			for op := range operations {
				got = append(got, op)
			}
			fallback := mode == "busy" || mode == "nonmatching" || mode == "old-capabilities"
			if fallback {
				if reused || err != nil || len(got) != 0 {
					t.Fatal(mode, reused, err, got)
				}
				return
			}
			if mode == "lost-reply" {
				if reused || err == nil || len(got) != 1 {
					t.Fatal("ambiguous handoff replayed", reused, err, got)
				}
				return
			}
			if err != nil || !reused || !strings.Contains(string(data), `"status":"accepted"`) {
				t.Fatal(mode, reused, err, string(data))
			}
			if mode == "stale-once" {
				if len(got) != 3 {
					t.Fatal(got)
				}
			} else if mode == "present-stale-once" {
				if len(got) != 3 {
					t.Fatal(got)
				}
			} else if len(got) != 2 {
				t.Fatal(got)
			}
		})
	}
}

func TestInstalledFolderEntryUsesLiteralSingleURI(t *testing.T) {
	data, err := os.ReadFile("../../fvwm-file-browser.desktop")
	if err != nil {
		t.Fatal(err)
	}
	desktop, err := xdgmenu.ParseDesktop(data)
	if err != nil {
		t.Fatal(err)
	}
	group := desktop.Groups["Desktop Entry"]
	if group["Type"] != "Application" || group["MimeType"] != "inode/directory;" || group["TryExec"] != "fvwm-file-browser" || group["DBusActivatable"] == "true" {
		t.Fatal(group)
	}
	args, err := xdgmenu.ExpandExec(group["Exec"], xdgmenu.ExecContext{URLs: []string{"--socket=/untrusted"}})
	if err != nil {
		t.Fatal(err)
	}
	if _, err := parseOptions(args[1:], "/home", io.Discard); err == nil {
		t.Fatal("desktop URI became a command option")
	}
	for _, urls := range [][]string{nil, {"file:///tmp/literal%20%27%3B%24%5Bexec.touch%5D"}} {
		argv, err := xdgmenu.ExpandExec(group["Exec"], xdgmenu.ExecContext{URLs: urls})
		if err != nil {
			t.Fatal(err)
		}
		if len(argv) != 3+len(urls) || argv[0] != "fvwm-file-browser" || argv[1] != "--reuse" || argv[2] != "--" {
			t.Fatal(argv)
		}
		o, err := parseOptions(argv[1:], "/home", io.Discard)
		if err != nil || !o.reuse {
			t.Fatal(argv, err)
		}
		if len(urls) == 0 && o.dir != "/home" {
			t.Fatal(o.dir)
		}
		if len(urls) != 0 && o.dir != "/tmp/literal ';$[exec.touch]" {
			t.Fatal(o.dir)
		}
	}
}
