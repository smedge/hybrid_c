#ifndef DATA_NODE_H
#define DATA_NODE_H

#include "position.h"
#include "screen.h"
#include <stdbool.h>

#define DATANODE_COUNT 16

void DataNode_initialize(Position position, const char *node_id);
void DataNode_cleanup(void);
void DataNode_refresh_phases(void);
void DataNode_update_all(unsigned int ticks);
void DataNode_render_all(void);
void DataNode_render_bloom_source(void);
void DataNode_render_notification(const Screen *screen);
void DataNode_render_voice_indicator(const Screen *screen);
void DataNode_render_overlay(const Screen *screen);
bool DataNode_is_reading(void);
bool DataNode_dismiss_reading(void);

/* Boss-triggered automated transfers (no physical node) */
void DataNode_trigger_transfer(const char *node_id);

/* Collection tracking */
bool DataNode_is_collected(const char *node_id);
int DataNode_collected_count(void);
const char *DataNode_collected_id(int index);
void DataNode_mark_collected(const char *node_id);
void DataNode_clear_collected(void);

/* God mode labels */
void DataNode_render_god_labels(void);

/* Minimap */
void DataNode_render_minimap(float ship_x, float ship_y,
	float screen_x, float screen_y, float size, float range);

#endif
