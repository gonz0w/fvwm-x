/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "tray.h"
#include "libs/Colorset.h"
#include "libs/Parse.h"
#include "libs/PictureUtils.h"
#include "libs/FftInterface.h"
#include "libs/fd_safety.h"
#include "libs/log.h"
#include "libs/safemalloc.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define TRAY_TURN_LIMIT 32U

static volatile sig_atomic_t tray_stopping;
static volatile sig_atomic_t tray_wake_fd = -1;

static void stop_signal(int number)
{
	int saved_errno = errno;
	char byte = 1;

	(void)number;
	tray_stopping = 1;
	if (tray_wake_fd >= 0)
	{
		ssize_t ignored = write(tray_wake_fd, &byte, 1);
		(void)ignored;
	}
	errno = saved_errno;
}

/* The isolated module may briefly wait for a cooperative old owner. Core
 * traffic stays queued in order; HUP is observable even with no POLLIN mask.
 * Never call this helper inside an X server transaction. */
bool tray_startup_pause(tray_ui *ui, int milliseconds)
{
	struct pollfd descriptors[3] = {
	    {ui->wake[0], POLLIN, 0}, {ui->channel[1], 0, 0},
	    {ConnectionNumber(ui->display), 0, 0}};

	if (tray_stopping || ui->stopped || ui->failed)
		return false;
	int result = poll(descriptors, 3, milliseconds);
	if (result < 0 && errno != EINTR)
		ui->failed = true;
	if (descriptors[0].revents &
	    (POLLIN | POLLERR | POLLHUP | POLLNVAL))
		ui->stopped = true;
	if (descriptors[1].revents & (POLLERR | POLLHUP | POLLNVAL))
		ui->stopped = true;
	if (descriptors[2].revents & (POLLERR | POLLHUP | POLLNVAL))
		ui->failed = true;
	return !tray_stopping && !ui->stopped && !ui->failed;
}

static void refused(tray_ui *ui, const char *reason)
{
	static unsigned int diagnostics;

	(void)ui;
	if (diagnostics < 8)
	{
		diagnostics++;
		fvwm_debug(__func__, "FvwmTray: %s", reason);
	}
}

static tray_options defaults(void)
{
	tray_options options = {0};

	options.padding = options.gap = 4;
	options.position = 3;
	options.visible = true;
	options.icon_size = 24;
	strcpy(options.icon_theme, "hicolor");
	strcpy(options.font_name, "fixed");
	return options;
}

static int scaled(const tray_ui *ui, unsigned int logical)
{
	int value = 0;

	if (monitor_scale_apply((int)logical,
	    (monitor_scale_factor){ui->quarters}, &value) !=
	    MONITOR_SCALE_CONVERSION_OK)
		return (int)logical;
	return value;
}

static bool font_valid(FlocaleFont *font, const char *name)
{
	char header[] = "Tray";

	/* Flocale's explicit fallback has a different name. Keep a failed font
	 * request from committing the rest of a configuration transaction. */
	if (!font || !font->name || strcmp(font->name, name) ||
	    font->height < 1 || font->height > 4096)
		return false;
	int width = FlocaleTextWidth(font, header, strlen(header));
	return width >= 0 && width <= 4096;
}

/* Monitor and reservation samples stay associated with the core's published
 * topology epoch. Foreign dimensions never pass through this scale path. */
