// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package configsource

import (
	"strings"
	"testing"
)

const fileSource = `{"version":1,"kind":"file","path":"/tmp/a$\"%&\\配置","first":2,"last":4,"include":[{"path":"/tmp/root","first":7,"last":9}],"reason":""}`

func TestDecodeStrictSchema(t *testing.T) {
	source, err := Decode([]byte(fileSource))
	if err != nil || source.Site.First != 2 || source.Site.Last != 4 || len(source.Include) != 1 {
		t.Fatalf("valid source: %+v %v", source, err)
	}
	for _, kind := range []string{"generated", "invocation", "unavailable"} {
		if _, err := Decode([]byte(`{"version":1,"kind":"` + kind + `"}`)); err != nil {
			t.Fatal(kind, err)
		}
	}
	for _, bad := range []string{
		`null`, `[]`, `{}`, `{"version":1,"kind":null}`, `{"version":1.0,"kind":"file"}`,
		`{"version":1,"kind":"file","path":"/a","first":1,"last":1,"extra":true}`,
		strings.Replace(fileSource, `"version":1`, `"version":1,"version":1`, 1),
		strings.Replace(fileSource, `"first":2`, `"first":null`, 1),
		strings.Replace(fileSource, `"first":2`, `"first":2.0`, 1),
		strings.Replace(fileSource, `"first":2`, `"first":2e0`, 1),
		strings.Replace(fileSource, `"first":2`, `"first":-1`, 1),
		strings.Replace(fileSource, `"first":2`, `"first":0`, 1),
		strings.Replace(fileSource, `"first":2`, `"first":4294967296`, 1),
		strings.Replace(fileSource, `"last":4`, `"last":1`, 1),
		strings.Replace(fileSource, `"include":[`, `"include":null,"ignored":[`, 1),
		strings.Replace(fileSource, `"reason":""`, `"reason":null`, 1),
		`{"version":1,"kind":"file","path":"/a\n","first":1,"last":1}`,
		`{"version":1,"kind":"file","path":"/a\ud800","first":1,"last":1}`,
		`{"version":1,"kind":"file","path":"/a\udc00","first":1,"last":1}`,
		`{"version":1,"kind":"file","path":"relative","first":1,"last":1}`,
		`{"version":1,"kind":"generated","path":"/a"}`,
		`{"version":1,"kind":"unavailable","include":[{"path":"/a","first":1,"last":1}]}`,
		`{"version":1,"kind":"unavailable","reason":"` + strings.Repeat("a", Limit) + `"}`,
		string([]byte{'{', 0xff, '}'}),
	} {
		if _, err := Decode([]byte(bad)); err == nil {
			t.Fatalf("accepted: %s", bad)
		}
	}
	for _, data := range []string{
		`{"version":1,"kind":"file","path":"/a\ud83d\ude00","first":4294967295,"last":4294967295}`,
		`{"version":1,"kind":"file","path":"/a","first":1,"last":1,"include":[]}`,
	} {
		if _, err := Decode([]byte(data)); err != nil {
			t.Fatal("valid Unicode/span rejected", err)
		}
	}
	base := `{"version":1,"kind":"unavailable","reason":""}`
	atLimit := strings.Replace(base, `"reason":""`, `"reason":"`+strings.Repeat("x", Limit-len(base))+`"`, 1)
	if len(atLimit) != Limit {
		t.Fatal("bad exact bound fixture")
	}
	if _, err := Decode([]byte(atLimit)); err != nil {
		t.Fatal("exact source byte bound rejected", err)
	}
	if _, err := Decode([]byte(atLimit + " ")); err == nil {
		t.Fatal("oversize source admitted")
	}
	parent := `{"path":"/a","first":1,"last":1}`
	for _, n := range []int{16, 17} {
		text := `{"version":1,"kind":"file","path":"/a","first":1,"last":1,"include":[` + strings.TrimSuffix(strings.Repeat(parent+",", n), ",") + `]}`
		if _, err := Decode([]byte(text)); (err == nil) != (n == 16) {
			t.Fatalf("include bound %d: %v", n, err)
		}
	}
}
