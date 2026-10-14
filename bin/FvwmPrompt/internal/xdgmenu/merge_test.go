// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"errors"
	"io/fs"
	"reflect"
	"strings"
	"testing"
)

type memoryMenus struct {
	files  map[string]string
	dirs   map[string][]string
	legacy *Node
	err    error
	reads  int
}

func (m *memoryMenus) ReadMenu(path string) ([]byte, error) {
	m.reads++
	if m.err != nil {
		return nil, m.err
	}
	if data, ok := m.files[path]; ok {
		return []byte(data), nil
	}
	return nil, fs.ErrNotExist
}
func (m *memoryMenus) MenuFiles(path string) ([]string, error) {
	if m.err != nil {
		return nil, m.err
	}
	if names, ok := m.dirs[path]; ok {
		return names, nil
	}
	return nil, fs.ErrNotExist
}
func (m *memoryMenus) LegacyMenu(path, prefix string) (*Node, error) {
	if m.err != nil {
		return nil, m.err
	}
	if m.legacy != nil {
		return m.legacy, nil
	}
	return nil, fs.ErrNotExist
}
func childTexts(node *Node, tag string) []string {
	var result []string
	for _, child := range node.Children {
		if child.Tag == tag {
			result = append(result, child.Text)
		}
	}
	return result
}
func childMenu(node *Node, name string) *Node {
	for _, child := range node.Children {
		if child.Tag == "Menu" {
			value, _ := menuName(child)
			if value == name {
				return child
			}
		}
	}
	return nil
}
func mergeFixture(t *testing.T, xml string, files map[string]string) *Node {
	t.Helper()
	if files == nil {
		files = make(map[string]string)
	}
	files["/config/menus/applications.menu"] = "<Menu><Name>Root</Name>" + xml + "</Menu>"
	got, err := MergeMenus("/config/menus/applications.menu", SearchPaths{}, &memoryMenus{files: files})
	if err != nil {
		t.Fatal(err)
	}
	return got
}

func TestMenuMergeOrder(t *testing.T) {
	t.Run("relative-origin-and-rule-order", func(t *testing.T) {
		root := mergeFixture(t, `<Include><All/></Include><MergeFile>sub/fragment.menu</MergeFile><Exclude><All/></Exclude>`, map[string]string{
			"/config/menus/sub/fragment.menu": `<Menu><Name>Ignored</Name><AppDir>../apps</AppDir><DirectoryDir>dirs</DirectoryDir><Include><Filename>one.desktop</Filename></Include></Menu>`,
		})
		if !reflect.DeepEqual(childTexts(root, "AppDir"), []string{"/config/menus/apps"}) || !reflect.DeepEqual(childTexts(root, "DirectoryDir"), []string{"/config/menus/sub/dirs"}) {
			t.Fatal(root)
		}
		var tags []string
		for _, n := range root.Children {
			tags = append(tags, n.Tag)
		}
		if !reflect.DeepEqual(tags, []string{"Name", "Include", "AppDir", "DirectoryDir", "Include", "Exclude"}) {
			t.Fatal(tags)
		}
	})
	t.Run("last-duplicate-merge", func(t *testing.T) {
		root := mergeFixture(t, `<MergeFile>f.menu</MergeFile><Exclude><All/></Exclude><MergeFile>./f.menu</MergeFile>`, map[string]string{
			"/config/menus/f.menu": `<Menu><Name>Ignored</Name><Include><All/></Include></Menu>`,
		})
		if len(root.Children) != 3 || root.Children[1].Tag != "Exclude" || root.Children[2].Tag != "Include" {
			t.Fatal(root)
		}
	})
	t.Run("same-menu-kept-last", func(t *testing.T) {
		root := mergeFixture(t, `<Menu><Name>Same</Name><Include><All/></Include></Menu><Menu><Name>Other</Name></Menu><Menu><Name>Same</Name><Exclude><All/></Exclude></Menu>`, nil)
		if len(root.Children) != 3 || childMenu(root, "Other") != root.Children[1] || childMenu(root, "Same") != root.Children[2] {
			t.Fatal(root)
		}
		n := root.Children[2]
		if len(n.Children) != 3 || n.Children[1].Tag != "Include" || n.Children[2].Tag != "Exclude" {
			t.Fatal(n)
		}
	})
	t.Run("last-path-duplicates-directory-fallbacks", func(t *testing.T) {
		root := mergeFixture(t, `<AppDir>/one</AppDir><AppDir>/two</AppDir><AppDir>/one</AppDir><Directory>a.directory</Directory><Directory>b.directory</Directory><Directory>a.directory</Directory>`, nil)
		if !reflect.DeepEqual(childTexts(root, "AppDir"), []string{"/two", "/one"}) || !reflect.DeepEqual(childTexts(root, "Directory"), []string{"b.directory", "a.directory"}) {
			t.Fatal(root)
		}
	})
	t.Run("unknown-and-layout-preserved", func(t *testing.T) {
		root := mergeFixture(t, `<Extension value="x"><Future>y</Future></Extension><Layout><Merge type="files"/></Layout>`, nil)
		if root.Children[1].Attr["value"] != "x" || root.Children[1].Children[0].Text != "y" || root.Children[2].Children[0].Attr["type"] != "files" {
			t.Fatal(root)
		}
	})
}

