// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"runtime/debug"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/runtimepath"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

var version = "development"

// The core's MAX_READ_ITEMS is the dispatch bound for one Read/PipeRead stream.
const maxReadCommands = 50000

type options struct {
	xdgmenu.ProviderDefaults
	desktop, menuType, list, setMenus, regenerate                  string
	all, dynamic, menuError, verbose, help, showVersion, configure bool
	timeout                                                        time.Duration
	iconScaleQuarters                                              int
	formAction                                                     string
	fields                                                         fieldFlags
	resultToken, formResult                                        string
}
type toggleFlag struct {
	target *bool
	value  bool
}

func (v toggleFlag) String() string   { return "false" }
func (v toggleFlag) IsBoolFlag() bool { return true }
func (v toggleFlag) Set(text string) error {
	if text != "true" && text != "false" {
		return errors.New("invalid boolean option")
	}
	*v.target = v.value == (text == "true")
	return nil
}

func parseOptions(args []string, defaults xdgmenu.ProviderDefaults) (options, error) {
	o := options{ProviderDefaults: defaults, regenerate: "PipeRead `fvwm-menu-desktop`", iconScaleQuarters: 4}
	set := flag.NewFlagSet("fvwm-menu-desktop", flag.ContinueOnError)
	set.SetOutput(io.Discard)
	for _, name := range []string{"h", "help"} {
		set.BoolVar(&o.help, name, false, "show help")
	}
	set.BoolVar(&o.showVersion, "version", false, "show version")
	for _, name := range []string{"v", "verbose"} {
		set.BoolVar(&o.verbose, name, false, "show diagnostics")
	}
	for _, name := range []string{"e", "menu-error"} {
		set.BoolVar(&o.menuError, name, false, "show errors in menu")
	}
	for _, name := range []string{"w", "with-titles"} {
		set.Var(toggleFlag{&o.Titles, true}, name, "show titles")
	}
	set.Var(toggleFlag{&o.Titles, false}, "without-titles", "hide titles")
	set.BoolVar(&o.Icons, "enable-mini-icons", o.Icons, "show icons")
	for _, name := range []string{"s", "size"} {
		set.IntVar(&o.Size, name, o.Size, "icon size")
	}
	for _, name := range []string{"t", "title"} {
		set.StringVar(&o.Title, name, o.Title, "top menu")
	}
	set.StringVar(&o.InstallPrefix, "install-prefix", o.InstallPrefix, "menu directory")
	set.StringVar(&o.desktop, "desktop", "", "desktop filter")
	set.StringVar(&o.menuType, "menu-type", "", "menu type filter")
	set.StringVar(&o.Theme, "theme", o.Theme, "icon theme")
	iconScale := "1"
	set.StringVar(&iconScale, "icon-scale", iconScale, "icon scale (quarter steps from 1 to 4)")
	set.StringVar(&o.IconDir, "mini-icon-dir", o.IconDir, "icon output directory")
	set.StringVar(&o.AppIcon, "app-icon", o.AppIcon, "application fallback icon")
	set.StringVar(&o.DirIcon, "dir-icon", o.DirIcon, "directory fallback icon")
	set.StringVar(&o.InsertInto, "insert-in-menu", o.InsertInto, "existing menu")
	set.StringVar(&o.list, "get-menus", "", "all or desktop")
	set.StringVar(&o.setMenus, "set-menus", "", "explicit menu paths")
	set.BoolVar(&o.all, "all-menus", false, "all discovered menus")
	set.StringVar(&o.IncludeItems, "include-items", o.IncludeItems, "additional controls")
	set.StringVar(&o.regenerate, "regen-cmd", o.regenerate, "regeneration action")
	set.StringVar(&o.TermCmd, "term-cmd", o.TermCmd, "terminal command")
	set.BoolVar(&o.dynamic, "dynamic", false, "dynamic menu")
	set.BoolVar(&o.configure, "configure", false, "native settings form")
	set.StringVar(&o.formAction, "form-action", "", "settings form operation")
	set.Var(&o.fields, "field", "encoded settings field")
	set.StringVar(&o.resultToken, "result-token", "", "private Form result identity")
	set.StringVar(&o.formResult, "form-result", "", "read completed Form result")
	deadline, err := configuredDeadline()
	if err != nil {
		return o, err
	}
	set.DurationVar(&o.timeout, "timeout", deadline, "worker deadline")
	for _, name := range []string{"check-app", "enable-style", "enable-tran-style", "fvwm-icons", "kde_config", "mini-icon-path", "merge-user-menu", "su_gui", "utf8", "wm-icons"} {
		var ignored bool
		set.BoolVar(&ignored, name, false, "obsolete option")
	}
	for _, name := range []string{"check-icons", "check-mini-icon", "destroy-type", "dir", "icon-app", "icon-folder", "icon-style", "icon-title", "icon-toptitle", "icons-path", "lang", "menu-style", "name", "png-icons-path", "submenu-name-prefix", "time-limit", "tran-icons-path", "tran-mini-icons-path", "type", "uniconv-exec", "uniconv", "xterm"} {
		var ignored string
		set.StringVar(&ignored, name, "", "obsolete option")
	}
	if err := set.Parse(legacyShortOptions(args)); err != nil {
		return o, err
	}
	o.iconScaleQuarters, err = xdgmenu.ParseIconScale(iconScale)
	if err != nil {
		return o, err
	}
	if len(set.Args()) != 0 {
		return o, errors.New("unexpected positional arguments")
	}
	if o.list != "" && o.list != "all" && o.list != "desktop" {
		return o, errors.New("--get-menus must be all or desktop")
	}
	if o.IncludeItems != "both" && o.IncludeItems != "none" && o.IncludeItems != "config" && o.IncludeItems != "regenerate" {
		return o, errors.New("--include-items must be both, none, config or regenerate")
	}
	if o.InstallPrefix != "" && !filepath.IsAbs(o.InstallPrefix) {
		return o, errors.New("--install-prefix must be absolute")
	}
	if o.timeout < time.Millisecond || o.timeout > 5*time.Minute {
		return o, errors.New("--timeout must be between 1ms and 5m")
	}
	if o.formAction != "" && o.formAction != "save" && o.formAction != "regenerate" && o.formAction != "export" {
		return o, errors.New("unknown settings form operation")
	}
	if len(o.fields.records) != 0 && o.formAction == "" {
		return o, errors.New("settings fields require a form operation")
	}
	for _, token := range []string{o.resultToken, o.formResult} {
		if token != "" && !validResultToken(token) {
			return o, errors.New("invalid Form result identity")
		}
	}
	if o.resultToken != "" && o.formAction == "" {
		return o, errors.New("Form result requires a settings operation")
	}
	return o, nil
}

