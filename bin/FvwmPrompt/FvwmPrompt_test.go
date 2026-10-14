// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"errors"
	"io"
	"testing"
)

func TestProfileAndReaderCompletionOrdering(t *testing.T) {
	ready := make(chan struct{})
	if err := profileEndError(ready, nil); err == nil {
		t.Fatal("clean EOF before a profile must fail startup")
	}
	close(ready)
	if err := profileEndError(ready, nil); err != nil {
		t.Fatalf("published profile followed by clean EOF: %v", err)
	}
	if err := profileEndError(ready, io.ErrUnexpectedEOF); !errors.Is(err, io.ErrUnexpectedEOF) {
		t.Fatalf("profile must not conceal a later incomplete record: %v", err)
	}
}