func TestMenuDefaultAndParentMerges(t *testing.T) {
	paths := SearchPaths{Config: []string{"/user", "/missing", "/system"}, Data: []string{"/data/user", "/data/system"}}
	for _, filename := range []string{"applications", "gnome-applications", "preferences"} {
		t.Run(filename, func(t *testing.T) {
			base := filename
			if strings.HasSuffix(base, "applications") {
				base = "applications"
			}
			m := &memoryMenus{files: map[string]string{
				"/user/menus/" + filename + ".menu":        `<Menu><Name>Root</Name><DefaultAppDirs/><DefaultDirectoryDirs/><MergeFile type="parent">ignored.menu</MergeFile><DefaultMergeDirs/></Menu>`,
				"/system/menus/" + filename + ".menu":      `<Menu><Name>System</Name><Directory>parent.directory</Directory></Menu>`,
				"/system/menus/" + base + "-merged/a.menu": `<Menu><Name>A</Name><Directory>system.directory</Directory></Menu>`,
				"/user/menus/" + base + "-merged/z.menu":   `<Menu><Name>Z</Name><Directory>user.directory</Directory></Menu>`,
			}, dirs: map[string][]string{
				"/system/menus/" + base + "-merged": {"a.menu", "ignored.txt"},
				"/user/menus/" + base + "-merged":   {"z.menu"},
			}}
			got, err := MergeMenus("/user/menus/"+filename+".menu", paths, m)
			if err != nil {
				t.Fatal(err)
			}
			if !reflect.DeepEqual(childTexts(got, "AppDir"), []string{"/data/system/applications", "/data/user/applications"}) || !reflect.DeepEqual(childTexts(got, "DirectoryDir"), []string{"/data/system/desktop-directories", "/data/user/desktop-directories"}) {
				t.Fatal(got)
			}
			if !reflect.DeepEqual(childTexts(got, "Directory"), []string{"parent.directory", "system.directory", "user.directory"}) {
				t.Fatal(got)
			}
		})
	}
	t.Run("parent-outside-search-is-absent", func(t *testing.T) {
		got, err := MergeMenus("/outside/a.menu", paths, &memoryMenus{files: map[string]string{"/outside/a.menu": `<Menu><Name>Root</Name><MergeFile type="parent">/secret.menu</MergeFile></Menu>`}})
		if err != nil || len(got.Children) != 1 {
			t.Fatal(got, err)
		}
	})
	t.Run("missing-optional-merges", func(t *testing.T) {
		got := mergeFixture(t, `<MergeFile>missing.menu</MergeFile><MergeDir>missing</MergeDir><LegacyDir>old</LegacyDir>`, nil)
		if len(got.Children) != 1 {
			t.Fatal(got)
		}
	})
	t.Run("directory-lexical-order-owned", func(t *testing.T) {
		names := []string{"z.menu", "a.menu"}
		m := &memoryMenus{files: map[string]string{
			"/r.menu":   `<Menu><Name>Root</Name><MergeDir>/d</MergeDir></Menu>`,
			"/d/a.menu": `<Menu><Name>A</Name><Directory>a.directory</Directory></Menu>`,
			"/d/z.menu": `<Menu><Name>Z</Name><Directory>z.directory</Directory></Menu>`,
		}, dirs: map[string][]string{"/d": names}}
		got, err := MergeMenus("/r.menu", SearchPaths{}, m)
		if err != nil || !reflect.DeepEqual(childTexts(got, "Directory"), []string{"a.directory", "z.directory"}) || names[0] != "z.menu" {
			t.Fatal(got, err, names)
		}
	})
}