func legacyShortOptions(args []string) []string {
	var result []string
	for i := 0; i < len(args); i++ {
		arg := args[i]
		if arg == "--" {
			result = append(result, args[i:]...)
			break
		}
		if len(arg) > 2 && arg[0] == '-' && arg[1] != '-' {
			for n := 1; n < len(arg); n++ {
				result = append(result, "-"+arg[n:n+1])
				if arg[n] == 's' || arg[n] == 't' {
					if n+1 < len(arg) {
						result = append(result, arg[n+1:])
					} else if i+1 < len(args) {
						i++
						result = append(result, args[i])
					}
					break
				}
			}
		} else {
			result = append(result, arg)
			// An option's value is data even when it begins with a dash.
			if optionTakesValue(arg) && i+1 < len(args) {
				i++
				result = append(result, args[i])
			}
		}
	}
	return result
}
func optionTakesValue(option string) bool {
	if strings.Contains(option, "=") {
		return false
	}
	switch strings.TrimLeft(option, "-") {
	case "s", "size", "t", "title", "install-prefix", "desktop", "menu-type", "theme", "icon-scale", "mini-icon-dir", "app-icon", "dir-icon", "insert-in-menu", "get-menus", "set-menus", "include-items", "regen-cmd", "term-cmd", "timeout", "form-action", "field", "result-token", "form-result",
		"check-icons", "check-mini-icon", "destroy-type", "dir", "icon-app", "icon-folder", "icon-style", "icon-title", "icon-toptitle", "icons-path", "lang", "menu-style", "name", "png-icons-path", "submenu-name-prefix", "time-limit", "tran-icons-path", "tran-mini-icons-path", "type", "uniconv-exec", "uniconv", "xterm":
		return true
	}
	return false
}

