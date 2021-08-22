#pragma once

#include "editor.h"

int Load(EditorStateMachine *machine, char *data);

#ifdef EMSCRIPTEN
void Download(EditorStateMachine *machine, const char *file_name);
#else
int Save(EditorStateMachine *machine, const char *path);
#endif // EMSCRIPTEN

// returns the selected file content
char *OpenFileDialog(void);
char *ReadFileContent(const char *path);