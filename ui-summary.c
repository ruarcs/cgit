/* ui-summary.c: functions for generating repo summary page
 *
 * Copyright (C) 2006 Lars Hjemli
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#include "cgit.h"

static int header;

static int cgit_print_branch_cb(const char *refname, const unsigned char *sha1,
				int flags, void *cb_data)
{
	struct commit *commit;
	struct commitinfo *info;
	char buf[256];
	char *ref;

	ref = xstrdup(refname);
	strncpy(buf, refname, sizeof(buf));
	commit = lookup_commit(sha1);
	// object is not really parsed at this point, because of some fallout
	// from previous calls to git functions in cgit_print_log()
	commit->object.parsed = 0;
	if (commit && !parse_commit(commit)){
		info = cgit_parse_commit(commit);
		html("<tr><td>");
		cgit_log_link(ref, NULL, NULL, ref, NULL, NULL, 0);
		html("</td><td>");
		cgit_print_age(commit->date, -1, NULL);
		html("</td><td>");
		html_txt(info->author);
		html("</td><td>");
		cgit_commit_link(info->subject, NULL, NULL, ref, NULL);
		html("</td></tr>\n");
		cgit_free_commitinfo(info);
	} else {
		html("<tr><td>");
		html_txt(buf);
		html("</td><td colspan='3'>");
		htmlf("*** bad ref %s ***", sha1_to_hex(sha1));
		html("</td></tr>\n");
	}
	free(ref);
	return 0;
}


static void cgit_print_object_ref(struct object *obj)
{
	char *page, *arg, *url;

	if (obj->type == OBJ_COMMIT) {
                cgit_commit_link(fmt("commit %s", sha1_to_hex(obj->sha1)), NULL, NULL,
				 cgit_query_head, sha1_to_hex(obj->sha1));
		return;
	} else if (obj->type == OBJ_TREE) {
		page = "tree";
		arg = "id";
	} else {
		page = "view";
		arg = "id";
	}

	url = cgit_pageurl(cgit_query_repo, page,
			   fmt("%s=%s", arg, sha1_to_hex(obj->sha1)));
	html_link_open(url, NULL, NULL);
	htmlf("%s %s", typename(obj->type),
	      sha1_to_hex(obj->sha1));
	html_link_close();
}

static void print_tag_header()
{
	html("<tr class='nohover'><th class='left'>Tag</th>"
	     "<th class='left'>Age</th>"
	     "<th class='left'>Author</th>"
	     "<th class='left'>Reference</th></tr>\n");
	header = 1;
}

static int cgit_print_tag_cb(const char *refname, const unsigned char *sha1,
				int flags, void *cb_data)
{
	struct tag *tag;
	struct taginfo *info;
	struct object *obj;
	char buf[256], *url;

	strncpy(buf, refname, sizeof(buf));
	obj = parse_object(sha1);
	if (!obj)
		return 1;
	if (obj->type == OBJ_TAG) {
		tag = lookup_tag(sha1);
		if (!tag || parse_tag(tag) || !(info = cgit_parse_tag(tag)))
			return 2;
		if (!header)
			print_tag_header();
		html("<tr><td>");
		url = cgit_pageurl(cgit_query_repo, "view",
				   fmt("id=%s", sha1_to_hex(sha1)));
		html_link_open(url, NULL, NULL);
		html_txt(buf);
		html_link_close();
		html("</td><td>");
		if (info->tagger_date > 0)
			cgit_print_age(info->tagger_date, -1, NULL);
		html("</td><td>");
		if (info->tagger)
			html(info->tagger);
		html("</td><td>");
		cgit_print_object_ref(tag->tagged);
		html("</td></tr>\n");
	} else {
		if (!header)
			print_tag_header();
		html("<tr><td>");
		html_txt(buf);
		html("</td><td colspan='2'/><td>");
		cgit_print_object_ref(obj);
		html("</td></tr>\n");
	}
	return 0;
}

static int cgit_print_archive_cb(const char *refname, const unsigned char *sha1,
				 int flags, void *cb_data)
{
	struct tag *tag;
	struct taginfo *info;
	struct object *obj;
	char buf[256], *url;
	unsigned char fileid[20];

	if (prefixcmp(refname, "refs/archives"))
		return 0;
	strncpy(buf, refname+14, sizeof(buf));
	obj = parse_object(sha1);
	if (!obj)
		return 1;
	if (obj->type == OBJ_TAG) {
		tag = lookup_tag(sha1);
		if (!tag || parse_tag(tag) || !(info = cgit_parse_tag(tag)))
			return 0;
		hashcpy(fileid, tag->tagged->sha1);
	} else if (obj->type != OBJ_BLOB) {
		return 0;
	} else {
		hashcpy(fileid, sha1);
	}
	if (!header) {
		html("<table id='downloads'>");
		html("<tr><th>Downloads</th></tr>");
		header = 1;
	}
	html("<tr><td>");
	url = cgit_pageurl(cgit_query_repo, "blob",
			   fmt("id=%s&amp;path=%s", sha1_to_hex(fileid),
			       buf));
	html_link_open(url, NULL, NULL);
	html_txt(buf);
	html_link_close();
	html("</td></tr>");
	return 0;
}

static void cgit_print_branches()
{
	html("<tr class='nohover'><th class='left'>Branch</th>"
	     "<th class='left'>Idle</th>"
	     "<th class='left'>Author</th>"
	     "<th class='left'>Head commit</th></tr>\n");
	for_each_branch_ref(cgit_print_branch_cb, NULL);
}

static void cgit_print_tags()
{
	header = 0;
	for_each_tag_ref(cgit_print_tag_cb, NULL);
}

static void cgit_print_archives()
{
	header = 0;
	for_each_ref(cgit_print_archive_cb, NULL);
	if (header)
		html("</table>");
}

void cgit_print_summary()
{
	html("<div id='summary'>");
	cgit_print_archives();
	html("<h2>");
	html_txt(cgit_repo->name);
	html(" - ");
	html_txt(cgit_repo->desc);
	html("</h2>");
	if (cgit_repo->readme)
		html_include(cgit_repo->readme);
	html("</div>");
	if (cgit_summary_log > 0)
		cgit_print_log(cgit_query_head, 0, cgit_summary_log, NULL, NULL, 0);
	html("<table class='list nowrap'>");
	if (cgit_summary_log > 0)
		html("<tr class='nohover'><td colspan='4'>&nbsp;</td></tr>");
	cgit_print_branches();
	html("<tr class='nohover'><td colspan='4'>&nbsp;</td></tr>");
	cgit_print_tags();
	html("</table>");
}
