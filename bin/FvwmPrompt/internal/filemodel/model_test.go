package filemodel

import (
	"bytes"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
)

func TestLiteralNamesHiddenEmptyAndStableRefresh(t *testing.T) {
	root := t.TempDir()
	empty, err := Scan(root, false)
	if err != nil || len(empty.Items) != 0 {
		t.Fatal(empty, err)
	}
	names := []string{"line\nnext", `quote" $[exec] percent% backtick` + "`", ".hidden", "-option"}
	for _, name := range names {
		if err := os.WriteFile(filepath.Join(root, name), []byte("file"), 0600); err != nil {
			t.Fatal(err)
		}
	}
	os.Mkdir(filepath.Join(root, "folder"), 0700)
	s, err := Scan(root, false)
	if err != nil || len(s.Items) != len(names) {
		t.Fatal(s, err)
	}
	if s.Items[0].Kind != "directory" {
		t.Fatal("directory sort")
	}
	next, err := Scan(root, true)
	if err != nil || len(next.Items) != len(names)+1 {
		t.Fatal(next, err)
	}
	encoded, _ := json.Marshal(next)
	var copy Snapshot
	if err := json.Unmarshal(encoded, &copy); err != nil {
		t.Fatal(err)
	}
	for _, name := range names {
		found := false
		for _, item := range copy.Items {
			if bytes.Equal(item.Name, []byte(name)) {
				path, kind, err := copy.Validate(item.ID)
				if err != nil || path != filepath.Join(root, name) || kind != "file" {
					t.Fatal(path, kind, err)
				}
				found = true
				for _, before := range s.Items {
					if bytes.Equal(before.Name, item.Name) && before.ID != item.ID {
						t.Fatal("refresh changed identity")
					}
				}
			}
		}
		if !found {
			t.Fatalf("name lost: %q", name)
		}
	}
	if _, _, err := copy.Validate("stale"); err == nil {
		t.Fatal("stale ID accepted")
	}
}

func TestReplacementAndSymlinkRefusal(t *testing.T) {
	root := t.TempDir()
	file := filepath.Join(root, "file")
	os.WriteFile(file, []byte("old"), 0600)
	s, _ := Scan(root, false)
	id := s.Items[0].ID
	os.Rename(file, file+".old")
	os.WriteFile(file, []byte("new"), 0600)
	if _, _, err := s.Validate(id); err == nil {
		t.Fatal("replaced item accepted")
	}
	os.Remove(file)
	os.Symlink(file+".old", file)
	s, _ = Scan(root, false)
	for _, item := range s.Items {
		if item.Kind == "symlink" {
			if _, _, err := s.Validate(item.ID); err == nil {
				t.Fatal("symlink acted upon")
			}
		}
	}
	os.Rename(root, root+"-old")
	defer os.RemoveAll(root + "-old")
	os.Mkdir(root, 0700)
	if _, _, err := s.Validate(s.Items[0].ID); err == nil {
		t.Fatal("replaced root accepted")
	}
}

func TestDirectoryAdmissionAndInvalidInputs(t *testing.T) {
	root := t.TempDir()
	for i := 0; i <= collection.MaxItems; i++ {
		if err := os.WriteFile(filepath.Join(root, fmt.Sprintf(".%04d", i)), nil, 0600); err != nil {
			t.Fatal(err)
		}
	}
	if _, err := Scan(root, false); err == nil {
		t.Fatal("hidden names bypassed admission budget")
	}
	for _, path := range []string{"relative", root + "\x00bad"} {
		if _, err := Scan(path, true); err == nil {
			t.Fatal("accepted invalid path")
		}
	}
}

func TestInvalidFilenameBytes(t *testing.T) {
	root := t.TempDir()
	name := string([]byte{'b', 0xff})
	if err := os.WriteFile(filepath.Join(root, name), nil, 0600); err != nil {
		if runtime.GOOS == "darwin" {
			t.Skip("host filesystem rejects non-UTF-8 filename; native Linux/BSD gate owns this case")
		}
		t.Fatal(err)
	}
	s, err := Scan(root, false)
	if err != nil || len(s.Items) != 1 || !bytes.Equal(s.Items[0].Name, []byte(name)) {
		t.Fatal(s, err)
	}
	path, _, err := s.Validate(s.Items[0].ID)
	if err != nil || path != filepath.Join(root, name) {
		t.Fatal(path, err)
	}
	data, _ := json.Marshal(s)
	var copy Snapshot
	if json.Unmarshal(data, &copy) != nil || !bytes.Equal(copy.Items[0].Name, []byte(name)) {
		t.Fatal("filename bytes lost on wire")
	}
}

func TestMetadataRevisionChangesWithoutChangingIdentity(t *testing.T) {
	root := t.TempDir()
	path := filepath.Join(root, "file")
	os.WriteFile(path, []byte("before"), 0600)
	before, err := Scan(root, false)
	if err != nil {
		t.Fatal(err)
	}
	os.WriteFile(path, []byte("changed size"), 0600)
	after, err := Scan(root, false)
	if err != nil {
		t.Fatal(err)
	}
	a, b := before.Entries()[0], after.Entries()[0]
	if a.ID != b.ID || a.Revision == b.Revision || len(a.Revision) != 32 {
		t.Fatal("metadata revision changed domain identity or lost observation", a, b)
	}
}
