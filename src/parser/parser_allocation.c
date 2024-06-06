#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "parser_allocation.h"

#define CHUNK 1

bool parser_create_allocator(Parser* parser) {
	if(create_memory_chain(
		CHUNK,
		sizeof(Node),
		&parser->nodes)
	== false)
		return false;
	// null token implicit (calloc)
	return true;
}

bool parser_allocator(Parser* parser) {
	const size_t count = parser->nodes.last->memArea.count;

	if((Node*) parser->nodes.top + 1 >= (Node*) parser->nodes.last->memArea.addr + count) {
		// the remaining area is filled with null tokens (calloc)
		if(memory_chain_add_area(
			CHUNK,
			&parser->nodes)
		== false)
			return false;
	} else {
		parser->nodes.previous = parser->nodes.top;
		parser->nodes.top = (Node*) parser->nodes.top + 1;
	}

	return true;
}

void parser_allocator_save(Parser* parser) {
	parser->nodes.buffer_count = parser->nodes.count;
	parser->nodes.buffer_previous = parser->nodes.previous;
	parser->nodes.buffer_top = parser->nodes.top;
}

void parser_allocator_clear(Parser* parser) {
	parser->nodes.buffer_count = 0;
	parser->nodes.buffer_previous = NULL;
	parser->nodes.buffer_top = NULL;
}

void parser_allocator_restore(Parser* parser) {
	assert(parser->nodes.buffer_count != 0);
	assert(parser->nodes.buffer_top != NULL);

	while(parser->nodes.count != parser->nodes.buffer_count)
		memory_chain_destroy_memory_area_last(&parser->nodes);

	parser->nodes.previous = parser->nodes.buffer_previous;
	parser->nodes.top = parser->nodes.buffer_top;

	parser_allocator_clear(parser);
}

void parser_destroy_allocator(Parser* parser) {
	destroy_memory_chain(&parser->nodes);
}

#undef CHUNK
