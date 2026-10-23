package main

import (
	"bufio"
	"bytes"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestRequestJSONAdmission(t *testing.T) {
	for _, value := range []string{`{}`, `[]`, `{"id":1}`, `{"id":"a","id":"b"}`,
		`{"id":"x"} {}`, `{"id":"x y"}`, `{"id":"` + strings.Repeat("a", 65) + `"}`,
		"{\n\"id\":\"x\"}", strings.Repeat("x", 1000)} {
		if _, err := requestID(value); err == nil {
			t.Fatal("invalid request admitted", value)
		}
	}
	if id, err := requestID(`{"version":1,"id":"valid-1_.","method":"query","resource":"files"}`); err != nil || id != "valid-1_." {
		t.Fatal(id, err)
	}
	o, err := parse([]string{"--socket", "/unused", "--request", `{"id":"a"}`})
	if err != nil || o.request != `{"id":"a"}` || o.socket != "/unused" {
		t.Fatal(o, err)
	}
	for _, args := range [][]string{{"--request"}, {"--request="}, {"--request", "{}", "--request", "{}"}} {
		if _, err := parse(args); err == nil {
			t.Fatal(args)
		}
	}
	if err := execute(options{request: `{"id":"a"}`, stdin: true}, strings.NewReader(""), new(bytes.Buffer), func(string) string { return "" }); err == nil {
		t.Fatal("request combined with legacy command stream")
	}
}

func TestRequestOnceCorrelationExactJSONAndDisconnect(t *testing.T) {
	for _, mode := range []string{"ok", "error", "uncorrelated-error", "wrong-id", "eof"} {
		t.Run(mode, func(t *testing.T) {
			root, err := os.MkdirTemp("", "mfl-rpc-")
			if err != nil {
				t.Fatal(err)
			}
			defer os.RemoveAll(root)
			path := filepath.Join(root, "socket")
			listener, err := net.Listen("unix", path)
			if err != nil {
				t.Fatal(err)
			}
			defer listener.Close()
			done := make(chan error, 1)
			go func() {
				connection, err := listener.Accept()
				if err != nil {
					done <- err
					return
				}
				defer connection.Close()
				fmt.Fprintln(connection, `{"connection_profile":{}}`)
				line, err := bufio.NewReader(connection).ReadString('\n')
				if err != nil || !strings.HasPrefix(line, "request ") {
					done <- fmt.Errorf("request missing: %q %v", line, err)
					return
				}
				fmt.Fprintln(connection, `{"echo":{"message":"unsolicited"}}`)
				reply := `{"reply":{"version":1,"id":"one","status":"ok","result":{"integer":9007199254740993}}}`
				switch mode {
				case "error":
					reply = `{"reply":{"version":1,"id":"one","status":"error","error":{"code":"provider_busy"}}}`
				case "uncorrelated-error":
					reply = `{"reply":{"version":1,"id":null,"status":"error","error":{"code":"invalid_request"}}}`
				case "wrong-id":
					reply = strings.Replace(reply, `"one"`, `"other"`, 1)
				case "eof":
					done <- nil
					return
				}
				for _, fragment := range []string{reply[:13], reply[13:] + "\n"} {
					if _, err := fmt.Fprint(connection, fragment); err != nil {
						done <- err
						return
					}
				}
				done <- nil
			}()
			var output bytes.Buffer
			err = requestOnce(options{socket: path, request: `{"version":1,"id":"one","method":"query","resource":"files"}`}, &output, func(string) string { return "" })
			if (err == nil) != (mode == "ok") {
				t.Fatal(mode, err, output.String())
			}
			if mode == "ok" && !strings.Contains(output.String(), "9007199254740993") {
				t.Fatal("JSON integer rounded", output.String())
			}
			if (mode == "error" || mode == "uncorrelated-error") && !strings.Contains(output.String(), `"status": "error"`) {
				t.Fatal("structured error was not printed", output.String())
			}
			if err := <-done; err != nil {
				t.Fatal(err)
			}
		})
	}
}