static bool update_area(tray_ui *ui)
{
	const module_ui_scale_snapshot *snapshot = &ui->monitors.published;
	const module_ui_scale_row *row = NULL;
	module_monitor_area area;
	unsigned int quarters = 4;
	XWindowAttributes root;

	if (ui->container.attached && !tray_container_geometry(ui))
		tray_container_detach(ui);
	if (snapshot->count)
	{
		if (ui->anchored || ui->container.attached)
		{
			rectangle anchor = ui->container.attached
			    ? (rectangle){ui->container.root_x,
			      ui->container.root_y,
			      ui->container.width, ui->container.height}
			    : (rectangle){ui->anchor.x, ui->anchor.y,
			      ui->anchor.width, ui->anchor.height};
			row = module_ui_scale_select(snapshot, &anchor, None);
			if (!row)
				return false;
		}
		else
		{
			row = &snapshot->rows[0];
			for (unsigned int i = 0; i < snapshot->count; i++)
				if (snapshot->rows[i].primary)
					row = &snapshot->rows[i];
		}
		if (!module_workarea_bounds(&ui->workareas, snapshot, row,
		    &area))
			return false;
		quarters = row->quarters;
	}
	else
	{
		if (!XGetWindowAttributes(ui->display, ui->root, &root))
			return false;
		area = (module_monitor_area){0, 0, root.width, root.height};
	}
	if (area.width < 1 || area.height < 1 || area.width > 32767 ||
	    area.height > 32767 || area.x < -32768 || area.x > 32767 ||
	    area.y < -32768 || area.y > 32767 ||
	    area.x + area.width > 32767 || area.y + area.height > 32767)
		return false;
	if (!ui->font || ui->quarters != quarters)
	{
		FlocaleFont *font = FlocaleLoadFontAtScale(ui->display,
		    ui->font_name, ui->module->name,
		    (monitor_scale_factor){quarters}, 0);
		if (!font_valid(font, ui->font_name))
		{
			if (font)
				FlocaleUnloadFont(ui->display, font);
			return false;
		}
		if (ui->font)
			FlocaleUnloadFont(ui->display, ui->font);
		ui->font = font;
		ui->quarters = quarters;
	}
	if (ui->container.attached)
		area = (module_monitor_area){ui->container.root_x,
		    ui->container.root_y, ui->container.width,
		    ui->container.height};
	ui->area = area;
	return true;
}

static void draw_header(tray_ui *ui, char *text, int x, int y,
	int width, int height)
{
	FlocaleWinString string = {0};
	XRectangle clip;

	if (width < 1 || height < 1)
		return;
	if (height > ui->font->height)
		height = ui->font->height;
	clip = (XRectangle){x, y, width, height};
	XSetClipRectangles(ui->display, ui->gc, 0, 0, &clip, 1, Unsorted);
	XSetForeground(ui->display, ui->gc, GetColorset(ui->colorset)->fg);
	if (ui->font->font)
		XSetFont(ui->display, ui->gc, ui->font->font->fid);
	string.str = text;
	string.len = strlen(text);
	string.gc = ui->gc;
	string.win = ui->host;
	string.colorset = GetColorset(ui->colorset);
	string.flags.has_colorset = 1;
	string.x = x;
	string.y = y + ui->font->ascent;
	string.clip_region = XCreateRegion();
	string.flags.has_clip_region = 1;
	XUnionRectWithRegion(&clip, string.clip_region, string.clip_region);
	FlocaleDrawString(ui->display, ui->font, &string, FWS_HAVE_LENGTH);
	XDestroyRegion(string.clip_region);
	XSetClipMask(ui->display, ui->gc, None);
}

