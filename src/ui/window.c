/*
 * window.c
 *
 * Copyright (C) 2012, 2013 James Booth <boothj5@gmail.com>
 *
 * This file is part of Profanity.
 *
 * Profanity is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Profanity is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Profanity.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#endif

#include "config/theme.h"
#include "ui/window.h"
#include "ui/muc_window.h"

static gboolean _default_handle_error_message(ProfWin *self, const char * const from,
    const char * const err_msg);
static void _win_print_time(ProfWin *self, char show_char);
static void _win_presence_colour_on(ProfWin *self, const char * const presence);
static void _win_presence_colour_off(ProfWin *self, const char * const presence);
static void _win_show_contact(ProfWin *self, PContact contact);
static void _print_incoming_message(ProfWin *self, GTimeVal *tv_stamp,
    const char * const from, const char * const message);

ProfWin*
win_create(const char * const title, int cols, win_type_t type)
{
    ProfWin *new_win = malloc(sizeof(struct prof_win_t));
    new_win->from = strdup(title);
    new_win->win = newpad(PAD_SIZE, cols);
    wbkgd(new_win->win, COLOUR_TEXT);
    new_win->y_pos = 0;
    new_win->paged = 0;
    new_win->unread = 0;
    new_win->history_shown = 0;
    new_win->type = type;

    new_win->print_time = _win_print_time;
    new_win->print_line = win_print_line;
    new_win->refresh_win = win_refresh;
    new_win->presence_colour_on = _win_presence_colour_on;
    new_win->presence_colour_off = _win_presence_colour_off;
    new_win->show_contact = _win_show_contact;

    switch (new_win->type)
    {
        case WIN_CONSOLE:
            new_win->handle_error_message = _default_handle_error_message;
            new_win->print_incoming_message = NULL;
            break;
        case WIN_CHAT:
            new_win->handle_error_message = _default_handle_error_message;
            new_win->print_incoming_message = _print_incoming_message;
            break;
        case WIN_MUC:
            new_win->handle_error_message = muc_handle_error_message;
            new_win->print_incoming_message = NULL;
            break;
        case WIN_PRIVATE:
            new_win->handle_error_message = _default_handle_error_message;
            new_win->print_incoming_message = _print_incoming_message;
            break;
        case WIN_DUCK:
            new_win->handle_error_message = _default_handle_error_message;
            new_win->print_incoming_message = NULL;
            break;
        default:
            new_win->handle_error_message = _default_handle_error_message;
            new_win->print_incoming_message = NULL;
            break;
    }

    scrollok(new_win->win, TRUE);

    return new_win;
}

void
win_free(ProfWin* window)
{
    delwin(window->win);
    free(window->from);
    free(window);
    window = NULL;
}

static void
_win_print_time(ProfWin* self, char show_char)
{
    GDateTime *time = g_date_time_new_now_local();
    gchar *date_fmt = g_date_time_format(time, "%H:%M:%S");
    wattron(self->win, COLOUR_TIME);
    wprintw(self->win, "%s %c ", date_fmt, show_char);
    wattroff(self->win, COLOUR_TIME);
    g_date_time_unref(time);
    g_free(date_fmt);
}

void
win_print_line(ProfWin *self, const char show_char, int attrs,
    const char * const msg, ...)
{
    va_list arg;
    va_start(arg, msg);
    GString *fmt_msg = g_string_new(NULL);
    g_string_vprintf(fmt_msg, msg, arg);
    _win_print_time(self, show_char);
    wattron(self->win, attrs);
    wprintw(self->win, "%s\n", fmt_msg->str);
    wattroff(self->win, attrs);
    g_string_free(fmt_msg, TRUE);
    va_end(arg);
}

void
win_refresh(ProfWin *self)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    prefresh(self->win, self->y_pos, 0, 1, 0, rows-3, cols-1);
}

static void
_win_presence_colour_on(ProfWin *self, const char * const presence)
{
    if (g_strcmp0(presence, "online") == 0) {
        wattron(self->win, COLOUR_ONLINE);
    } else if (g_strcmp0(presence, "away") == 0) {
        wattron(self->win, COLOUR_AWAY);
    } else if (g_strcmp0(presence, "chat") == 0) {
        wattron(self->win, COLOUR_CHAT);
    } else if (g_strcmp0(presence, "dnd") == 0) {
        wattron(self->win, COLOUR_DND);
    } else if (g_strcmp0(presence, "xa") == 0) {
        wattron(self->win, COLOUR_XA);
    } else {
        wattron(self->win, COLOUR_OFFLINE);
    }
}

static void
_win_presence_colour_off(ProfWin *self, const char * const presence)
{
    if (g_strcmp0(presence, "online") == 0) {
        wattroff(self->win, COLOUR_ONLINE);
    } else if (g_strcmp0(presence, "away") == 0) {
        wattroff(self->win, COLOUR_AWAY);
    } else if (g_strcmp0(presence, "chat") == 0) {
        wattroff(self->win, COLOUR_CHAT);
    } else if (g_strcmp0(presence, "dnd") == 0) {
        wattroff(self->win, COLOUR_DND);
    } else if (g_strcmp0(presence, "xa") == 0) {
        wattroff(self->win, COLOUR_XA);
    } else {
        wattroff(self->win, COLOUR_OFFLINE);
    }
}

static void
_win_show_contact(ProfWin *self, PContact contact)
{
    const char *barejid = p_contact_barejid(contact);
    const char *name = p_contact_name(contact);
    const char *presence = p_contact_presence(contact);
    const char *status = p_contact_status(contact);
    GDateTime *last_activity = p_contact_last_activity(contact);

    _win_print_time(self, '-');
    _win_presence_colour_on(self, presence);

    if (name != NULL) {
        wprintw(self->win, "%s", name);
    } else {
        wprintw(self->win, "%s", barejid);
    }

    wprintw(self->win, " is %s", presence);

    if (last_activity != NULL) {
        GDateTime *now = g_date_time_new_now_local();
        GTimeSpan span = g_date_time_difference(now, last_activity);

        wprintw(self->win, ", idle ");

        int hours = span / G_TIME_SPAN_HOUR;
        span = span - hours * G_TIME_SPAN_HOUR;
        if (hours > 0) {
            wprintw(self->win, "%dh", hours);
        }

        int minutes = span / G_TIME_SPAN_MINUTE;
        span = span - minutes * G_TIME_SPAN_MINUTE;
        wprintw(self->win, "%dm", minutes);

        int seconds = span / G_TIME_SPAN_SECOND;
        wprintw(self->win, "%ds", seconds);
    }

    if (status != NULL) {
        wprintw(self->win, ", \"%s\"", p_contact_status(contact));
    }

    wprintw(self->win, "\n");
    _win_presence_colour_off(self, presence);
}

static gboolean
_default_handle_error_message(ProfWin *self, const char * const from,
    const char * const err_msg)
{
    return FALSE;
}

static void
_print_incoming_message(ProfWin *self, GTimeVal *tv_stamp,
    const char * const from, const char * const message)
{
    if (tv_stamp == NULL) {
        self->print_time(self, '-');
    } else {
        GDateTime *time = g_date_time_new_from_timeval_utc(tv_stamp);
        gchar *date_fmt = g_date_time_format(time, "%H:%M:%S");
        wattron(self->win, COLOUR_TIME);
        wprintw(self->win, "%s - ", date_fmt);
        wattroff(self->win, COLOUR_TIME);
        g_date_time_unref(time);
        g_free(date_fmt);
    }

    if (strncmp(message, "/me ", 4) == 0) {
        wattron(self->win, COLOUR_THEM);
        wprintw(self->win, "*%s ", from);
        waddstr(self->win, message + 4);
        wprintw(self->win, "\n");
        wattroff(self->win, COLOUR_THEM);
    } else {
        wattron(self->win, COLOUR_THEM);
        wprintw(self->win, "%s: ", from);
        wattroff(self->win, COLOUR_THEM);
        waddstr(self->win, message);
        wprintw(self->win, "\n");
    }
}
