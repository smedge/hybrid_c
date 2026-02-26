#ifndef NARRATIVE_H
#define NARRATIVE_H

#define MAX_NARRATIVE_ENTRIES 256
#define NARRATIVE_TITLE_LEN 128
#define NARRATIVE_BODY_LEN 2048

typedef struct {
	char node_id[32];
	char zone_name[64];
	char title[NARRATIVE_TITLE_LEN];
	char voice_path[256];
	char body[NARRATIVE_BODY_LEN];
} NarrativeEntry;

void Narrative_load(const char *path);
void Narrative_cleanup(void);
const NarrativeEntry *Narrative_get(const char *node_id);
int Narrative_count(void);

#endif