void tray_render(tray_ui *ui)
{
	char header[80];

	ui->dirty = false;
	/* Discovery and docking can precede the configuration replay. Publish
	 * no default frame before the first complete valid owner snapshot. */
	if (!ui->config_ready)
		return;
	if (!ui->host || ui->stopped || !update_area(ui))
	{
		refused(ui, "unavailable host geometry or font");
		ui->failed = true;
		return;
	}
	tray_sni_sync(ui);
	int padding = scaled(ui, ui->padding);
	int gap = scaled(ui, ui->gap);
	/* Reserve the maximum bounded cue so paging itself does not resize
	 * the popup or move its navigation targets. */
	char measure[] = "Tray 32/32 (32) -32";
	int header_width = FlocaleTextWidth(ui->font, measure, strlen(measure));
	int header_height = ui->font->height;
	int target = scaled(ui, 24);
	/* The host's clickable entry keeps the shared native target floor even
	 * with zero padding and a one-pixel foreign icon. */
	if (header_width < target)
		header_width = target;
	header_width += target * 2 + gap * 2;
	if (header_width > 4096)
		header_width = 4096;
	if (header_height < target)
		header_height = target;
	bool layout_valid = ui->container.attached
	    ? tray_model_compact(&ui->model, ui->area.width, ui->area.height,
	      padding, gap, target, ui->vertical, ui->visible, ui->page,
	      &ui->layout, &ui->page_prev, &ui->page_next)
	    : tray_model_page(&ui->model, ui->area.width, ui->area.height,
	      padding, gap, header_width, header_height, ui->vertical,
	      ui->visible, ui->page, &ui->layout);
	if (!layout_valid)
	{
		refused(ui, "unrepresentable host layout");
		ui->failed = true;
		return;
	}
	ui->page = ui->layout.page;
	int focused = tray_model_index(&ui->model, ui->model.focused);
	if ((!ui->visible && ui->active) ||
	    (focused >= 0 && !ui->layout.cells[focused].visible))
		tray_x11_unfocus(ui, true);
	int x = ui->container.attached ? 0 : ui->area.x;
	int y = ui->container.attached ? 0 : ui->area.y;
	if (!ui->container.attached && (ui->position & 1))
		x += ui->area.width - ui->layout.width;
	if (!ui->container.attached && (ui->position & 2))
		y += ui->area.height - ui->layout.height;
	if (ui->anchored)
	{
		tray_rect area = {ui->area.x, ui->area.y,
		    ui->area.width, ui->area.height}, placed;
		if (!tray_model_place(&ui->anchor, &area, ui->layout.width,
		    ui->layout.height, &placed))
		{
			refused(ui, "unrepresentable popup placement");
			ui->failed = true;
			return;
		}
		x = placed.x;
		y = placed.y;
	}
	XMoveResizeWindow(ui->display, ui->host, x, y,
	    ui->layout.width, ui->layout.height);
	SetWindowBackground(ui->display, ui->host, ui->layout.width,
	    ui->layout.height, GetColorset(ui->colorset),
	    DefaultDepth(ui->display, DefaultScreen(ui->display)),
	    ui->gc, False);
	XClearWindow(ui->display, ui->host);
	if (ui->visible)
	{
		XMapWindow(ui->display, ui->proxy);
		XMapWindow(ui->display, ui->host);
	}
	else
		XUnmapWindow(ui->display, ui->host);
	tray_x11_layout(ui);
	if (!ui->container.attached)
		ui->page_prev = ui->page_next = (tray_cell){0};
	if (!ui->failed && !ui->stopped && ui->visible)
	{
		snprintf(header, sizeof(header), "Tray %u/%u (%u)",
		    ui->layout.page + 1, ui->layout.pages, ui->model.count);
		if (ui->layout.omitted)
		{
			size_t length = strlen(header);
			snprintf(header + length, sizeof(header) - length,
			    " -%u", ui->layout.omitted);
		}
		if (ui->container.attached)
		{
			if (ui->page_prev.visible)
			{
				draw_header(ui, "<", ui->page_prev.x,
				    ui->page_prev.y, target, target);
				draw_header(ui, ">", ui->page_next.x,
				    ui->page_next.y, target, target);
			}
			else if (!ui->layout.count)
				draw_header(ui, "Tray", padding, padding,
				    ui->layout.width - padding * 2,
				    header_height);
			XStoreName(ui->display, ui->host, header);
			tray_sni_repaint_popups(ui);
			XFlush(ui->display);
			return;
		}
		int width = ui->layout.width - padding * 2;
		if (width >= target * 2 &&
		    ui->layout.height >= padding + target)
		{
			ui->page_prev = (tray_cell){padding, padding,
			    target, target, true};
			ui->page_next = (tray_cell){
			    ui->layout.width - padding - target, padding,
			    target, target, true};
			draw_header(ui, "<", ui->page_prev.x, padding,
			    target, target);
			draw_header(ui, ">", ui->page_next.x, padding,
			    target, target);
			draw_header(ui, header, padding + target + gap, padding,
			    width - target * 2 - gap * 2, header_height);
		}
		else
			draw_header(ui, header, padding, padding,
			    width, header_height);
	}
	tray_sni_repaint_popups(ui);
	XFlush(ui->display);
}

