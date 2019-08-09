#include "html.h"
#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern int PSLevel;
int	pspdf_export_out(tree_t *document, tree_t *toc, FILE *out);

#ifdef __cplusplus
}
#endif /* __cplusplus */