func environment() map[string]string {
	values := make(map[string]string)
	for _, entry := range os.Environ() {
		key, value, _ := strings.Cut(entry, "=")
		values[key] = value
	}
	return values
}
func userDirectory(env map[string]string) (string, error) {
	path := env["FVWM_USERDIR"]
	if path == "" {
		path = filepath.Join(env["HOME"], ".fvwm")
	}
	if !filepath.IsAbs(path) {
		return "", errors.New("FVWM_USERDIR or HOME must be absolute")
	}
	return filepath.Clean(path), nil
}
func loadDefaults(ctx context.Context, userdir string) (xdgmenu.ProviderDefaults, error) {
	if err := ctx.Err(); err != nil {
		return xdgmenu.ProviderDefaults{}, err
	}
	data, err := xdgsource.ReadSettings(filepath.Join(userdir, ".FvwmForm-XDGMenu-Config"))
	if errors.Is(err, fs.ErrNotExist) {
		err = nil
		data = nil
	}
	if err != nil {
		return xdgmenu.ProviderDefaults{}, err
	}
	records, err := xdgmenu.ParsePreferences(data)
	if err != nil {
		return xdgmenu.ProviderDefaults{}, err
	}
	return xdgmenu.DefaultsFromPreferences(records)
}
func discover(source *xdgsource.Source, paths xdgmenu.SearchPaths, prefix string) ([]xdgmenu.MenuFile, error) {
	var dirs []string
	if prefix != "" {
		dirs = []string{filepath.Clean(prefix)}
	} else {
		for _, dir := range paths.Config {
			dirs = append(dirs, filepath.Join(dir, "menus"))
		}
	}
	var inventory []xdgmenu.MenuDirectory
	for _, dir := range dirs {
		names, err := source.MenuFiles(dir)
		if errors.Is(err, fs.ErrNotExist) {
			continue
		}
		if err != nil {
			return nil, err
		}
		inventory = append(inventory, xdgmenu.MenuDirectory{Path: dir, Names: names})
	}
	return xdgmenu.DiscoverMenus(inventory)
}
func menuLabel(filename string) string {
	words := strings.Split(strings.TrimSuffix(filepath.Base(filename), ".menu"), "-")
	for i, word := range words {
		if word != "" {
			runes := []rune(word)
			words[i] = strings.ToUpper(string(runes[:1])) + string(runes[1:])
		}
	}
	return strings.Join(words, " ")
}
func buildDocuments(ctx context.Context, source *xdgsource.Source, files []xdgmenu.MenuFile, paths xdgmenu.SearchPaths, env map[string]string) ([]xdgmenu.MenuDocument, []string, error) {
	var documents []xdgmenu.MenuDocument
	var nonempty []string
	for _, file := range files {
		if err := ctx.Err(); err != nil {
			return nil, nil, err
		}
		root, err := xdgmenu.MergeMenus(file.Path, paths, source)
		if err != nil {
			return nil, nil, err
		}
		pools, err := source.Prepare(root)
		if err != nil {
			return nil, nil, err
		}
		compiled, err := xdgmenu.BuildMenuPlans(root, pools)
		if err != nil {
			return nil, nil, err
		}
		var allocated *xdgmenu.AllocatedMenu
		if compiled != nil {
			allocated, err = xdgmenu.AllocateMenus(compiled.Plan)
			if err != nil {
				return nil, nil, err
			}
		}
		menu, err := xdgmenu.PresentMenus(compiled, allocated, xdgmenu.PresentationOptions{Locales: xdgmenu.LocalePreferences(env), Desktops: xdgmenu.CurrentDesktops(env["XDG_CURRENT_DESKTOP"]), Directories: source.Directories, Executable: func(name string) bool { _, err := exec.LookPath(name); return err == nil }})
		if err != nil {
			return nil, nil, err
		}
		documents = append(documents, xdgmenu.MenuDocument{Menu: menu, Label: menuLabel(file.Path)})
		if menu != nil && len(menu.Items) != 0 {
			nonempty = append(nonempty, file.Path)
		}
	}
	return documents, nonempty, nil
}