void tray_page(tray_ui *ui, int direction, bool keyboard, Time timestamp)
{
	if (ui->dirty)
		tray_render(ui);
	if (ui->failed || ui->stopped || !ui->visible || ui->layout.pages < 2)
		return;
	bool refocus = keyboard && ui->active;
	ui->page = direction < 0
	    ? (ui->page + ui->layout.pages - 1) % ui->layout.pages
	    : (ui->page + 1) % ui->layout.pages;
	/* Keyboard paging keeps the proxy, avoiding a queued core restoration
	 * racing the explicit entry into the new page. Other paging restores
	 * the application before unmapping the old logical focus. */
	tray_x11_unfocus(ui, !refocus);
	ui->dirty = true;
	tray_render(ui);
	if (refocus && !ui->failed && !ui->stopped)
		tray_x11_focus(ui, 0, direction < 0 ? -1 : 0, timestamp);
}

/* Geometry resnapshots update topology/appearance without undoing runtime
 * Show/Hide/ToggleAt. Explicit requests dominate automatic refreshes. */
static void request_config(tray_ui *ui, bool apply_visible)
{
	if (ui->config_pending)
	{
		ui->config_refresh = true;
		ui->refresh_apply_visible |= apply_visible;
		return;
	}
	ui->staged = defaults();
	ui->config_valid = true;
	ui->config_pending = true;
	ui->config_apply_visible = apply_visible;
	InitGetConfigLine(ui->channel, ui->prefix);
}

void tray_request_config(tray_ui *ui)
{
	request_config(ui, true);
}

static char *skip_space(char *text)
{
	while (*text == ' ' || *text == '\t')
		text++;
	return text;
}

static void stage_option(tray_ui *ui, char *text)
{
	char *option = skip_space(text);
	char *value = option;
	char *end;
	int number;
	bool valid = false;

	while (*value && *value != ' ' && *value != '\t')
		value++;
	if (!*value)
		goto invalid;
	*value++ = 0;
	value = skip_space(value);
	/* Whitespace following an otherwise complete option is harmless. */
	end = value + strlen(value);
	while (end > value && (end[-1] == ' ' || end[-1] == '\t'))
		*--end = 0;
	if (!strcasecmp(option, "Font"))
	{
		char *font = NULL;
		end = GetNextToken(value, &font);
		valid = font && *font && strlen(font) <= 256 &&
		    (!end || !*skip_space(end));
		if (valid)
			strcpy(ui->staged.font_name, font);
		free(font);
	}
	else if (!strcasecmp(option, "Colorset"))
	{
		valid = ParseColorsetId(value, &end, &number) && !*end &&
		    number >= 0;
		if (valid)
			ui->staged.colorset = number;
	}
	else if (!strcasecmp(option, "Gap") || !strcasecmp(option, "Padding"))
	{
		valid = module_parse_decimal_integers(value, &number, 1) &&
		    number >= 0 && number <= 64;
		if (valid && !strcasecmp(option, "Gap"))
			ui->staged.gap = number;
		else if (valid)
			ui->staged.padding = number;
	}
	else if (!strcasecmp(option, "Orientation"))
	{
		valid = !strcasecmp(value, "Horizontal") ||
		    !strcasecmp(value, "Vertical");
		if (valid)
			ui->staged.vertical = !strcasecmp(value, "Vertical");
	}
	else if (!strcasecmp(option, "Position"))
	{
		const char *positions[] = {
		    "TopLeft", "TopRight", "BottomLeft", "BottomRight"};
		for (unsigned int i = 0; i < 4; i++)
			if (!strcasecmp(value, positions[i]))
			{
				ui->staged.position = i;
				valid = true;
				break;
			}
	}
	else if (!strcasecmp(option, "StatusNotifier"))
	{
		valid = !strcasecmp(value, "On") || !strcasecmp(value, "Off");
		if (valid)
			ui->staged.modern = !strcasecmp(value, "On");
	}
	else if (!strcasecmp(option, "IconSize"))
	{
		valid = module_parse_decimal_integers(value, &number, 1) &&
		    number >= 8 && number <= 128;
		if (valid)
			ui->staged.icon_size = number;
	}
	else if (!strcasecmp(option, "IconTheme"))
	{
		char *theme = NULL;
		end = GetNextToken(value, &theme);
		valid = theme && *theme && strlen(theme) <= 128 &&
		    !strpbrk(theme, "/\\\r\n") && strcmp(theme, ".") &&
		    strcmp(theme, "..") && (!end || !*skip_space(end));
		if (valid)
			strcpy(ui->staged.icon_theme, theme);
		free(theme);
	}
	else if (!strcasecmp(option, "Visible"))
	{
		valid = !strcasecmp(value, "On") || !strcasecmp(value, "Off");
		if (valid)
			ui->staged.visible = !strcasecmp(value, "On");
	}
	if (valid)
		return;
invalid:
	ui->config_valid = false;
}

