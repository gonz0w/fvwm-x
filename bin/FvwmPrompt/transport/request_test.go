package transport

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func requestAtLimit(bytes int) string {
	base := `{"id":"limit","padding":""}`
	return strings.Replace(base, `"padding":""`, `"padding":"`+strings.Repeat("x", bytes-len("request ")-len(base))+`"`, 1)
}

func TestStructuredRequestAndRawCommandHaveSeparateBounds(t *testing.T) {
	accepted, refused := requestAtLimit(MaxRequest), requestAtLimit(MaxRequest+1)
	if len("request "+accepted) != 4096 || len("request "+refused) != 4097 {
		t.Fatal("incorrect boundary fixture")
	}
	if id, err := RequestID(accepted); err != nil || id != "limit" {
		t.Fatal("4096-byte structured frame refused", id, err)
	}
	if _, err := RequestID(refused); err == nil {
		t.Fatal("4097-byte structured frame admitted")
	}
	conn := &stubConn{chunk: 17}
	session := New(conn)
	defer session.Close()
	if err := session.sendRequest(accepted); err != nil || conn.String() != "request "+accepted+"\n" {
		t.Fatal("structured frame did not complete", err)
	}
	before := conn.String()
	if err := session.sendRequest(refused); err == nil || conn.closed || conn.String() != before {
		t.Fatal("oversized local request wrote bytes or lost session")
	}
	if err := session.Send(strings.Repeat("x", 1001)); err == nil || conn.closed || conn.String() != before {
		t.Fatal("client request ceiling widened raw commands")
	}
	if err := session.Send("request " + accepted); err == nil || conn.String() != before {
		t.Fatal("unvalidated raw request widened public Send")
	}
}

func TestTypedResourceRolePlanCrossesOrdinaryCommandLimit(t *testing.T) {
	roles := []map[string]any{}
	for _, id := range []int{0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14} {
		roles = append(roles, map[string]any{"id": id, "foreground": "#ffffff",
			"background": "#000000", "highlight": "#000000", "shadow": "#000000"})
	}
	data, err := json.Marshal(map[string]any{"version": 1, "id": "roles", "method": "action",
		"resource": "preview", "operation": "prepare", "generation": strings.Repeat("a", 48),
		"values": map[string]any{"plan_version": 1, "roles": roles}})
	if err != nil || len(data)+8 <= MaxCommand || len(data)+8 > MaxRequest {
		t.Fatal("role-plan fixture does not exercise separate client bound", len(data), err)
	}
	dir, err := os.MkdirTemp("", "mflroles-")
	if err != nil {
		t.Fatal(err)
	}
	defer os.RemoveAll(dir)
	endpoint := filepath.Join(dir, "rpc")
	listener, err := net.Listen("unix", endpoint)
	if err != nil {
		t.Fatal(err)
	}
	defer listener.Close()
	finished := make(chan error, 1)
	go func() {
		conn, err := listener.Accept()
		if err != nil {
			finished <- err
			return
		}
		defer conn.Close()
		_ = conn.SetDeadline(time.Now().Add(2 * time.Second))
		line, err := bufio.NewReader(conn).ReadString('\n')
		if err != nil || line != "request "+string(data)+"\n" {
			finished <- fmt.Errorf("typed role plan changed or truncated: %v", err)
			return
		}
		_, err = fmt.Fprintln(conn, `{"reply":{"version":1,"id":"roles","status":"ok","result":{"state":"prepared"}}}`)
		finished <- err
	}()
	_, err = Request(context.Background(), endpoint, string(data))
	if err != nil {
		t.Fatal("typed plan did not traverse ordinary request transport", err)
	}
	if err := <-finished; err != nil {
		t.Fatal(err)
	}
}

func TestRequestCancellationClosesAndJoinsReader(t *testing.T) {
	for _, test := range []struct {
		name string
		call func(context.Context, string, string) (json.RawMessage, error)
	}{{"request", Request}, {"query", Query}} {
		t.Run(test.name, func(t *testing.T) { requestCancellation(t, test.call) })
	}
}
func requestCancellation(t *testing.T, call func(context.Context, string, string) (json.RawMessage, error)) {
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
	defer listener.Close()
	observed := make(chan error, 1)
	ready := make(chan struct{})
	go func() {
		conn, err := listener.Accept()
		if err != nil {
			observed <- err
			return
		}
		defer conn.Close()
		reader := bufio.NewReader(conn)
		_, err = reader.ReadString('\n')
		close(ready)
		if err == nil {
			_, err = reader.ReadByte()
		}
		observed <- err
	}()
	ctx, cancel := context.WithCancel(context.Background())
	finished := make(chan error, 1)
	go func() {
		_, err := call(ctx, endpoint, `{"version":1,"id":"cancel","method":"query","resource":"files"}`)
		finished <- err
	}()
	<-ready
	cancel()
	select {
	case err := <-finished:
		if err != context.Canceled {
			t.Fatal(err)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("request reader not joined")
	}
	select {
	case err := <-observed:
		if err == nil {
			t.Fatal("connection remained open")
		}
	case <-time.After(2 * time.Second):
		t.Fatal("peer did not observe close")
	}
}
