package main

import (
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

func watchPolicyFixture() (*directoryWatch, *filemodel.Snapshot, time.Time) {
	snapshot := &filemodel.Snapshot{Path: []byte("/folder"), Root: filemodel.Identity{Device: 1, Inode: 2}}
	request := &watchRequest{snapshot.Path, snapshot.Root}
	w := &directoryWatch{enabled: true, active: true, serial: 3, desired: request,
		current: request, cancel: func() {}}
	now := time.Unix(100, 0)
	w.event(watchEvent{3, "ready"}, now)
	return w, snapshot, now
}

func TestWatchCoalescesBurstAndDefersToUserWork(t *testing.T) {
	w, _, now := watchPolicyFixture()
	if w.takeRefresh(now, false) == nil {
		t.Fatal("ready did not reconcile scan/registration gap")
	}
	for i := 0; i < 1000; i++ {
		w.event(watchEvent{3, "changed"}, now.Add(time.Millisecond))
	}
	if w.takeRefresh(now.Add(500*time.Millisecond), false) != nil {
		t.Fatal("background refresh bypassed one-second bound")
	}
	if w.takeRefresh(now.Add(time.Second), true) != nil || !w.dirty {
		t.Fatal("background refresh competed with user work or lost invalidation")
	}
	if w.takeRefresh(now.Add(time.Second), false) == nil ||
		w.takeRefresh(now.Add(time.Second), false) != nil {
		t.Fatal("burst did not collapse into one snapshot request")
	}
	w.tick(now.Add(30*time.Second), "")
	if !w.dirty || w.takeRefresh(now.Add(30*time.Second), true) != nil ||
		w.takeRefresh(now.Add(30*time.Second), false) == nil {
		t.Fatal("periodic reconciliation lost or bypassed worker ownership")
	}
}

func TestWatchLossAndStaleEventsCannotOwnReplacement(t *testing.T) {
	w, snapshot, now := watchPolicyFixture()
	cancelled := 0
	w.cancel = func() { cancelled++ }
	w.event(watchEvent{3, "lost"}, now)
	w.event(watchEvent{3, "ready"}, now)
	if !w.failed || w.ready || w.dirty || cancelled != 1 {
		t.Fatal("late ready revived a lost observer", w)
	}
	w.desire(snapshot, true)
	if w.failed || !w.active {
		t.Fatal("manual retry freed the process before reap")
	}
	other := *snapshot
	other.Path = []byte("/other")
	w.desire(&other, false)
	w.event(watchEvent{3, "ready"}, now)
	w.event(watchEvent{3, "changed"}, now)
	w.event(watchEvent{2, "done"}, now)
	if w.ready || w.dirty || !w.active || string(w.desired.Path) != "/other" {
		t.Fatal("old observer altered replacement state")
	}
	w.event(watchEvent{3, "done"}, now)
	if w.active || w.current != nil || w.cancel != nil {
		t.Fatal("reap did not release observer slot")
	}
}

func TestWatchLeaseAndStartupDeadline(t *testing.T) {
	w, snapshot, now := watchPolicyFixture()
	w.event(watchEvent{3, "expired"}, now)
	if w.ready || w.failed || !w.active || w.desired == nil {
		t.Fatal("lease expiry lost desired watch or released process early")
	}
	w.event(watchEvent{3, "done"}, now)
	if w.active || !sameWatch(w.desired, &watchRequest{snapshot.Path, snapshot.Root}) {
		t.Fatal("lease completion cannot re-arm desired directory")
	}
	w, _, now = watchPolicyFixture()
	w.ready = false
	w.armDeadline = now
	if !w.tick(now, "") || !w.failed || w.takeRefresh(now, false) != nil {
		t.Fatal("startup deadline did not select manual fallback")
	}
}

func TestBrowserWatchOptionsToggleAndCancel(t *testing.T) {
	for _, value := range []string{"", "off", "auto", "recursive"} {
		o := options{dir: "/", watch: value}
		err := prepareOptions(&o, true, nil, nil)
		if (err == nil) != (value != "recursive") {
			t.Fatal(value, err)
		}
	}
	b := browser{instance: "test", form: 1}
	if _, err := b.action(testControl(&b, "watch", "-", []byte{0, 0})); err != nil || !b.watch.enabled {
		t.Fatal("watch toggle", err)
	}
	if _, err := b.action(testControl(&b, "cancel", "-", []byte{0, 0})); err != nil || b.watch.enabled || b.watch.desired != nil {
		t.Fatal("cancel did not disable background work", err)
	}
}
