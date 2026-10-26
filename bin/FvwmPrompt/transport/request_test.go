package transport

import (
	"bufio"
	"context"
	"encoding/json"
	"net"
	"os"
	"path/filepath"
	"testing"
	"time"
)

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
