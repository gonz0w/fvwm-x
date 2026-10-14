#!/usr/bin/env perl
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
use strict;
use warnings;
use FVWM::EventNames;
use JSON::PP;
require $ARGV[0];

my %calls;
my $module = bless { handlers => {
	regular => {}, special => {}, extended => {
		MX_ECHO() => [sub { $calls{echo}++; }],
		MX_REPLY() => [sub { $calls{reply}++; }],
		(MAX_XMSG_MASK | M_EXTENDED_MSG) => [sub { $calls{all}++; }],
	}
} }, 'FVWM::Module';

binmode STDIN;
local $/;
my $bytes = <STDIN>;
my @expected = qw(MX_VISIBLE_ICON_NAME MX_ENTER_WINDOW MX_LEAVE_WINDOW
	MX_PROPERTY_CHANGE MX_MONITOR_ENABLED MX_MONITOR_DISABLED
	MX_MONITOR_CHANGED MX_MONITOR_FOCUS MX_ECHO MX_REPLY);
my @decoded;
my $mask = 0;
for my $name (@expected) {
	die "truncated header\n" if length($bytes) < HEADER_SIZE * INTSIZE;
	my ($start, $type, $size, $time) =
		unpack('L!4', substr($bytes, 0, HEADER_SIZE * INTSIZE, ''));
	die "wrong header\n" unless $start == START_FLAG && $time == 1234
		&& $size >= HEADER_SIZE && $size <= 256;
	my $length = ($size - HEADER_SIZE) * INTSIZE;
	die "truncated body\n" if length($bytes) < $length;
	my $body = substr($bytes, 0, $length, '');
	die "wrong event $name\n" unless event_name($type) eq $name;
	my $args = event_args($type, event_arg_values($type, $body));
	$module->invoke_handler(FVWM::Event->new($type, $body));
	my @keys = $name eq 'MX_PROPERTY_CHANGE' ? qw(type value win_id) :
		qw(win_id frame_id ptr);
	my @numbers = (11, 22, 33);
	for my $key (@keys) {
		die "wrong native word $key\n" unless $args->{$key} == shift @numbers;
	}
	if ($name ne 'MX_ENTER_WINDOW' && $name ne 'MX_LEAVE_WINDOW') {
		my $key = $name eq 'MX_ECHO' ? 'message' :
			$name =~ /(?:PROPERTY_CHANGE|REPLY)$/ ? 'text' : 'name';
		die "wrong native string\n" unless $args->{$key} eq 'native text';
	}
	$mask |= $type & ~M_EXTENDED_MSG;
	push @decoded, $name;
}
die "trailing bytes or mask mismatch\n" if length($bytes) || $mask != MAX_XMSG_MASK;
die "extended handlers crossed categories\n" unless $calls{echo} == 1
	&& $calls{reply} == 1 && $calls{all} == 10;
print encode_json(\@decoded), "\n";
