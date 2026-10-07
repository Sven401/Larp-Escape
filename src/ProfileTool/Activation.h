#pragma once
// Ordinary host setup workflow, private two-slot storage and synthetic owners.
int runActivation(const char *initialPath, bool hintFactory, const char *candidatePath,
                  const char *exportActivePath, const char *journalDirectory, bool restoreOnly);

// Explicit host-file journal directory; actual temporary source, injected cleanup fault.
int runApplicationFileProbe(const char *journalDirectory);
