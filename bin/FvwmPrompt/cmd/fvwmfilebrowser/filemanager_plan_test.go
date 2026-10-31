package main

import (
	"io"
	"strings"
	"testing"
)

func TestFileManagerPlannerGroupsBytePathsInFirstOrder(t *testing.T) {
	groups, err := planFileManager(fileManagerFrame{Method: "ShowItemProperties", URIs: []string{
		"file:///b/%FF", "file://localhost/a/one", "file:///b//%FF", "file:///b/two%2520",
	}})
	if err != nil || len(groups) != 2 || !groups[0].properties || string(groups[0].targets[0]) != "/b/\xff" || string(groups[0].targets[1]) != "/b/two%20" || string(groups[1].targets[0]) != "/a/one" {
		t.Fatal(groups, err)
	}
	groups, err = planFileManager(fileManagerFrame{Method: "ShowFolders", URIs: []string{"file:///b//", "file:///a", "file://localhost/b"}})
	if err != nil || len(groups) != 2 || groups[0].dir != "/b" || groups[1].dir != "/a" {
		t.Fatal(groups, err)
	}
}

func TestFileManagerPlannerRejectsWholeInvalidRequest(t *testing.T) {
	for _, uri := range []string{"/local/path", "file:/local/path", "file://remote/path", "file:///a/../b", "file:///a/%2e/b", "file:///a?x", "file:///a#x", "file:///a%00b", "file:///"} {
		groups, err := planFileManager(fileManagerFrame{Method: "ShowItems", URIs: []string{"file:///good/item", uri}})
		if err == nil || groups != nil {
			t.Fatal("partial or invalid plan admitted", uri, groups, err)
		}
	}
	for _, frame := range []fileManagerFrame{
		{Method: "bad", URIs: []string{"file:///a"}},
		{Method: "ShowFolders"},
		{Method: "ShowFolders", URIs: []string{"file:///a"}, StartupID: "line\nbreak"},
		{Method: "ShowFolders", URIs: []string{"file:///a"}, StartupID: strings.Repeat("a", 513)},
		{Method: "ShowFolders", URIs: []string{"file:///a"}, StartupID: "\xff"},
		{Method: "ShowFolders", URIs: []string{"file:///a", "file:///a", "file:///a", "file:///a", "file:///a", "file:///a", "file:///a", "file:///a", "file:///a"}},
		{Method: "ShowFolders", URIs: []string{"file:///" + strings.Repeat("a", 4090), "file:///" + strings.Repeat("b", 4090), "file:///" + strings.Repeat("c", 4090), "file:///" + strings.Repeat("d", 4090)}},
	} {
		if groups, err := planFileManager(frame); err == nil || groups != nil {
			t.Fatal(frame.Method, groups, err)
		}
	}
	if _, err := planFileManager(fileManagerFrame{Method: "ShowFolders", URIs: []string{"file:///"}}); err != nil {
		t.Fatal("root folder should be allowed", err)
	}
}

func TestFileManagerCLIIsExplicitAndExclusive(t *testing.T) {
	for _, args := range [][]string{{"--filemanager-service", "--reuse"}, {"--filemanager-service", "--dir", "/tmp"}, {"--filemanager-service", "--hidden=false"}, {"--filemanager-service", "/tmp"}, {"--filemanager-service", "--reveal", "/tmp/a"}, {"--_filemanager-child"}} {
		if _, err := parseOptions(args, "/home", io.Discard); err == nil {
			t.Fatal("invalid service options", args)
		}
	}
	o, err := parseOptions([]string{"--filemanager-service", "--socket", "/tmp/socket"}, "/home", io.Discard)
	if err != nil || !o.fileManagerService || o.socket != "/tmp/socket" {
		t.Fatal(o, err)
	}
}