func produce(ctx context.Context, args []string, out, diagnostics io.Writer) error {
	builtins, _ := xdgmenu.DefaultsFromPreferences(nil)
	initial, err := parseOptions(args, builtins)
	if err != nil {
		return err
	}
	if initial.help {
		_, err = io.WriteString(out, "Usage: fvwm-menu-desktop [OPTIONS]\nReads XDG menus and creates native FVWM menus.\nSee fvwm-menu-desktop(1) for supported options.\n")
		return err
	}
	if initial.showVersion {
		_, err = fmt.Fprintln(out, "fvwm-menu-desktop version", version)
		return err
	}
	env := environment()
	userdir, err := userDirectory(env)
	if err != nil {
		return err
	}
	env["FVWM_USERDIR"] = userdir
	defaults, err := loadDefaults(ctx, userdir)
	if err != nil {
		return err
	}
	o, err := parseOptions(args, defaults)
	if err != nil {
		return err
	}
	if o.formResult != "" {
		path := resultPath(userdir, o.formResult)
		data, err := xdgsource.ReadPrivate(path)
		if err != nil {
			return err
		}
		if err = os.Remove(path); err != nil {
			return err
		}
		_, err = out.Write(data)
		return err
	}
	var settings []byte
	if o.formAction != "" {
		var records []xdgmenu.Preference
		records, settings, err = formPreferences(userdir, o.fields)
		if err != nil {
			return err
		}
		o.ProviderDefaults, err = xdgmenu.DefaultsFromPreferences(records)
		if err != nil {
			return err
		}
		if o.formAction == "save" {
			if err = os.MkdirAll(userdir, 0700); err != nil {
				return err
			}
			if err = xdgsource.PublishPrivate(settingsPath(userdir), settings); err != nil {
				return err
			}
			return formOutput(out, userdir, o.resultToken, []byte("Nop\n"))
		}
	}
	paths, err := xdgmenu.SearchDirectories(env["HOME"], env)
	if err != nil {
		return err
	}
	source := xdgsource.New(ctx)
	source.KDEDirectoryLookup = func() ([]string, error) { return kdeDirectories(ctx) }
	if o.configure {
		return renderForm(ctx, source, paths, env, userdir, o, out)
	}
	var files []xdgmenu.MenuFile
	if o.setMenus != "" {
		for _, path := range strings.Fields(o.setMenus) {
			absolute, err := filepath.Abs(path)
			if err != nil {
				return err
			}
			files = append(files, xdgmenu.MenuFile{Path: absolute, Name: filepath.Base(absolute)})
		}
	} else {
		files, err = discover(source, paths, o.InstallPrefix)
		if err != nil {
			return err
		}
		if o.list != "all" {
			files, err = xdgmenu.SelectMenus(files, xdgmenu.MenuSelection{Desktop: o.desktop, MenuType: o.menuType, Prefix: env["XDG_MENU_PREFIX"], UserDirectory: filepath.Join(paths.Config[0], "menus"), Saved: o.Menus, All: o.all})
			if err != nil {
				return err
			}
		}
	}
	if len(files) == 0 {
		return errors.New("no matching desktop menus found")
	}
	documents, nonempty, err := buildDocuments(ctx, source, files, paths, env)
	if err != nil {
		return err
	}
	if o.list != "" {
		_, err = fmt.Fprintln(out, strings.Join(nonempty, " "))
		return err
	}
	if err = os.MkdirAll(userdir, 0700); err != nil {
		return err
	}
	encoder, err := newActionEncoder(userdir, o.Title, o.TermCmd)
	if err != nil {
		return err
	}
	defer encoder.close()
	var commands bytes.Buffer
	renderOptions := xdgmenu.RenderOptions{Title: o.Title, InsertInto: o.InsertInto, Titles: o.Titles, Dynamic: o.dynamic, IncludeItems: o.IncludeItems, Regenerate: o.regenerate, Configure: "PipeRead `fvwm-menu-desktop --configure`", Action: encoder.encode, Icons: o.Icons}
	var icons *iconRenderer
	if o.Icons {
		icons, err = newIconRenderer(source, paths, env, o)
		if err != nil {
			return err
		}
		renderOptions.Icon = icons.resolve
	}
	if err = xdgmenu.RenderMenus(&commands, documents, renderOptions); err != nil {
		return err
	}
	if icons != nil {
		if err = iconStyles(&commands, documents, icons); err != nil {
			return err
		}
		if err = os.MkdirAll(icons.dir, 0700); err != nil {
			return err
		}
		if err = xdgsource.PublishPrivate(filepath.Join(icons.dir, ".theme"), []byte(o.Theme)); err != nil {
			return err
		}
	}
	if bytes.Count(commands.Bytes(), []byte{'\n'}) > maxReadCommands {
		return errors.New("generated menu exceeds the core Read command-count limit")
	}
	if err = encoder.publish(); err != nil {
		return err
	}
	if o.formAction == "export" {
		path, err := outputPath(o.OutputPath, env)
		if err != nil {
			return err
		}
		if err = xdgsource.PublishPrivate(path, commands.Bytes()); err != nil {
			return err
		}
		if strings.ContainsAny(path, "\x00\r\n") {
			return errors.New("invalid menu output path")
		}
		quoted := xdgmenu.FVWMQuote(path)
		commands.Reset()
		fmt.Fprintln(&commands, "Read", quoted)
	}
	if settings != nil {
		if err = xdgsource.PublishPrivate(settingsPath(userdir), settings); err != nil {
			return err
		}
	}
	if o.verbose {
		fmt.Fprintf(diagnostics, "Generated %d menu documents and %d application actions\n", len(documents), len(encoder.catalog.Records))
	}
	return formOutput(out, userdir, o.resultToken, commands.Bytes())
}

