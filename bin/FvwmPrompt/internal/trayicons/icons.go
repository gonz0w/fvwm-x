// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package trayicons is the existing desktop worker's private named-icon mode.
// It reuses the shared finder/presenter with a smaller operation lifetime.
package trayicons

import (
	"bufio"
	"bytes"
	"context"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/iconrender"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

const MaxRequests = 8
const WireLimit = 65536
const AcquisitionBytes = 8 * 1024 * 1024
const PublicationBytes = 2 * 1024 * 1024
const SourcePixels = 256 * 256

type Request struct {
	ID, Generation         uint64
	Size, Quarters         int
	Name, Theme, ThemePath string
}
type Batch struct {
	Directory  string
	Extensions []string
	Requests   []Request
}

func number(token string, maximum uint64) (uint64, error) {
	if token == "" || len(token) > 20 {
		return 0, errors.New("invalid tray icon integer")
	}
	for _, c := range token {
		if c < '0' || c > '9' {
			return 0, errors.New("invalid tray icon integer")
		}
	}
	n, err := strconv.ParseUint(token, 10, 64)
	if err != nil || n == 0 || n > maximum {
		return 0, errors.New("tray icon integer outside bound")
	}
	return n, nil
}
func text(token string, limit int) (string, error) {
	if token == "-" {
		return "", nil
	}
	if token == "" || len(token) > 2*limit || len(token)%2 != 0 {
		return "", errors.New("tray icon text outside bound")
	}
	data, err := hex.DecodeString(token)
	if err != nil || !utf8.Valid(data) || bytes.ContainsAny(data, "\x00\r\n") {
		return "", errors.New("invalid tray icon text")
	}
	return string(data), nil
}

// Decode rejects extra records, ambiguous whitespace and duplicate identities.
// The directory's private ownership is checked separately at execution time.
func Decode(data []byte) (Batch, error) {
	var batch Batch
	if len(data) > WireLimit || !utf8.Valid(data) || bytes.ContainsAny(data, "\x00\r") || len(data) == 0 || data[len(data)-1] != '\n' {
		return batch, errors.New("invalid tray icon frame")
	}
	lines := strings.Split(string(data[:len(data)-1]), "\n")
	if len(lines) < 3 {
		return batch, errors.New("incomplete tray icon frame")
	}
	header := strings.Split(lines[0], " ")
	if len(header) != 5 || header[0] != "FVWM_TRAY_ICONS" || header[1] != "1" {
		return batch, errors.New("unsupported tray icon protocol")
	}
	count, err := number(header[2], MaxRequests)
	if err != nil || len(lines) != int(count)+2 || lines[len(lines)-1] != "END" {
		return batch, errors.New("invalid tray icon record count")
	}
	batch.Directory, err = text(header[3], 1024)
	if err != nil || !filepath.IsAbs(batch.Directory) || filepath.Clean(batch.Directory) != batch.Directory {
		return Batch{}, errors.New("invalid private icon directory")
	}
	for _, extension := range strings.Split(header[4], ",") {
		if extension != "png" && extension != "svg" && extension != "xpm" {
			return Batch{}, errors.New("unsupported tray icon format")
		}
		for _, old := range batch.Extensions {
			if extension == old {
				return Batch{}, errors.New("duplicate tray icon format")
			}
		}
		batch.Extensions = append(batch.Extensions, extension)
	}
	for _, line := range lines[1 : len(lines)-1] {
		fields := strings.Split(line, " ")
		if len(fields) != 7 {
			return Batch{}, errors.New("invalid tray icon record")
		}
		var r Request
		r.ID, err = number(fields[0], ^uint64(0))
		if err != nil {
			return Batch{}, err
		}
		r.Generation, err = number(fields[1], ^uint64(0))
		if err != nil {
			return Batch{}, err
		}
		for _, old := range batch.Requests {
			if old.ID == r.ID {
				return Batch{}, errors.New("duplicate tray icon identity")
			}
		}
		n, e := number(fields[2], 128)
		if e != nil || n < 8 {
			return Batch{}, errors.New("invalid tray icon size")
		}
		r.Size = int(n)
		n, e = number(fields[3], 16)
		if e != nil || n < 4 {
			return Batch{}, errors.New("invalid tray icon scale")
		}
		r.Quarters = int(n)
		r.Name, err = text(fields[4], 256)
		if err != nil || r.Name == "" {
			return Batch{}, errors.New("invalid tray icon name")
		}
		r.Theme, err = text(fields[5], 128)
		if err != nil {
			return Batch{}, err
		}
		if r.Theme == "" {
			r.Theme = "hicolor"
		}
		if !xdgmenu.ValidIconTheme(r.Theme) {
			return Batch{}, errors.New("invalid tray icon theme")
		}
		r.ThemePath, err = text(fields[6], 1024)
		if err != nil {
			return Batch{}, err
		}
		if r.ThemePath != "" && (!filepath.IsAbs(r.ThemePath) || filepath.Clean(r.ThemePath) != r.ThemePath) {
			return Batch{}, errors.New("invalid tray icon extra root")
		}
		batch.Requests = append(batch.Requests, r)
	}
	return batch, nil
}

func privateDirectory(path string) error {
	info, err := os.Lstat(path)
	if err != nil {
		return err
	}
	st, ok := info.Sys().(*syscall.Stat_t)
	if !ok || !info.IsDir() || info.Mode().Perm() != 0700 || st.Uid != uint32(os.Geteuid()) {
		return errors.New("tray icon output directory is not private")
	}
	return nil
}

// Run is one bounded batch, not a resident service. The parent adapter owns the
// hard child deadline; context checks also cover stdin and cooperative lookup.
func Run(ctx context.Context, input io.Reader, output io.Writer, environment map[string]string) error {
	type incoming struct {
		data []byte
		err  error
	}
	ready := make(chan incoming, 1)
	go func() { data, err := io.ReadAll(io.LimitReader(input, WireLimit+1)); ready <- incoming{data, err} }()
	var data []byte
	select {
	case in := <-ready:
		if in.err != nil {
			return in.err
		}
		data = in.data
	case <-ctx.Done():
		return ctx.Err()
	}
	batch, err := Decode(data)
	if err != nil {
		return err
	}
	if err := privateDirectory(batch.Directory); err != nil {
		return err
	}
	home := environment["HOME"]
	paths, err := xdgmenu.SearchDirectories(home, environment)
	if err != nil {
		return err
	}
	source, err := xdgsource.NewLimited(ctx, AcquisitionBytes, 2048)
	if err != nil {
		return err
	}
	var result bytes.Buffer
	fmt.Fprintf(&result, "FVWM_TRAY_ICONS 1 %d\n", len(batch.Requests))
	for _, request := range batch.Requests {
		roots := []string(nil)
		if request.ThemePath != "" {
			roots = append(roots, request.ThemePath)
		}
		// Finder roots differ per application, while acquisition bytes/files
		// remain shared and charged once for this complete batch.
		renderer, err := iconrender.New(source, paths, home, iconrender.Config{
			CacheDir: batch.Directory, Extensions: batch.Extensions,
			CacheBytes: PublicationBytes, OutputBytes: PublicationBytes / MaxRequests,
			SourcePixels: SourcePixels, ExtraRoots: roots,
		})
		if err != nil {
			return err
		}
		path, err := renderer.Resolve(xdgmenu.IconRequest{Name: request.Name, Theme: request.Theme, Size: request.Size, ScaleQuarters: request.Quarters})
		if err != nil {
			return err
		}
		if len(path) > 2048 {
			return errors.New("tray icon result path exceeds bound")
		}
		encoded := "-"
		if path != "" {
			encoded = hex.EncodeToString([]byte(path))
		}
		fmt.Fprintf(&result, "%d %d %s\n", request.ID, request.Generation, encoded)
		if result.Len() > WireLimit-4 {
			return errors.New("tray icon response exceeds bound")
		}
	}
	result.WriteString("END\n")
	writer := bufio.NewWriter(output)
	if _, err := writer.Write(result.Bytes()); err != nil {
		return err
	}
	return writer.Flush()
}
