#include "narrative.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static NarrativeEntry entries[MAX_NARRATIVE_ENTRIES];
static int entryCount = 0;

void Narrative_load(const char *path)
{
	entryCount = 0;

	FILE *f = fopen(path, "r");
	if (!f) {
		printf("Narrative: could not open %s\n", path);
		return;
	}

	char line[4096];
	NarrativeEntry *cur = NULL;

	while (fgets(line, sizeof(line), f)) {
		/* Strip trailing newline */
		int len = (int)strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';

		/* Skip empty lines and comments */
		if (len == 0 || line[0] == '#')
			continue;

		if (strncmp(line, "node ", 5) == 0) {
			if (entryCount >= MAX_NARRATIVE_ENTRIES) {
				printf("Narrative: max entries (%d) reached\n", MAX_NARRATIVE_ENTRIES);
				break;
			}
			cur = &entries[entryCount];
			memset(cur, 0, sizeof(*cur));
			cur->voice_gain = 1.0f;
			strncpy(cur->node_id, line + 5, sizeof(cur->node_id) - 1);
		}
		else if (cur && strncmp(line, "zone ", 5) == 0) {
			strncpy(cur->zone_name, line + 5, sizeof(cur->zone_name) - 1);
		}
		else if (cur && strncmp(line, "title ", 6) == 0) {
			strncpy(cur->title, line + 6, sizeof(cur->title) - 1);
		}
		else if (cur && strncmp(line, "voice ", 6) == 0) {
			strncpy(cur->voice_path, line + 6, sizeof(cur->voice_path) - 1);
		}
		else if (cur && strncmp(line, "gain ", 5) == 0) {
			cur->voice_gain = (float)atof(line + 5);
		}
		else if (cur && strncmp(line, "body ", 5) == 0) {
			/* Process \n escape sequences into real newlines */
			const char *src = line + 5;
			char *dst = cur->body;
			int remaining = NARRATIVE_BODY_LEN - 1;
			while (*src && remaining > 0) {
				if (src[0] == '\\' && src[1] == 'n') {
					*dst++ = '\n';
					src += 2;
					remaining--;
				} else {
					*dst++ = *src++;
					remaining--;
				}
			}
			*dst = '\0';
		}
		else if (cur && strcmp(line, "end") == 0) {
			entryCount++;
			cur = NULL;
		}
	}

	fclose(f);
	printf("Narrative: loaded %d entries from %s\n", entryCount, path);
}

void Narrative_cleanup(void)
{
	entryCount = 0;
}

const NarrativeEntry *Narrative_get(const char *node_id)
{
	for (int i = 0; i < entryCount; i++) {
		if (strcmp(entries[i].node_id, node_id) == 0)
			return &entries[i];
	}
	return NULL;
}

int Narrative_count(void)
{
	return entryCount;
}
