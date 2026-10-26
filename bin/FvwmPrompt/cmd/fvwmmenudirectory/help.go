// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"fmt"
	"io"
)

func help(out io.Writer, program string, o options) {
	fmt.Fprintf(out, "A utility that builds directory listing for fvwm.\n\n")
	fmt.Fprintf(out, "Usage: %s [OPTIONS]\n", program)
	fmt.Fprintf(out, "Options:\n")
	fmt.Fprintf(out, "\t--help             show this help and exit\n")
	fmt.Fprintf(out, "\t--version          show the version and exit\n")
	fmt.Fprintf(out, "\t--name=NAME        menu name,  default is '%s'\n", o.name)
	fmt.Fprintf(out, "\t--title=NAME       menu title, default is '%s'\n", o.title)
	fmt.Fprintf(out, "\t--item=NAME        menu item format, default is '%s'\n", o.item)
	fmt.Fprintf(out, "\t--icon-title=XPM   menu title icon, default is none\n")
	fmt.Fprintf(out, "\t--icon-dir=XPM     menu dir   icon, default is none\n")
	fmt.Fprintf(out, "\t--icon-file=XPM    menu file  icon, default is none\n")
	fmt.Fprintf(out, "\t--icon-app=XPM     menu +x    icon, default is none\n")
	fmt.Fprintf(out, "\t--icon-theme=NAME  resolve named icons in an isolated worker\n")
	fmt.Fprintf(out, "\t--icon-size=NUM    nominal icon size (1..4096), default 24\n")
	fmt.Fprintf(out, "\t--icon-scale=NUM   scale (1..4 in quarter steps), default 1\n")
	fmt.Fprintf(out, "\t--mini-icon-dir=DIR private resolved-icon cache\n")
	fmt.Fprintf(out, "\t--wm-icons         define icon names to use with wm-icons\n")
	fmt.Fprintf(out, "\t--dir=NAME         starting dir, default is '%s'\n", o.dir)
	fmt.Fprintf(out, "\t--order=NUM        NUM (-6 .. 6), default is 5\n")
	fmt.Fprintf(out, "\t\t1 - do not sort,  2 - dirs first, 3 - files first\n")
	fmt.Fprintf(out, "\t\t4 - sort by name, 5 - dirs first, 6 - files first\n")
	fmt.Fprintf(out, "\t\tNegative number represents reverse order.\n")
	fmt.Fprintf(out, "\t--all              show hidden files, default is no\n")
	fmt.Fprintf(out, "\t--links            follow linked dirs, default is no\n")
	fmt.Fprintf(out, "\t--xterm=CMD        xterm call, default is '%s'\n", o.xterm)
	fmt.Fprintf(out, "\t--exec-title=CMD   title exec command, default is '%s'\n", o.execTitle)
	fmt.Fprintf(out, "\t--exec-file=CMD    file exec command, default is '%s'\n", o.execFile)
	fmt.Fprintf(out, "\t--exec-app[=CMD]   +x files exec command, default is '-'\n")
	fmt.Fprintf(out, "\t--command-title=CMD title fvwm command, default is '-'\n")
	fmt.Fprintf(out, "\t--command-file=CMD file fvwm command, default is '-'\n")
	fmt.Fprintf(out, "\t--command-app=CMD  +x files fvwm command, default is '-'\n")
	fmt.Fprintf(out, "\t--reuse            no popups, reuse the same menu (no)\n")
	fmt.Fprintf(out, "\t--check-subdirs    check subdir for +x permission (no)\n")
	fmt.Fprintf(out, "\t--special-dirs[=X] include .. and ~ directories (no)\n")
	fmt.Fprintf(out, "\t--memory-for-speed use speed optimization (no)\n")
	fmt.Fprintf(out, "\t--menu-style=NAME  assign specified MenuStyle name to menus\n")
	fmt.Fprintf(out, "Short options are ok if not ambiguous: -a, -icon-f.\n")
}
