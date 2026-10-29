// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"context"
	"errors"
	"io"
	"os/exec"
	"syscall"
	"time"
)

type processOutput struct {
	data []byte
	err  error
}

type fileWorkerProcess struct {
	done chan struct{}
	stop chan struct{}
}

// Run/main cannot leave while a separate worker group still has an owner.
// Closing delivery first also releases an owner whose result mailbox is full.
// A blocked kernel teardown may delay this join; it cannot release ownership.
func (b *browser) shutdownWorker() {
	b.pending = nil
	if b.worker != nil {
		close(b.worker.stop)
	}
	if b.cancel != nil {
		b.cancel()
	}
	if b.worker != nil {
		<-b.worker.done
		b.worker = nil
	}
	b.active, b.activeSerial, b.cancel = false, 0, nil
}

// One owner serializes read/cancel retirement and Wait. The unreaped leader
// reserves its PID/PGID until every signal is finished; no cancellation callback
// can later signal a recycled group. Applications escape this group at Start.
func ownedProcessOutput(ctx context.Context, command *exec.Cmd, limit int64, group bool) ([]byte, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	if group {
		command.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	}
	command.WaitDelay = time.Second
	stdout, err := command.StdoutPipe()
	if err != nil {
		return nil, err
	}
	if err = command.Start(); err != nil {
		stdout.Close()
		return nil, err
	}
	read := make(chan processOutput, 1)
	go func() {
		data, err := io.ReadAll(io.LimitReader(stdout, limit+1))
		if int64(len(data)) > limit {
			err = errors.New("process output exceeds limit")
		}
		read <- processOutput{data, err}
	}()
	var output processOutput
	finished := false
	select {
	case output = <-read:
		finished = true
	case <-ctx.Done():
	}
	// This always precedes Wait, including successful EOF and malformed output.
	// The classifier inherits its worker's group; inner timeout kills its leader
	// here and parent retirement also removes any remaining classifier children.
	if group {
		syscall.Kill(-command.Process.Pid, syscall.SIGKILL)
	} else {
		command.Process.Kill()
	}
	stdout.Close()
	waitErr := command.Wait()
	if !finished {
		output = <-read
	}
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	if output.err != nil {
		return output.data, output.err
	}
	return output.data, waitErr
}

func detachApplication(command *exec.Cmd) {
	command.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
}
