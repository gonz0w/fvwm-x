// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bufio"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"syscall"
)

const sourceLimit = 128 * 1024 * 1024
const logicalLimit = 1024 * 1024
const readLimit = 4096

var continued = regexp.MustCompile(`\\\s*$`)

type conversionError struct {
	message string
	status  int
}

func (e *conversionError) Error() string { return e.message }

func errorStatus(err error) int {
	var e *conversionError
	if errors.As(err, &e) {
		return e.status
	}
	return 1
}

func sourceError(err error) error {
	var errno syscall.Errno
	if errors.As(err, &errno) {
		// Preserve the recorded Perl errno statuses without exposing Go paths.
		text := errno.Error()
		if len(text) != 0 {
			text = strings.ToUpper(text[:1]) + text[1:]
		}
		return &conversionError{"Unable to open source file:  " + text, int(errno)}
	}
	return fmt.Errorf("Unable to open source file:  %w", err)
}

func convertFile(path, context string) (*converter, error) {
	fd, err := syscall.Open(path, syscall.O_RDONLY|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0)
	if err != nil {
		return nil, sourceError(err)
	}
	file := os.NewFile(uintptr(fd), path)
	defer file.Close()
	info, err := file.Stat()
	if err != nil {
		return nil, err
	}
	if !info.Mode().IsRegular() {
		return nil, errors.New("source must be a regular file")
	}
	if info.Size() > sourceLimit {
		return nil, errors.New("source exceeds 128 MiB")
	}
	reader := bufio.NewReaderSize(io.LimitReader(file, sourceLimit+1), 65536)
	c := &converter{context: context}
	total := 0
	var logical []byte
	for {
		physical, err := reader.ReadSlice('\n')
		total += len(physical)
		if total > sourceLimit {
			return nil, errors.New("source exceeds 128 MiB")
		}
		if len(logical)+len(physical) > logicalLimit {
			return nil, errors.New("logical line exceeds 1 MiB")
		}
		logical = append(logical, physical...)
		if err == bufio.ErrBufferFull {
			continue
		}
		if err != nil && err != io.EOF {
			return nil, fmt.Errorf("read source: %w", err)
		}
		// Perl redo returns to chomp after every appended physical line.
		if len(logical) > 0 && logical[len(logical)-1] == '\n' {
			logical = logical[:len(logical)-1]
		}
		if continued.Match(logical) {
			if err == io.EOF {
				return nil, errors.New("unterminated continuation at end of source")
			}
			continue
		}
		if len(physical) != 0 || len(logical) != 0 || err != io.EOF {
			c.dispatch(string(logical))
			if c.err != nil {
				return nil, c.err
			}
		}
		logical = logical[:0]
		if err == io.EOF {
			return c, nil
		}
	}
}

// Link publishes a completed private file only if the destination is absent.
// Relative operations stay anchored to the opened directory inode. The CLI
// performs no concurrent filesystem work while its current directory changes.
func publish(destination string, c *converter) (result error) {
	original, err := os.Open(".")
	if err != nil {
		return err
	}
	defer original.Close()
	fd, err := syscall.Open(filepath.Dir(destination), syscall.O_RDONLY|syscall.O_DIRECTORY|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0)
	if err != nil {
		return err
	}
	parent := os.NewFile(uintptr(fd), filepath.Dir(destination))
	defer parent.Close()
	info, err := parent.Stat()
	if err != nil {
		return err
	}
	if info.Mode().Perm()&0022 != 0 && info.Mode()&os.ModeSticky == 0 {
		return errors.New("destination directory must not be group/world writable without the sticky bit")
	}
	if err := syscall.Fchdir(int(parent.Fd())); err != nil {
		return err
	}
	defer func() {
		if err := syscall.Fchdir(int(original.Fd())); result == nil {
			result = err
		}
	}()
	file, err := os.CreateTemp(".", ".fvwm-convert-2.6-*")
	if err != nil {
		return err
	}
	defer os.Remove(file.Name())
	if err := c.writeOutput(file); err != nil {
		file.Close()
		return err
	}
	if err := file.Close(); err != nil {
		return err
	}
	return os.Link(file.Name(), filepath.Base(destination))
}

type conversionJob struct {
	follow      bool
	out         io.Writer
	diagnostics io.Writer
	cwd         string
	userdir     string
	home        string
	queue       []string
	references  int
	context     string
}

func newJob(follow bool, out, diagnostics io.Writer) (*conversionJob, error) {
	cwd, err := os.Getwd()
	if err != nil {
		return nil, err
	}
	return &conversionJob{follow: follow, out: out, diagnostics: diagnostics,
		cwd: cwd, userdir: os.Getenv("FVWM_USERDIR"), home: os.Getenv("HOME")}, nil
}

func (j *conversionJob) absolute(path string) string {
	if filepath.IsAbs(path) {
		return path
	}
	return filepath.Join(j.cwd, path)
}

func exists(path string) bool {
	_, err := os.Stat(path)
	return err == nil
}

func (j *conversionJob) findRead(name string) string {
	if exists(j.absolute(name)) {
		return name
	}
	if strings.HasPrefix(name, "$.") {
		name = strings.TrimPrefix(strings.TrimPrefix(name, "$."), "/")
	}
	// Concatenation retains the legacy handling of absolute names and unset
	// environment variables; configured commands are never interpolated/run.
	for _, prefix := range []string{j.userdir, j.home + "/.fvwm", j.cwd} {
		path := prefix + "/" + name
		if exists(j.absolute(path)) {
			return path
		}
	}
	return ""
}

func (j *conversionJob) one(source, destination string) error {
	if destination == "" {
		destination = filepath.Join(j.cwd, filepath.Base(source)+".converted")
	}
	if _, err := os.Lstat(j.absolute(destination)); err == nil {
		return &conversionError{"Destination file:  " + destination + " exists", 255}
	} else if !os.IsNotExist(err) {
		return err
	}
	c, err := convertFile(j.absolute(source), j.context)
	if err != nil {
		return err
	}
	if j.references+len(c.reads) > readLimit {
		return errors.New("Read references exceed 4096")
	}
	j.references += len(c.reads)
	for _, reference := range c.reads {
		path := j.findRead(reference.name)
		if path != "" {
			j.queue = append(j.queue, path)
		} else {
			fmt.Fprintf(j.diagnostics, "Unable to follow:  %s\n", reference.line)
		}
	}
	if err := publish(j.absolute(destination), c); err != nil {
		if os.IsExist(err) {
			return &conversionError{"Destination file:  " + destination + " exists", 255}
		}
		return fmt.Errorf("Couldn't open %s: %w", destination, err)
	}
	j.context = c.context
	return nil
}

func (j *conversionJob) process(source, destination string) error {
	if err := j.one(source, destination); err != nil {
		return err
	}
	if !j.follow {
		for _, path := range j.queue {
			fmt.Fprintf(j.out, "The following files were detected, but not processed:\n\n%s", path)
		}
		if len(j.queue) != 0 {
			fmt.Fprintln(j.out)
		}
		return nil
	}
	for i := 0; i < len(j.queue); i++ {
		source := j.queue[i]
		fmt.Fprintf(j.diagnostics, "Following:  Read %s...\n", source)
		if err := j.one(source, ""); err != nil {
			return err
		}
	}
	return nil
}