func TestMenuMovesAndDeletion(t *testing.T) {
	cases := []struct {
		name, xml string
		check     func(*Node) bool
	}{
		{"rename", `<Menu><Name>A</Name><Directory>a.directory</Directory></Menu><Move><Old>A</Old><New>B</New></Move>`, func(r *Node) bool { return childMenu(r, "A") == nil && childMenu(r, "B") != nil }},
		{"create-path", `<Menu><Name>A</Name></Menu><Move><Old>A</Old><New>B/C</New></Move>`, func(r *Node) bool { return childMenu(r, "A") == nil && childMenu(childMenu(r, "B"), "C") != nil }},
		{"existing-target-prepend", `<Menu><Name>A</Name><Include><All/></Include></Menu><Menu><Name>B</Name><Exclude><All/></Exclude></Menu><Move><Old>A</Old><New>B</New></Move>`, func(r *Node) bool {
			n := childMenu(r, "B")
			return len(n.Children) == 3 && n.Children[1].Tag == "Include" && n.Children[2].Tag == "Exclude"
		}},
		{"ordered-pairs", `<Menu><Name>A</Name></Menu><Move><Old>A</Old><New>B</New><Old>B</Old><New>C</New></Move>`, func(r *Node) bool {
			return childMenu(r, "A") == nil && childMenu(r, "B") == nil && childMenu(r, "C") != nil
		}},
		{"missing-source", `<Move><Old>Absent</Old><New>Never/Created</New></Move>`, func(r *Node) bool { return childMenu(r, "Never") == nil }},
		{"self-noop", `<Menu><Name>A</Name></Menu><Move><Old>A</Old><New>A</New></Move>`, func(r *Node) bool { return childMenu(r, "A") != nil }},
		{"deepest-first", `<Menu><Name>A</Name><Menu><Name>B</Name></Menu><Move><Old>B</Old><New>C</New></Move></Menu><Move><Old>A/C</Old><New>D</New></Move>`, func(r *Node) bool { return childMenu(childMenu(r, "A"), "C") == nil && childMenu(r, "D") != nil }},
		{"destination-ancestor", `<Menu><Name>A</Name><Menu><Name>B</Name><Directory>b.directory</Directory></Menu></Menu><Move><Old>A/B</Old><New>A</New></Move>`, func(r *Node) bool {
			return childMenu(childMenu(r, "A"), "B") == nil && reflect.DeepEqual(childTexts(childMenu(r, "A"), "Directory"), []string{"b.directory"})
		}},
		{"delete-subtree", `<Menu><Name>A</Name><Deleted/><Menu><Name>B</Name><NotDeleted/></Menu></Menu>`, func(r *Node) bool { return childMenu(r, "A") == nil }},
		{"last-notdeleted", `<Menu><Name>A</Name><Deleted/><NotDeleted/></Menu>`, func(r *Node) bool { return childMenu(r, "A") != nil }},
		{"last-deleted", `<Menu><Name>A</Name><NotDeleted/><Deleted/></Menu>`, func(r *Node) bool { return childMenu(r, "A") == nil }},
		{"root-deleted", `<Deleted/>`, func(r *Node) bool { return r == nil }},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			got := mergeFixture(t, c.xml, nil)
			if !c.check(got) {
				t.Fatal(got)
			}
		})
	}
}

