#include "html.h"
#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern int PSLevel;
extern int PageWidth;
extern int PageLeft;
extern int PageRight;
extern char Path[2048];

int	pspdf_export_out(tree_t *document, tree_t *toc, FILE *out);

#ifdef __cplusplus
}
#endif /* __cplusplus */
