// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package transport

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestQueryAdmissionAndExchangeShareConnection(t *testing.T) {
	for _, admit := range []bool{true, false} {
		t.Run(fmt.Sprint(admit), func(t *testing.T) {
			dir, err := os.MkdirTemp("", "mflq-")
			if err != nil {
				t.Fatal(err)
			}
			defer os.RemoveAll(dir)
			path := filepath.Join(dir, "rpc")
			listener, err := net.Listen("unix", path)
			if err != nil {
				t.Fatal(err)
			}
			defer listener.Close()
			// A valid padded query exercises the distinct client-frame ceiling
			// after read-only admission, on this same connection.
			queryText := `{"version":1,"id":"readonly","method":"query","resource":"appearance"}` + strings.Repeat(" ", MaxCommand)
			finished := make(chan error, 1)
			go func() {
				conn, err := listener.Accept()
				if err != nil {
					finished <- err
					return
				}
				defer conn.Close()
				_ = conn.SetDeadline(time.Now().Add(2 * time.Second))
				reader := bufio.NewReader(conn)
				line, err := reader.ReadString('\n')
				if err != nil {
					finished <- err
					return
				}
				var mode map[string]any
				if !strings.HasPrefix(line, "request ") || json.Unmarshal([]byte(strings.TrimPrefix(line, "request ")), &mode) != nil || mode["method"] != "set_mode" || mode["mode"] != "read-only" || mode["id"] == "readonly" {
					finished <- fmt.Errorf("bad admission %q", line)
					return
				}
				status := "error"
				if admit {
					status = "ok"
				}
				_, err = fmt.Fprintf(conn, "{\"reply\":{\"version\":1,\"id\":%q,\"status\":%q,\"result\":{\"read_only\":%t}}}\n", mode["id"], status, admit)
				if err != nil {
					finished <- err
					return
				}
				if !admit {
					_, err = reader.ReadByte()
					if err != io.EOF {
						finished <- fmt.Errorf("query sent before admission: %v", err)
					} else {
						finished <- nil
					}
					return
				}
				line, err = reader.ReadString('\n')
				if err != nil {
					finished <- err
					return
				}
				var query map[string]any
				if line != "request "+queryText+"\n" || json.Unmarshal([]byte(strings.TrimPrefix(line, "request ")), &query) != nil || query["method"] != "query" || query["id"] != "readonly" || query["resource"] != "appearance" {
					finished <- fmt.Errorf("bad query %q", line)
					return
				}
				_, err = io.WriteString(conn, "{\"reply\":{\"version\":1,\"id\":\"readonly\",\"status\":\"ok\",\"result\":{\"generation\":\"one\"}}}\n")
				finished <- err
			}()
			result, err := Query(context.Background(), path, queryText)
			if admit && (err != nil || !strings.Contains(string(result), `"generation":"one"`)) || !admit && err == nil {
				t.Fatal(string(result), err)
			}
			if err := <-finished; err != nil {
				t.Fatal(err)
			}
		})
	}
}

func TestQueryRejectsMutationBeforeConnecting(t *testing.T) {
	for _, text := range []string{`{"version":1,"id":"x","method":"action"}`, `{"version":1,"id":"x","method":"query","method":"action"}`, `{"version":1,"id":"x","method":"set_mode"}`} {
		if _, err := Query(context.Background(), "not-a-socket", text); err == nil || strings.Contains(err.Error(), "connect") {
			t.Fatal(err)
		}
	}
}
