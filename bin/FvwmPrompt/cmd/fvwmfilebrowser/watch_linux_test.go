//go:build linux

package main

import (
	"encoding/binary"
	"syscall"
	"testing"
)

func TestInotifyLengthOverflowAndLoss(t *testing.T) {
	data := make([]byte, syscall.SizeofInotifyEvent)
	binary.NativeEndian.PutUint32(data[4:8], syscall.IN_Q_OVERFLOW)
	if changed, lost, err := inotifyChanges(data); !changed || lost || err != nil {
		t.Fatal(changed, lost, err)
	}
	binary.NativeEndian.PutUint32(data[4:8], syscall.IN_IGNORED)
	if changed, lost, err := inotifyChanges(data); !changed || !lost || err != nil {
		t.Fatal(changed, lost, err)
	}
	for _, size := range []uint32{1, 0xffffffff} {
		binary.NativeEndian.PutUint32(data[12:16], size)
		if _, _, err := inotifyChanges(data); err == nil {
			t.Fatal("invalid native event length accepted")
		}
	}
	if _, _, err := inotifyChanges(data[:15]); err == nil {
		t.Fatal("short native event accepted")
	}
}