static void config_message(tray_ui *ui, char *text)
{
	module_ui_scale_status status =
	    module_ui_scale_receive(&ui->monitors, text);

	if (status == MODULE_UI_SCALE_PUBLISHED)
	{
		module_workarea_sync(&ui->workareas, &ui->monitors.published);
		ui->dirty = true;
		if (!ui->config_pending)
			request_config(ui, false);
		return;
	}
	if (status != MODULE_UI_SCALE_IGNORED)
		return;
	if (module_workarea_receive(&ui->workareas,
	    &ui->monitors.published, text) == MODULE_WORKAREA_ADMITTED)
	{
		ui->dirty = true;
		return;
	}
	if (!strncasecmp(text, "LoggingFD", 9) &&
	    (text[9] == ' ' || text[9] == '\t'))
	{
		int descriptor = -1;
		if (!module_parse_decimal_integers(text + 9, &descriptor, 1) ||
		    !log_adopt_inherited_fd(descriptor))
			refused(ui, "invalid logging descriptor");
		return;
	}
	if (!strncasecmp(text, "Colorset ", 9))
	{
		if (LoadColorset(text + 8) == ui->colorset)
			ui->dirty = true;
		return;
	}
	size_t length = strlen(ui->prefix);
	if (strncasecmp(text, ui->prefix, length) ||
	    (text[length] && text[length] != ':' && text[length] != ' ' &&
	    text[length] != '\t'))
		return;
	/* Unsolicited module changes request a complete authoritative snapshot;
	 * never partially apply a lone line outside a transaction. */
	if (!ui->config_pending)
	{
		tray_request_config(ui);
		return;
	}
	text += length;
	if (*text == ':')
		text++;
	stage_option(ui, text);
}

