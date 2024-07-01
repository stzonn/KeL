#ifndef PARSER_ALLOCATION
#define PARSER_ALLOCATION

#include <stdlib.h>
#include "parser_def.h"

void parser_initialize_allocators(Parser* parser);
bool parser_create_allocators(Parser* parser);
void parser_destroy_allocators(Parser* parser);
bool parser_allocator(Parser* parser);
void parser_allocator_next_link(
	const Node* node,
	const MemoryChainLink** link);
bool parser_allocator_next(
	const Parser* parser,
	const MemoryChainLink** link,
	const Node** node);
const Node* parser_allocator_start_node(
	const Parser* parser,
	const MemoryChainLink** link);
bool parser_allocator_continue_node(
	const Parser* parser,
	const Node* node);
const Node* parser_allocator_start_declaration(
	const Parser* parser,
	const MemoryChainLink** link);
bool parser_allocator_continue_declaration(
	const Parser* parser,
	const Node* node);
void parser_allocator_node_previous(
	Node** node,
	MemoryChainLink* restrict* link);

#endif
