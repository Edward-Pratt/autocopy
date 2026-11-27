// VS Code keeps a highlight as the primary selection after a click or an
// arrow key, so autocopy cannot tell a highlight that is done from one about
// to be pasted over. Copying them anyway replaced the clipboard all the time,
// and paste-over by rule pasted the old clipboard where the highlight was
// wanted. So highlights made in VS Code are left alone: its own Ctrl+C and
// middle-click are the only copies there.
#include <string.h>

#include "vscode.h"

static const char *const classes[] = {
	"com.microsoft.VSCode", "code", "Code", "code-oss", "codium", "VSCodium",
};

int is_vscode(const char *class)
{
	for (size_t i = 0; i < sizeof classes / sizeof *classes; i++)
		if (!strcmp(classes[i], class))
			return 1;
	return 0;
}