static void config_complete(tray_ui *ui)
{
	FlocaleFont *font = NULL;

	module_ui_scale_abort(&ui->monitors);
	if (!ui->config_pending)
		return;
	if (ui->config_valid)
	{
		font = FlocaleLoadFontAtScale(ui->display, ui->staged.font_name,
		    ui->module->name, (monitor_scale_factor){ui->quarters}, 0);
		if (!font_valid(font, ui->staged.font_name) ||
		    !AllocColorset(ui->staged.colorset))
			ui->config_valid = false;
	}
	if (ui->config_valid)
	{
		FlocaleUnloadFont(ui->display, ui->font);
		ui->font = font;
		font = NULL;
		ui->colorset = ui->staged.colorset;
		ui->padding = ui->staged.padding;
		ui->gap = ui->staged.gap;
		ui->position = ui->staged.position;
		ui->vertical = ui->staged.vertical;
		ui->modern = ui->staged.modern;
		ui->icon_size = ui->staged.icon_size;
		strcpy(ui->icon_theme, ui->staged.icon_theme);
		if (ui->config_apply_visible || !ui->config_ready)
			ui->visible = ui->staged.visible;
		ui->config_ready = true;
		strcpy(ui->font_name, ui->staged.font_name);
		tray_x11_hints(ui);
		tray_sni_configure(ui, ui->config_apply_visible);
		ui->dirty = true;
	}
	else
		refused(ui, "invalid configuration snapshot");
	if (font)
		FlocaleUnloadFont(ui->display, font);
	ui->config_pending = false;
	ui->config_apply_visible = false;
	if (ui->config_refresh)
	{
		bool apply_visible = ui->refresh_apply_visible;
		ui->config_refresh = false;
		ui->refresh_apply_visible = false;
		request_config(ui, apply_visible);
	}
}

static void status_reply(tray_ui *ui)
{
	char text[256];
	unsigned int mapped = 0;
	for (unsigned int i = 0;
	    i < ui->model.count && i < TRAY_CLIENT_MAX; i++)
		if (ui->model.clients[i].mapped)
			mapped++;
	int length = snprintf(text, sizeof(text),
	    "Echo FVWM_TRAY_STATUS %u %u %u %u %u %u %" PRIu64
	    " %lu %lu %lu %d", ui->model.count, mapped,
	    TRAY_CLIENT_MAX, ui->quarters, ui->claimed ? 1U : 0U,
	    !ui->config_ready || ui->config_pending || ui->config_refresh
	    ? 1U : 0U,
	    ui->monitors.published.epoch, ui->host, ui->owner, ui->proxy,
	    ui->font ? ui->font->height : 0);

	if (length < 0 || (size_t)length >= sizeof(text))
	{
		ui->failed = true;
		return;
	}
	SendText(ui->channel, text, 0);
}

static void native_message(tray_ui *ui, char *text)
{
	text = skip_space(text);
	char *end = text + strlen(text);
	while (end > text && (end[-1] == ' ' || end[-1] == '\t'))
		*--end = 0;
	if (!strcasecmp(text, "Reload"))
		tray_request_config(ui);
	else if (!strcasecmp(text, "Show") || !strcasecmp(text, "Hide"))
	{
		ui->visible = !strcasecmp(text, "Show");
		if (ui->visible)
			ui->anchored = false;
		ui->dirty = true;
	}
	else if (!strncasecmp(text, "ToggleAt", 8) &&
	    (text[8] == ' ' || text[8] == '\t'))
	{
		tray_rect anchor;
		if (ui->container.attached)
		{
			refused(ui, "popup anchor unavailable while contained");
			return;
		}
		if (!tray_model_anchor(text + 8, &anchor))
		{
			refused(ui, "invalid popup anchor");
			return;
		}
		if (ui->visible)
			ui->visible = false;
		else
		{
			ui->anchor = anchor;
			ui->anchored = true;
			ui->visible = true;
		}
		ui->dirty = true;
	}
	else if (!strcasecmp(text, "PageNext") || !strcasecmp(text, "PagePrev"))
		tray_page(ui, !strcasecmp(text, "PagePrev") ? -1 : 1,
		    false, CurrentTime);
	else if (!strcasecmp(text, "Focus") || !strcasecmp(text, "Next") ||
	    !strcasecmp(text, "Prev"))
	{
		if (ui->dirty)
			tray_render(ui);
		if (!ui->failed && !ui->stopped)
			tray_x11_focus(ui, 0, !strcasecmp(text, "Next") ? 1 :
			    (!strcasecmp(text, "Prev") ? -1 : 0), CurrentTime);
	}
	else if (!strcasecmp(text, "ServiceStatus"))
		tray_sni_status_reply(ui);
	else if (!strcasecmp(text, "Status"))
	{
		if (ui->dirty)
			tray_render(ui);
		status_reply(ui);
	}
	else
		refused(ui, "invalid native command");
}

