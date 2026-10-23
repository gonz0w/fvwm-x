// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

// This definition is read-only. Queries expose a fresh names slice, never the
// catalog: capability publication, frame admission and worker routing agree.
var providerOperations = [...]struct {
	name   string
	worker bool
}{
	{"selection", false},
	{"filter", false},
	{"sort", false},
	{"view", false},
	{"properties", false},
	{"navigate", true},
	{"refresh", true},
	{"open", true},
	{"terminal", true},
	{"custom", true},
	{"cancel", false},
	{"reveal", true},
	{"inspect", true},
	{"hidden", true},
	{"watch", false},
	{"present", false},
}

func providerKnownAction(name string) bool {
	for _, operation := range providerOperations {
		if operation.name == name {
			return true
		}
	}
	return false
}

func providerWorkerAction(name string) bool {
	for _, operation := range providerOperations {
		if operation.name == name {
			return operation.worker
		}
	}
	return false
}

func providerActionNames() []string {
	names := make([]string, 0, len(providerOperations))
	for _, operation := range providerOperations {
		names = append(names, operation.name)
	}
	return names
}
