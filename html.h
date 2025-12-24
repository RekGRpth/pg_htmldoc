#ifndef _HTML_H_
#  define _HTML_H_

#  include "types.h"
#  include "file.h"

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */

typedef enum
{
  MARKUP_FILE = -3,	/* File Delimiter */
  MARKUP_UNKNOWN = -2,	/* Unknown element */
  MARKUP_ERROR = -1,
  MARKUP_NONE = 0,
  MARKUP_COMMENT,
  MARKUP_DOCTYPE,
  MARKUP_A,
  MARKUP_ACRONYM,
  MARKUP_ADDRESS,
  MARKUP_APPLET,
  MARKUP_AREA,
  MARKUP_B,
  MARKUP_BASE,
  MARKUP_BASEFONT,
  MARKUP_BIG,
  MARKUP_BLINK,
  MARKUP_BLOCKQUOTE,
  MARKUP_BODY,
  MARKUP_BR,
  MARKUP_CAPTION,
  MARKUP_CENTER,
  MARKUP_CITE,
  MARKUP_CODE,
  MARKUP_COL,
  MARKUP_COLGROUP,
  MARKUP_DD,
  MARKUP_DEL,
  MARKUP_DFN,
  MARKUP_DIR,
  MARKUP_DIV,
  MARKUP_DL,
  MARKUP_DT,
  MARKUP_EM,
  MARKUP_EMBED,
  MARKUP_FONT,
  MARKUP_FORM,
  MARKUP_FRAME,
  MARKUP_FRAMESET,
  MARKUP_H1,
  MARKUP_H2,
  MARKUP_H3,
  MARKUP_H4,
  MARKUP_H5,
  MARKUP_H6,
  MARKUP_H7,
  MARKUP_H8,
  MARKUP_H9,
  MARKUP_H10,
  MARKUP_H11,
  MARKUP_H12,
  MARKUP_H13,
  MARKUP_H14,
  MARKUP_H15,
  MARKUP_HEAD,
  MARKUP_HR,
  MARKUP_HTML,
  MARKUP_I,
  MARKUP_IMG,
  MARKUP_INPUT,
  MARKUP_INS,
  MARKUP_ISINDEX,
  MARKUP_KBD,
  MARKUP_LI,
  MARKUP_LINK,
  MARKUP_MAP,
  MARKUP_MENU,
  MARKUP_META,
  MARKUP_MULTICOL,
  MARKUP_NOBR,
  MARKUP_NOFRAMES,
  MARKUP_OL,
  MARKUP_OPTION,
  MARKUP_P,
  MARKUP_PRE,
  MARKUP_S,
  MARKUP_SAMP,
  MARKUP_SCRIPT,
  MARKUP_SELECT,
  MARKUP_SMALL,
  MARKUP_SPACER,
  MARKUP_SPAN,
  MARKUP_STRIKE,
  MARKUP_STRONG,
  MARKUP_STYLE,
  MARKUP_SUB,
  MARKUP_SUP,
  MARKUP_TABLE,
  MARKUP_TBODY,
  MARKUP_TD,
  MARKUP_TEXTAREA,
  MARKUP_TFOOT,
  MARKUP_TH,
  MARKUP_THEAD,
  MARKUP_TITLE,
  MARKUP_TR,
  MARKUP_TT,
  MARKUP_U,
  MARKUP_UL,
  MARKUP_VAR,
  MARKUP_WBR
} markup_t;

typedef struct
{
  uchar			*name,		/* Variable name */
			*value;		/* Variable value */
} var_t;

typedef struct tree_str
{
  struct tree_str	*parent,	/* Parent tree entry */
			*child,		/* First child entry */
			*last_child,	/* Last child entry */
			*prev,		/* Previous entry on this level */
			*next,		/* Next entry on this level */
			*link;		/* Linked-to */
  markup_t		markup;		/* Markup code */
  uchar			*data;		/* Text (MARKUP_NONE or MARKUP_COMMENT) */
  unsigned		halignment:2,	/* Horizontal alignment */
			valignment:2,	/* Vertical alignment */
			typeface:3,	/* Typeface code */
			size:3,		/* Size of text */
			style:2,	/* Style of text */
			underline:1,	/* Text is underlined? */
			strikethrough:1,/* Text is struck-through? */
			subscript:1,	/* Text is subscripted? */
			superscript:1,	/* Text is superscripted? */
			preformatted:1,	/* Preformatted text? */
			indent:4;	/* Indentation level 0-15 */
  uchar			red,		/* Color of this fragment */
			green,
			blue;
  float			width,		/* Width of this fragment in points */
			height;		/* Height of this fragment in points */
  int			nvars;		/* Number of variables... */
  var_t			*vars;		/* Variables... */
} tree_t;

extern float		_htmlPPI;
extern float		_htmlBrowserWidth;
extern int		_htmlInitialized;

tree_t	*htmlReadFile(tree_t *parent, FILE *fp, const char *base);
tree_t	*htmlAddTree(tree_t *parent, markup_t markup, uchar *data);
int	htmlDeleteTree(tree_t *parent);
void	htmlFixLinks(tree_t *doc, tree_t *tree, uchar *base);
int	htmlSetVariable(tree_t *t, uchar *name, uchar *value);
void	htmlSetCharSet(const char *cs);

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_HTML_H_ */