static bool service_packets(tray_ui *ui)
{
	for (unsigned int i = 0; i < TRAY_TURN_LIMIT; i++)
	{
		FvwmPacket *packet;
		module_packet_read_status read =
		    module_packet_reader_read(&ui->reader, &packet);
		if (read == MODULE_PACKET_READ_MORE)
			return false;
		if (read != MODULE_PACKET_READ_READY)
		{
			ui->stopped = true;
			if (read == MODULE_PACKET_READ_ERROR)
			{
				refused(ui, "invalid module packet stream");
				ui->failed = true;
			}
			return false;
		}
		if (packet->type == M_STRING || packet->type == M_CONFIG_INFO)
		{
			if (!module_packet_body_has_text(packet->body,
			    FvwmPacketBodySize(*packet), 3))
			{
				refused(ui, "malformed module record");
				ui->failed = true;
				return false;
			}
			char *text = (char *)&packet->body[3];
			if (packet->type == M_STRING)
				native_message(ui, text);
			else
				config_message(ui, text);
		}
		else if (packet->type == M_END_CONFIG_INFO)
			config_complete(ui);
		else if (packet->type == M_SENDCONFIG)
			tray_request_config(ui);
		if (ui->failed || ui->stopped || tray_stopping)
			return false;
	}
	return true;
}