func main() {
	debug.SetMemoryLimit(256 * 1024 * 1024)
	args := os.Args[1:]
	if len(args) != 0 && args[0] == "--launch" {
		deadline, err := configuredDeadline()
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(2)
		}
		watchdog := time.AfterFunc(deadline, func() {
			fmt.Fprintln(os.Stderr, "fvwm-menu-desktop: application record acquisition exceeded deadline")
			os.Exit(1)
		})
		if err := launch(args[1:]); err != nil {
			watchdog.Stop()
			fmt.Fprintln(os.Stderr, "fvwm-menu-desktop:", err)
			os.Exit(1)
		}
		return
	}
	if len(args) != 0 && args[0] == "--_worker" {
		args = args[1:]
		timeout, err := workerTimeout(args)
		if err != nil {
			fmt.Fprintln(os.Stderr, "fvwm-menu-desktop:", err)
			os.Exit(2)
		}
		ctx, cancel := context.WithTimeout(context.Background(), timeout)
		defer cancel()
		if err = produce(ctx, args, os.Stdout, os.Stderr); err != nil {
			fmt.Fprintln(os.Stderr, "fvwm-menu-desktop:", err)
			os.Exit(1)
		}
		return
	}
	timeout, err := workerTimeout(args)
	if err != nil {
		fmt.Fprintln(os.Stderr, "fvwm-menu-desktop:", err)
		os.Exit(2)
	}
	executable, err := runtimepath.Executable(os.Args[0])
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()
	if err := supervise(ctx, executable, args, os.Stdout, os.Stderr); err != nil {
		failureMenu(args, os.Stdout)
		fmt.Fprintln(os.Stderr, "fvwm-menu-desktop:", err)
		os.Exit(1)
	}
}