func TestMenuMergeFailureAndOwnership(t *testing.T) {
	t.Run("file-cycle", func(t *testing.T) {
		got := mergeFixture(t, `<MergeFile>b.menu</MergeFile>`, map[string]string{"/config/menus/b.menu": `<Menu><Name>B</Name><MergeFile>applications.menu</MergeFile><Directory>b.directory</Directory></Menu>`})
		if !reflect.DeepEqual(childTexts(got, "Directory"), []string{"b.directory"}) {
			t.Fatal(got)
		}
	})
	t.Run("acquisition-error", func(t *testing.T) {
		failure := errors.New("read failed")
		got, err := MergeMenus("/r.menu", SearchPaths{}, &memoryMenus{err: failure})
		if got != nil || !errors.Is(err, failure) {
			t.Fatal(got, err)
		}
	})
	t.Run("legacy-tree-not-mutated", func(t *testing.T) {
		legacy := &Node{Tag: "Menu", Children: []*Node{{Tag: "Name", Text: "Legacy"}, {Tag: "AppDir", Text: "apps"}}}
		m := &memoryMenus{files: map[string]string{"/r.menu": `<Menu><Name>Root</Name><LegacyDir>/old</LegacyDir></Menu>`}, legacy: legacy}
		got, err := MergeMenus("/r.menu", SearchPaths{}, m)
		if err != nil || legacy.Children[1].Text != "apps" || got.Children[1].Text != "/old/apps" {
			t.Fatal(got, legacy, err)
		}
		got.Children[1].Text = "changed"
		if legacy.Children[1].Text != "apps" {
			t.Fatal("aliased source")
		}
	})
	for _, xml := range []string{
		`<Name>Duplicate</Name>`, `<Menu><Name>A/B</Name></Menu>`, `<MergeFile type="unknown">f.menu</MergeFile>`,
		`<Move><Old>A</Old></Move>`, `<Move><New>A</New><Old>B</Old></Move>`, `<Move><Old>A</Old><New>/B</New></Move>`,
		`<Menu><Name>A</Name></Menu><Move><Old>A</Old><New>A/B</New></Move>`,
	} {
		t.Run(xml, func(t *testing.T) {
			m := &memoryMenus{files: map[string]string{"/r.menu": "<Menu><Name>Root</Name>" + xml + "</Menu>"}}
			got, err := MergeMenus("/r.menu", SearchPaths{}, m)
			if err == nil || got != nil {
				t.Fatal(got, err)
			}
		})
	}
	t.Run("bad-directory-inventory", func(t *testing.T) {
		m := &memoryMenus{files: map[string]string{"/r.menu": `<Menu><Name>Root</Name><MergeDir>/d</MergeDir></Menu>`}, dirs: map[string][]string{"/d": {"../escape.menu"}}}
		got, err := MergeMenus("/r.menu", SearchPaths{}, m)
		if got != nil || err == nil {
			t.Fatal(got, err)
		}
	})
	t.Run("repeated-expansion-bounded", func(t *testing.T) {
		m := &memoryMenus{files: map[string]string{}}
		for i := 0; i < 20; i++ {
			name := strings.Repeat("a", i+1)
			next := strings.Repeat("a", i+2)
			m.files["/"+name+".menu"] = `<Menu><Name>` + name + `</Name><Menu><Name>One</Name><MergeFile>` + next + `.menu</MergeFile></Menu><Menu><Name>Two</Name><MergeFile>` + next + `.menu</MergeFile></Menu></Menu>`
		}
		got, err := MergeMenus("/a.menu", SearchPaths{}, m)
		if got != nil || err == nil || m.reads > MaxAllocationWork {
			t.Fatal(got, err, m.reads)
		}
	})
}
