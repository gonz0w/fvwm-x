package main

import (
	"bufio"
	"bytes"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"math"
	"net"
	"os"
	"strconv"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/transport"
)

func providerFixture(t *testing.T) (browser, fileQuery) {
	t.Helper()
	snapshot := &filemodel.Snapshot{Schema: 1, Path: []byte("/unavailable/observed")}
	for i := 0; i < 12; i++ {
		name := []byte(fmt.Sprintf("item-%02d", i))
		if i == 11 {
			name = []byte("literal ' \n\xff")
		}
		snapshot.Items = append(snapshot.Items, filemodel.Item{ID: fmt.Sprintf("%032x", i+1), Name: name,
			Kind: "file", Identity: filemodel.Identity{Mode: os.FileMode(0600), Size: int64(i), MTime: math.MaxInt64 - int64(i)}})
	}
	b := browser{instance: strings.Repeat("a", 32), snapshot: snapshot}
	entries, _ := snapshot.OrderedEntries("name", false)
	if err := b.model.Replace(entries); err != nil {
		t.Fatal(err)
	}
	return b, fileQuery{Version: 1, Serial: "0000000000000001", Provider: b.instance, Scope: "items", Limit: 8}
}

func TestProviderPagesLiteralBytesAndExactIntegers(t *testing.T) {
	b, query := providerFixture(t)
	first, code := b.providerResult(query)
	if code != "" {
		t.Fatal(code)
	}
	page := first.(map[string]any)
	items := page["items"].([]providerItem)
	if len(items) != 8 || page["next_offset"] != 8 || page["total"] != 12 {
		t.Fatal(page)
	}
	if items[0].Modified != strconv.FormatInt(math.MaxInt64, 10) {
		t.Fatal("nanoseconds lost integer precision", items[0])
	}
	query.Generation, query.Offset = page["generation"].(string), 8
	second, code := b.providerResult(query)
	if code != "" {
		t.Fatal(code)
	}
	page = second.(map[string]any)
	items = page["items"].([]providerItem)
	if len(items) != 4 || page["next_offset"] != nil || !bytes.Equal(items[3].Name, []byte("literal ' \n\xff")) {
		t.Fatal(page)
	}
	data, _ := json.Marshal(page)
	var decoded struct {
		Items []providerItem `json:"items"`
	}
	if json.Unmarshal(data, &decoded) != nil || !bytes.Equal(decoded.Items[3].Name, items[3].Name) {
		t.Fatal("filename bytes did not round trip")
	}
	query.Offset = 13
	if _, code := b.providerResult(query); code != "offset_out_of_range" {
		t.Fatal(code)
	}
}

func TestProviderSelectionSnapshotAndObservedSummary(t *testing.T) {
	b, query := providerFixture(t)
	previous := b.providerGeneration()
	ids := []string{b.model.Entries[0].ID, b.model.Entries[11].ID}
	if err := b.model.SelectIDs(b.model.Generation, ids); err != nil {
		t.Fatal(err)
	}
	query.Generation = previous
	if _, code := b.providerResult(query); code != "stale_generation" {
		t.Fatal("selection did not invalidate snapshot token", code)
	}
	b.model.SetFilter("no-match")
	query.Generation, query.Scope = "", "selection"
	result, code := b.providerResult(query)
	if code != "" || len(result.(map[string]any)["items"].([]providerItem)) != 2 {
		t.Fatal("hidden selection lost", code, result)
	}
	query.Scope = "properties"
	result, code = b.providerResult(query)
	if code != "" {
		t.Fatal(code)
	}
	properties := result.(map[string]any)["properties"].(map[string]any)
	if properties["count"] != 2 || properties["files"] != 2 || properties["regular_file_bytes"] != "11" || properties["links_followed"] != false {
		t.Fatal(properties)
	}
	query.Scope = "state"
	result, code = b.providerResult(query)
	state := result.(map[string]any)
	if code != "" || state["visible"] != 0 || state["selected"] != 2 || state["file_content"] != false || state["actions"] != false {
		t.Fatal(code, state)
	}
}

func TestProviderRefusesMalformedStaleAndUnreadyQueries(t *testing.T) {
	b, query := providerFixture(t)
	for _, edit := range []func(*fileQuery){
		func(q *fileQuery) { q.Version = 2 },
		func(q *fileQuery) { q.Provider = "other" },
		func(q *fileQuery) { q.Serial = "bad\nEcho" },
		func(q *fileQuery) { q.Scope = "activate" },
		func(q *fileQuery) { q.Offset = -1 },
		func(q *fileQuery) { q.Limit = 9 },
		func(q *fileQuery) { q.Generation = strings.Repeat("A", 32) },
	} {
		bad := query
		edit(&bad)
		if _, code := b.providerResult(bad); code != "invalid_request" {
			t.Fatal(bad, code)
		}
		if b.answerProvider(bad) == nil {
			t.Fatal("invalid input reached native transport")
		}
	}
	query.Offset = 1
	if _, code := b.providerResult(query); code != "generation_required" {
		t.Fatal(code)
	}
	b.snapshot = nil
	if _, code := b.providerResult(query); code != "not_ready" {
		t.Fatal(code)
	}
}

func TestProviderReplyFramingAndResultBudget(t *testing.T) {
	for _, oversized := range []bool{false, true} {
		b, query := providerFixture(t)
		if oversized {
			b.snapshot.Path = bytes.Repeat([]byte("x"), providerResultMax)
			query.Scope = "state"
		}
		left, right := net.Pipe()
		b.session = transport.New(left)
		done := make(chan error, 1)
		go func() { done <- b.answerProvider(query); left.Close() }()
		var data []byte
		parts := 0
		scanner := bufio.NewScanner(right)
		for scanner.Scan() {
			line := scanner.Text()
			fields := strings.Fields(line)
			if len(line) > transport.MaxCommand || len(fields) < 4 || fields[0] != "files_reply" || fields[1] != query.Serial {
				t.Fatal(line)
			}
			if fields[2] == "end" {
				if fields[3] != strconv.Itoa(parts) || len(fields) != 4 {
					t.Fatal(line)
				}
				break
			}
			if len(fields) != 5 || fields[2] != "part" || fields[3] != strconv.Itoa(parts) {
				t.Fatal(line)
			}
			piece, err := hex.DecodeString(fields[4])
			if err != nil || len(piece) > providerChunk {
				t.Fatal(line)
			}
			data = append(data, piece...)
			parts++
		}
		if err := <-done; err != nil || parts > 32 || len(data) > providerResultMax || !json.Valid(data) {
			t.Fatal(err, parts, len(data))
		}
		if oversized && !bytes.Contains(data, []byte("result_too_large")) {
			t.Fatal(string(data))
		}
		right.Close()
	}
}
