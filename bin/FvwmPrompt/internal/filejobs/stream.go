//go:build go1.25

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filejobs

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"io"
	"time"
)

func decodeRecord(data []byte, value any) error {
	d := json.NewDecoder(bytes.NewReader(data))
	d.DisallowUnknownFields()
	if err := d.Decode(value); err != nil {
		return err
	}
	if d.Decode(new(any)) != io.EOF {
		return errors.New("trailing worker data")
	}
	return nil
}

func readRecord(reader *bufio.Reader, limit int) ([]byte, error) {
	var data []byte
	for {
		part, err := reader.ReadSlice('\n')
		if len(part) > limit-len(data) {
			return nil, errors.New("worker record exceeds limit")
		}
		data = append(data, part...)
		if err == nil {
			return data, nil
		}
		if err != bufio.ErrBufferFull {
			return nil, err
		}
	}
}

// Serve owns one private child job. Parent EOF cancels work; production input
// is closable so a finished worker can stop its control reader. A blocked OS
// call or pipe write still needs the parent's process deadline/kill/reap policy.
func Serve(input io.Reader, output io.Writer) error {
	reader := bufio.NewReader(input)
	data, err := readRecord(reader, MaxRequest)
	if err != nil {
		return err
	}
	var request Request
	if err = decodeRecord(data, &request); err != nil {
		return err
	}
	if err = ValidateRequest(request); err != nil {
		return err
	}
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Minute)
	defer cancel()
	if closer, ok := input.(io.Closer); ok {
		defer closer.Close()
	}
	controls := make(chan Control, 1)
	controlErrors := make(chan error, 1)
	go func() {
		defer close(controls)
		for count := 0; count < 4096; count++ {
			line, e := readRecord(reader, MaxRecord)
			if e != nil {
				if e != io.EOF {
					controlErrors <- e
				}
				cancel()
				return
			}
			var c Control
			if e = decodeRecord(line, &c); e != nil {
				controlErrors <- e
				cancel()
				return
			}
			select {
			case controls <- c:
			case <-ctx.Done():
				return
			}
		}
		controlErrors <- errors.New("too many worker controls")
		cancel()
	}()
	err = Run(ctx, request, controls, func(event Event) error {
		data, e := json.Marshal(event)
		if e != nil {
			return e
		}
		if len(data)+1 > MaxRecord {
			return errors.New("worker event exceeds limit")
		}
		data = append(data, '\n')
		for len(data) > 0 {
			n, e := output.Write(data)
			if e != nil {
				return e
			}
			if n <= 0 {
				return io.ErrShortWrite
			}
			data = data[n:]
		}
		return nil
	})
	if err != nil {
		return err
	}
	select {
	case e := <-controlErrors:
		return e
	default:
		return nil
	}
}
