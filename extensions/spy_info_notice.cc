/*
 *  ircd-ratbox: A slightly useful ircd.
 *  spy_info_notice.c: Sends a notice when someone uses INFO.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

using namespace ircd;

static const char spy_desc[] = "Sends a notice when someone uses INFO";

void show_info(hook_data *);

mapi_hfn_list_av1 info_hfnlist[] = {
	{"doing_info", (hookfn) show_info},
	{NULL, NULL}
};

DECLARE_MODULE_AV2(info_spy, NULL, NULL, NULL, NULL, info_hfnlist, NULL, NULL, spy_desc);

void
show_info(hook_data *data)
{
	sendto_realops_snomask(sno::SPY, L_ALL,
			     "info requested by %s (%s@%s) [%s]",
			     data->client->name, data->client->username,
			     data->client->host, data->client->servptr->name);
}
