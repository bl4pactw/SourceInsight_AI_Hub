#include <stdlib.h>
#include "mdrender.h"
#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"
#include "cmark-gfm-core-extensions.h"

#define MDRENDER_OPTIONS        (CMARK_OPT_SOURCEPOS | CMARK_OPT_VALIDATE_UTF8 | CMARK_OPT_FOOTNOTES | CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE)

static const char *const extension_name_tbl[] =
{
	"table",
	"strikethrough",
	"autolink",
	"tagfilter",
	"tasklist",
};

static void attach_extensions(cmark_parser *parser)
{
	cmark_syntax_extension *extension;
	size_t index;

	for (index = 0; index < sizeof(extension_name_tbl) / sizeof(extension_name_tbl[0]); index++) {
		extension = cmark_find_syntax_extension(extension_name_tbl[index]);
		if (extension)
			cmark_parser_attach_syntax_extension(parser, extension);
	}
}

char *mdrender_to_html(const char *markdown, size_t markdown_length)
{
	cmark_parser *parser;
	cmark_node *document;
	char *html = NULL;

	cmark_gfm_core_extensions_ensure_registered();
	parser = cmark_parser_new(MDRENDER_OPTIONS);
	if (!parser)
		return NULL;

	attach_extensions(parser);
	cmark_parser_feed(parser, markdown, markdown_length);
	document = cmark_parser_finish(parser);

	if (document) {
		html = cmark_render_html(document, MDRENDER_OPTIONS, cmark_parser_get_syntax_extensions(parser));
		cmark_node_free(document);
	}

	cmark_parser_free(parser);

	return html;
}

void mdrender_free(char *html)
{
	free(html);
}