static bool module_name_valid(const ModuleArgs *module)
{
	if (module->namelen < 1 || module->namelen > 128)
		return false;
	for (int i = 0; i < module->namelen; i++)
	{
		unsigned char c = (unsigned char)module->name[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9') || c == '_' || c == '-'))
			return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	ModuleArgs *module = ParseModuleArgs(argc, argv, 1);
	tray_ui *ui;
	int status = 1;
	bool x11_started = false;

	if (!module || module->user_argc || module->from_fvwm < 0 ||
	    !module_name_valid(module))
		return 1;
	ui = fxcalloc(1, sizeof(*ui));
	ui->module = module;
	ui->channel[0] = module->to_fvwm;
	ui->channel[1] = module->from_fvwm;
	ui->wake[0] = ui->wake[1] = -1;
	ui->staged = defaults();
	ui->padding = ui->staged.padding;
	ui->gap = ui->staged.gap;
	ui->position = ui->staged.position;
	ui->visible = true;
	ui->icon_size = ui->staged.icon_size;
	strcpy(ui->icon_theme, ui->staged.icon_theme);
	ui->quarters = 4;
	strcpy(ui->font_name, ui->staged.font_name);
	snprintf(ui->prefix, sizeof(ui->prefix), "*%s", module->name);
	if (!fvwm_pipe_cloexec(ui->wake))
		goto out;
	for (unsigned int i = 0; i < 2; i++)
	{
		int flags = fcntl(ui->wake[i], F_GETFL);
		if (flags < 0 ||
		    fcntl(ui->wake[i], F_SETFL, flags | O_NONBLOCK))
			goto out;
	}
	tray_wake_fd = ui->wake[1];
	signal(SIGTERM, stop_signal);
	signal(SIGINT, stop_signal);
	signal(SIGHUP, stop_signal);
	signal(SIGPIPE, stop_signal);
	SendFinishedStartupNotification(ui->channel);
	FlocaleInit(LC_CTYPE, "", "", module->name);
	ui->display = XOpenDisplay(NULL);
	if (!ui->display)
		goto out;
	ui->root = DefaultRootWindow(ui->display);
	flib_init_graphics(ui->display);
	if (!AllocColorset(0))
		goto out;
	ui->font = FlocaleLoadFontAtScale(ui->display, ui->font_name,
	    module->name, (monitor_scale_factor){ui->quarters}, 0);
	if (!font_valid(ui->font, ui->font_name))
		goto out;
	ui->gc = XCreateGC(ui->display, ui->root, 0, NULL);
	if (!ui->gc)
		goto out;
	/* All host resources exist before selection acquisition. */
	x11_started = true;
	if (!tray_x11_start(ui))
	{
		refused(ui, "tray selection unavailable or startup failed");
		goto out;
	}
	tray_sni_init(ui);
	SetMessageMask(ui->channel,
	    M_STRING | M_CONFIG_INFO | M_END_CONFIG_INFO | M_SENDCONFIG);
	if (!module_packet_reader_init(&ui->reader, ui->channel[1]))
		goto out;
	tray_request_config(ui);
	ui->dirty = true;
	while (!tray_stopping && !ui->stopped && !ui->failed)
	{
		for (unsigned int i = 0; i < TRAY_TURN_LIMIT &&
		    XPending(ui->display); i++)
		{
			XEvent event;
			XNextEvent(ui->display, &event);
			if (event.type == Expose &&
			    event.xexpose.window == ui->host)
				ui->dirty = true;
			else if (event.type == PropertyNotify &&
			    event.xproperty.window == ui->root &&
			    event.xproperty.atom == ui->workarea_atom)
			{
				ui->dirty = true;
				request_config(ui, false);
			}
			else
				tray_x11_event(ui, &event);
			if (ui->failed || ui->stopped || tray_stopping)
				break;
		}
		if (ui->failed || ui->stopped || tray_stopping)
			break;
		bool more = service_packets(ui);
		if (ui->failed || ui->stopped || tray_stopping)
			break;
		double stamp = tray_sni_now();
		tray_container_service(ui, stamp);
		tray_sni_service(ui, stamp);
		if (ui->dirty)
			tray_render(ui);
		if (ui->failed || ui->stopped || tray_stopping)
			break;
		struct pollfd descriptors[6] = {
		    {ConnectionNumber(ui->display), POLLIN, 0},
		    {ui->channel[1], POLLIN, 0},
		    {ui->wake[0], POLLIN, 0}};
		int timeout = more || ui->dirty || XPending(ui->display)
		    ? 0 : -1;
		unsigned int extra = tray_sni_pollfds(ui, descriptors + 3);
		int service_timeout = tray_sni_timeout(ui, tray_sni_now());
		if (service_timeout >= 0 &&
		    (timeout < 0 || service_timeout < timeout))
			timeout = service_timeout;
		service_timeout = tray_container_timeout(ui, tray_sni_now());
		if (service_timeout >= 0 &&
		    (timeout < 0 || service_timeout < timeout))
			timeout = service_timeout;
		if (poll(descriptors, 3 + extra, timeout) < 0 && errno != EINTR)
		{
			ui->failed = true;
			break;
		}
		if (descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL))
			ui->failed = true;
		if (descriptors[1].revents & POLLNVAL)
			ui->failed = true;
	}
	status = !ui->failed && (tray_stopping || ui->stopped) ? 0 : 1;
out:
	/* Foreign clients leave their sockets before fonts, GC or X connection.
	 * Failed release keeps the save set through XCloseDisplay. */
	if (ui->display)
	{
		tray_sni_stop(ui);
		if (x11_started)
			tray_x11_stop(ui);
		tray_sni_destroy(ui);
		if (ui->x_error_code)
			fvwm_debug(__func__,
			    "FvwmTray: X error %u resource %lu",
			    ui->x_error_code, ui->x_error_resource);
		if (ui->font)
			FlocaleUnloadFont(ui->display, ui->font);
		if (ui->gc)
			XFreeGC(ui->display, ui->gc);
		XCloseDisplay(ui->display);
	}
	if (ui->failed)
		status = 1;
	FftFini();
	tray_wake_fd = -1;
	if (ui->wake[1] >= 0)
		close(ui->wake[1]);
	if (ui->wake[0] >= 0)
		close(ui->wake[0]);
	close(ui->channel[0]);
	close(ui->channel[1]);
	free(ui);
	return status;
}
