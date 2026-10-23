// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"github.com/fvwmorg/FvwmPrompt/transport"
	"io"
	"os"
)

func requestID(text string) (string, error) { return transport.RequestID(text) }

func requestOnce(o options, output io.Writer, getenv func(string) string) error {
	reply, err := transport.Request(context.Background(), socketPath(o.socket, getenv, os.Getuid()), o.request)
	if err != nil {
		return err
	}
	decoder := json.NewDecoder(bytes.NewReader(reply))
	decoder.UseNumber()
	var object any
	if err := decoder.Decode(&object); err != nil {
		return err
	}
	encoder := json.NewEncoder(output)
	encoder.SetIndent("", "    ")
	encoder.SetEscapeHTML(false)
	if err := encoder.Encode(object); err != nil {
		return err
	}
	var envelope struct {
		Reply struct {
			Status string `json:"status"`
		} `json:"reply"`
	}
	json.Unmarshal(reply, &envelope)
	if envelope.Reply.Status == "error" {
		return errors.New("MFL request refused; see JSON error code")
	}
	return nil
}
