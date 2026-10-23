// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"io"
	"unicode/utf8"
)

// Fields are bounded literal values. Keep strict object/duplicate admission
// separate from operation schemas so malformed input never reaches native state.
type providerFields map[string]json.RawMessage

func decodeProviderValues(data []byte) (providerFields, error) {
	if len(data) > providerValuesMax || !utf8.Valid(data) {
		return nil, errors.New("invalid action values")
	}
	decoder := json.NewDecoder(bytes.NewReader(data))
	if token, err := decoder.Token(); err != nil || token != json.Delim('{') {
		return nil, errors.New("action values must be an object")
	}
	fields := make(providerFields)
	for decoder.More() {
		key, err := decoder.Token()
		name, ok := key.(string)
		if err != nil || !ok || fields[name] != nil || len(fields) == 2 {
			return nil, errors.New("invalid or duplicate action field")
		}
		var value json.RawMessage
		if err := decoder.Decode(&value); err != nil {
			return nil, err
		}
		fields[name] = value
	}
	if token, err := decoder.Token(); err != nil || token != json.Delim('}') {
		return nil, errors.New("invalid action object")
	}
	var extra any
	if decoder.Decode(&extra) != io.EOF {
		return nil, errors.New("trailing action values")
	}
	return fields, nil
}

func (f providerFields) string(name string, target *string) bool {
	value := f[name]
	return len(value) > 0 && value[0] == '"' && json.Unmarshal(value, target) == nil
}

func (f providerFields) boolean(name string, target *bool) bool {
	value := f[name]
	return (bytes.Equal(value, []byte("true")) || bytes.Equal(value, []byte("false"))) &&
		json.Unmarshal(value, target) == nil
}

func (f providerFields) strings(name string, target *[]string, max int) bool {
	value := f[name]
	return len(value) > 0 && value[0] == '[' && json.Unmarshal(value, target) == nil &&
		len(*target) > 0 && len(*target) <= max
}
