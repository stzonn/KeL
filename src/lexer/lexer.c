#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "lexer.h"
#include "lexer_allocation.h"
#include "lexer_error.h"
#include "lexer_utils.h"

/*
 * L stands for "Left"
 * R stands for "Right"
 * Q stands for "Qualifier"
 * PL stands for "Period Left"
 * It is related to the position around a colon. Everything is an L by default.
 *
 * Checking order:
 * 1 - is command (sets `previous_is_command`, dependency: L)
 * 2 - is QL
 * 3 - is L
 * 4 - is QR
 * 5 - is R (sets `previous_is_modifier`, dependencies: L and R)
 *     1 - Consumes all following R modifier operators.
 * 6 - is QLR
 * 7 - is LR
 * 8 - is PL
 * 9 - is literal 
 * 10 - is special (sets `previous_is_modifier`, dependencies: L and R)
 *      1 - Consumes all following R modifier operators
 *      2 - R left parenthesis
 *      3 - R grave accent
 *      4 - Consumes all following L modifier operators
 *      5 - R right parenthesis
 *      6 - Lonely colon
 *      7 - Other special symbols
 * 11 - is valid_name
 *
 * Qualifier cases are checked first to detect the brackets, so it is easier to detect
 * names (for instance, the name of an L or a R).
*/

// more errors will be supported in "lexer_error.c".
static bool token_error = false;

static void create_token_special(
const char* restrict code,
long int start,
TokenType type,
Token* restrict token) {
	*token = (Token) {
		.type = type,
		.subtype = lexer_character_to_subtype(code[start]),
		.start = start,
		.end = start + 1};
}

static void create_token_colon_word(
TokenType type,
long int L_start,
long int L_end,
long int R_start,
long int R_end,
Token* restrict token) {
	*token = (Token) {
		.type = type,
		.subtype = TokenSubtype_NO,
		.L_start = L_start,
		.L_end = L_end,
		.R_start = R_start,
		.R_end = R_end};
}

static void create_token_literal(
TokenSubtype subtype,
long int start,
long int end,
Token* token) {
	*token = (Token) {
		.type = TokenType_LITERAL,
		.subtype = subtype,
		.start = start,
		.end = end};
}

static bool if_command_create_token(
const char* code,
long int start,
Token* token) {
	if(!lexer_is_command(code[start]))
		return false;

	create_token_special(
		code,
		start,
		TokenType_COMMAND,
		token);
	return true;
}

static bool get_QL(
const char* restrict code,
long int start,
long int* end,
long int* L_start,
long int* L_end) {
	if(code[start] != '[')
		return false;

	long int buffer_end = start + 1;
	*L_start = start + 1;

	do {
		lexer_get_next_word(
			code,
			&start,
			&buffer_end);
	} while(code[buffer_end] != ']');

	*L_end = buffer_end;
	*end = buffer_end + 1;
	return true;
}

static bool if_QL_create_token(
const char* restrict code,
long int start,
long int* end,
Token* token) {
	long int buffer_end = *end;
	long int L_start;
	long int L_end;

	if(!get_QL(
		code,
		start,
		&buffer_end,
		&L_start,
		&L_end)
	|| (isgraph(code[buffer_end])
	 && code[buffer_end] != ':')) {
		// better to check this here?
		if(isgraph(code[buffer_end]))
			token_error = true;

		return false;
	}

	if(code[buffer_end] == ':'
	// QR possibility
	&& !isgraph(code[buffer_end + 1]))
		buffer_end += 1;

	if(isgraph(code[buffer_end]))
		return false;

	*end = buffer_end;
	*token = (Token) {
		.type = TokenType_QL,
		.subtype = TokenSubtype_NO,
		.L_start = L_start,
		.L_end = L_end,
		.R_start = L_end,
		.R_end = L_end};
	return true;
}

static bool if_L_create_token(
bool previous_is_command,
bool previous_is_modifier,
const char* restrict code,
long int start,
long int* end,
Token* token) {
	if(previous_is_command
	|| previous_is_modifier
	|| code[start - 1] == ':'
	|| lexer_is_valid_name(
		code,
		start,
		*end)
	== false)
		return false;

	create_token_colon_word(
		TokenType_L,
		start,
		*end,
		*end,
		*end,
		token);
	
	if(code[*end] == ':'
	// R possibility
	&& !isgraph(code[*end + 1]))
		*end += 1;

	return true;
}

static bool get_QR(
const char* restrict code,
long int start,
long int* end,
long int* R_start,
long int* R_end) {
	if(code[start] != ':'
	|| code[start + 1] != '[')
		return false;

	start += 1;
	long int buffer_end = start + 1;
	*R_start = start + 1;

	do {
		lexer_get_next_word(
			code,
			&start,
			&buffer_end);
		TokenSubtype buffer_subtype;
	} while(code[buffer_end] != ']'); 

	*R_end = buffer_end;
	*end = buffer_end + 1;
	return true;
}

static bool if_QR_create_token(
const char* restrict code,
long int start,
long int* end,
Token* token) {
	TokenSubtype subtype;
	long int buffer_end = *end;
	long int R_start;
	long int R_end;

	if(!get_QR(
		code,
		start,
		&buffer_end,
		&R_start,
		&R_end)
	|| isgraph(code[buffer_end]))
		return false;

	*end = buffer_end;
	*token = (Token) {
		.type = TokenType_QR,
		.subtype = TokenSubtype_NO,
		.L_start = R_start,
		.L_end = R_start,
		.R_start = R_start,
		.R_end = R_end};
	return true;
}

static bool if_R_create_token(
bool previous_is_modifier,
const char* restrict code,
long int start,
long int* end,
Token* token) {
	if((code[start] != ':'
	 && !previous_is_modifier)
	|| (!isalpha(code[start + 1])))
		return false;

	long int buffer_end = start + 1;

	if(!previous_is_modifier) {
		lexer_get_next_word(
			code,
			&start,
			&buffer_end);
	}

	if(lexer_is_valid_name(
		code,
		start,
		buffer_end)
	== false)
		return false; // could be an array

	if(!previous_is_modifier)
		*end = buffer_end;

	create_token_colon_word(
		TokenType_R,
		start,
		start,
		start,
		*end,
		token);
	return true;
}

static bool if_QLR_create_token(
const char* restrict code,
long int start,
long int* end,
Token* token) {
	long int buffer_end = *end;
	long int L_start;
	long int L_end;
	long int R_start;
	long int R_end;
	
	if(get_QL(
		code,
		start,
		&buffer_end,
		&L_start,
		&L_end)
	== false)
		return false;

	start = buffer_end;

	if(get_QR(
		code,
		start,
		&buffer_end,
		&R_start,
		&R_end)
	== false)
		return false;

	if(isgraph(code[buffer_end]))
		return false;

	*end = buffer_end;
	*token = (Token) {
		.type = TokenType_QLR,
		.subtype = TokenSubtype_NO,
		.L_start = L_start,
		.L_end = L_end,
		.R_start = R_start,
		.R_end = R_end};
	return true;
}

static bool if_LR_create_token(
bool previous_is_command,
const char* restrict code,
long int start,
long int* end,
Token* token) {
	if(previous_is_command
	|| !lexer_is_valid_name(
		code,
		start,
		*end)
	|| code[*end] != ':')
		return false;

	long int R_start = *end + 1;
	long int R_end = *end + 1;
	lexer_get_next_word(
		code,
		&R_start,
		&R_end);

	if(lexer_is_valid_name(
		code,
		R_start,
		R_end)
	== false)
		return false;

	create_token_colon_word(
		TokenType_LR,
		start,
		*end,
		R_start,
		R_end,
		token);
	*end = R_end;
	return true;
}

static bool if_literal_create_token(
const char* restrict code,
long int start,
long int* end,
Token* token) {
	TokenSubtype subtype;
	long int buffer_end = start + 1;

	if(isdigit(code[start])) {
		// base check
		if(code[start] == '0'
		&& !isdigit(code[buffer_end])) {
			switch(code[buffer_end]) {
				case 'B': break;
				case 'o': break;
				case 'x': break;
				default: token_error = true; return false; // unknown base
			}

			buffer_end += 1;

			if(!isXdigit(code[buffer_end])) {
				token_error = true;
				return false;
			}
		}

		while(code[buffer_end] != '\0'
		   && (isXdigit(code[buffer_end])
		    || code[buffer_end] == '`')) buffer_end += 1;
		// a number cannot be followed by '`' and must be followed by a blank or a special symbole
		if(code[buffer_end - 1] == '`'
		|| (isgraph(code[buffer_end])
		 && !lexer_is_special(code[buffer_end]))) {
			token_error = true;
			return false;
		}

		subtype = TokenSubtype_LITERAL_NUMBER;
	} else if(code[start] == '\'') {
		while(code[buffer_end] != '\0'
		   && code[buffer_end] != '\'') buffer_end += 1;

		if(code[buffer_end] != '\'') {
			token_error = true;
			return false;
		}

		start += 1;
		buffer_end += 1;
		subtype = TokenSubtype_LITERAL_CHARACTER;
	} else if(code[start] == '`') {
		while(code[buffer_end] != '\0'
		   && code[buffer_end] != '`') buffer_end += 1;

		if(code[buffer_end] != '`') {
			token_error = true;
			return false;
		}

		start += 1;
		buffer_end += 1;
		subtype = TokenSubtype_LITERAL_STRING;
	} else {
		return false;
	}

	*token = (Token) {
		.type = TokenType_LITERAL,
		.subtype = subtype,
		.start = start,
		.end = buffer_end - (subtype != TokenSubtype_LITERAL_NUMBER ? 1 : 0)};
	*end = buffer_end;
	return true;
}

static bool if_PL_create_token(
const char* code,
long int start,
long int* end,
Token* token) {
	if(code[start] != '.'
	|| !isgraph(code[start + 1]))
		return false;

	long int buffer_end = start + 1;
	lexer_get_next_word(
		code,
		&start,
		&buffer_end);

	if(lexer_is_valid_name(
		code,
		start,
		buffer_end)
	== false)
		return false;

	*end = buffer_end;
	create_token_colon_word(
		TokenType_PL,
		start,
		buffer_end,
		buffer_end,
		buffer_end,
		token);
	return true;
}

static bool if_special_create_token(
const char* code,
long int start,
Token* token) {
	if(!lexer_is_special(code[start]))
		return false;

	create_token_special(
		code,
		start,
		TokenType_SPECIAL,
		token);
	return true;
}

static bool if_valid_name_create_token(
const char* code,
long int start,
long int end,
Token* token) {
	if(lexer_is_valid_name(
		code,
		start,
		end)
	== false)
		return false;

	*token = (Token) {
		.type = TokenType_IDENTIFIER,
		.subtype = TokenSubtype_NO,
		.start = start,
		.end = end};
	return true;
}

bool create_lexer(
const Source* source,
Allocator* restrict allocator,
Lexer* restrict lexer) {
	lexer->source = source;
	lexer->tokens = NULL;
	lexer->count = 0;

	const char* code = source->content;
	bool previous_is_command = false;
	bool previous_is_modifier = false;
	long int count_L_parenthesis_nest = 0; // to get a good match with R parenthesis
	long int start = 0;
	long int end = 0;
	long int i = 0;

	if(lexer_scan_errors(
		source,
		allocator)
	== false)
		return false;
	// to prevent to check code[start - 1] != ':' in the L case
	if(code[start] == ':') {
		// allocation
		if(lexer_allocate_chunk(
			i,
			lexer)
		== false) {
			destroy_lexer(lexer);
			return false;
		}
		// only valid cases
		if(if_QR_create_token(
			code,
			start,
			&end,
			&lexer->tokens[0])
		== true) {
			// OK
		} else if(if_R_create_token(
			false,
			code,
			start,
			&end,
			&lexer->tokens[0])
		== true) {
			goto R; // there is work to be done
		} else {
			return false;
		}

		i += 1;
	}
	// main loop
	while(lexer_get_next_word(
		code,
		&start,
		&end)
	== true) {
		while(lexer_skip_comment(
			code,
			&start,
			&end));

		if(code[end] == '\0')
			break;
		// allocation
		if(lexer_allocate_chunk(
			i,
			lexer)
		== false) {
			destroy_lexer(lexer);
			return false;
		}
		// create tokens
		Token* token = &lexer->tokens[i];

		if(if_command_create_token(
			code,
			start,
			token)
		== true) {
			previous_is_command = true;
		} else if(if_QL_create_token(
			code,
			start,
			&end,
			token)
		== true) {
			// OK
		} else if(if_L_create_token(
			previous_is_command,
			previous_is_modifier,
			code,
			start,
			&end,
			token)
		== true)  {
			// OK
		} else if(if_QR_create_token(
			code,
			start,
			&end,
			token)
		== true) {
			// OK
		} else if(if_R_create_token(
			previous_is_modifier,
			code,
			start,
			&end,
			token)
		== true) {
R:
			previous_is_modifier = false;
			long int buffer_end = end;
			lexer_get_next_word(
				code,
				&start,
				&buffer_end);

			if(lexer_is_operator_modifier(code[start])) {
				Token* tokens = lexer->tokens;

				do {					
					i += 1;
					tokens[i] = (Token) {
						.type = TokenType_R,
						.subtype = lexer_character_to_subtype(code[start]),
						.L_start = start,
						.L_end = start,
						.R_start = start,
						.R_end = buffer_end};
					
					if(lexer_allocate_chunk(
						i,
						lexer)
					== false) {
						destroy_lexer(lexer);
						return false;
					}

					lexer_get_next_word(
						code,
						&start,
						&buffer_end);
				} while(lexer_is_operator_modifier(code[start])); 

				end = start;
			}
		} else if(if_QLR_create_token(
			code,
			start,
			&end,
			token)
		== true) {
			// OK
		} else if(if_LR_create_token(
			previous_is_command,
			code,
			start,
			&end,
			token)
		== true) {
			// OK
		} else if(if_PL_create_token(
			code,
			start,
			&end,
			token)
		== true) {
			// OK
		} else if(if_literal_create_token(
			code,
			start,
			&end,
			token)
		== true) {
			// OK
		} else if(lexer_is_special(code[start])) {
			Token* tokens = lexer->tokens;
			long int buffer_end = end;
			// right case
			if(code[start] == ':'
			&& (lexer_is_operator_leveling(code[buffer_end])
			 || code[buffer_end] == '[')) {
				// start at the first leveling operator, or open bracket
				start += 1;
				buffer_end += 1;

				do {
					tokens[i] = (Token) {
						.type = TokenType_R,
						.subtype = lexer_character_to_subtype(code[start]),
						.L_start = start,
						.L_end = start,
						.R_start = start,
						.R_end = buffer_end};
					i += 1;
					// it must not be EOF (KEY_MODIFIER_EOF)
					lexer_get_next_word(
						code,
						&start,
						&buffer_end);

					if(lexer_allocate_chunk(
						i,
						lexer)
					== false) {
						destroy_lexer(lexer);
						return false;
					}
				} while(lexer_is_operator_modifier(code[start]));

				end = start;
				previous_is_modifier = true;
				i -= 1; // `i` is incremented at the end of the loop
			} else if(code[start] == ':'
			       && code[buffer_end] == '(') {
				// it is the only special symbol in this case
				tokens[i] = (Token) {
					.type = TokenType_R,
					.subtype = TokenSubtype_LPARENTHESIS,
					.L_start = start,
					.L_end = start,
					.R_start = start + 1,
					.R_end = start + 2};
				end += 1;
			} else if(code[start] == ':'
			       && code[buffer_end] == '`') {
				tokens[i] = (Token) {
					.type = TokenType_R,
					.subtype = TokenSubtype_GRAVE_ACCENT,
					.L_start = start,
					.L_end = start,
					.R_start = start + 1,
					.R_end = start + 2};
				end += 1;
			// left case
			} else if(lexer_is_operator_leveling(code[start])
			       || code[start] == '[') {
				long int buffer_start = start;

				while(lexer_is_operator_modifier(code[buffer_end])) {
					lexer_get_next_word(
						code,
						&buffer_start,
						&buffer_end);
				}

				if(code[buffer_end] == ':'
				&& lexer_is_operator_modifier(code[start])) {
					do {
						tokens[i] = (Token) {
							.type = TokenType_L,
							.subtype = lexer_character_to_subtype(code[start]),
							.L_start = start,
							.L_end = end,
							.R_start = end,
							.R_end = end};
						i += 1;
						lexer_get_next_word(
							code,
							&start,
							&end);

						if(lexer_allocate_chunk(
							i,
							lexer)
						== false) {
							destroy_lexer(lexer);
							return false;
						}
					} while(code[start] != ':');

					end -= 1;
					i -= 1; // `i` is incremented at the end of the loop
				} else
					goto TOKEN_SPECIAL;
			} else if(code[start] == ')'
			       && count_L_parenthesis_nest == 0) {
				tokens[i] = (Token) {
					.type = TokenType_R,
					.subtype = TokenSubtype_RPARENTHESIS,
					.L_start = start,
					.L_end = start,
					.R_start = start,
					.R_end = start + 1};
			} else {
TOKEN_SPECIAL:
				// to process R parenthesis
				if(code[start] == '(')
					count_L_parenthesis_nest += 1;
				else if(code[start] == ')')
					count_L_parenthesis_nest -= 1;
				
				if(code[start] == ':') {
					create_token_special(
						code,
						start,
						TokenType_COLON_LONELY,
						token);
				} else {
					create_token_special(
						code,
						start,
						TokenType_SPECIAL,
						token);
				}
			}
		} else if(if_valid_name_create_token(
			code,
			start,
			end,
			token)
		== true) {
			// OK
		} else {
			destroy_lexer(lexer);
			return false;
		}

		previous_is_command = lexer_is_command(code[start]);
		i += 1;
	}

	if(i == 0) {
		destroy_lexer(lexer);
		return false;
	}

	lexer->count = i;
	Token* tokens_realloc = realloc(
		lexer->tokens,
		(lexer->count + 1) * sizeof(Token));

	if(tokens_realloc == NULL) {
		destroy_lexer(lexer);
		return false;
	}

	lexer->tokens = tokens_realloc;
	lexer->tokens[i] = (Token) {
		.type = TokenType_NO,
		.subtype = TokenSubtype_NO};
	return true;
}

void destroy_lexer(
Lexer* restrict lexer) {
	lexer->source = NULL;
	free(lexer->tokens);
	lexer->count = 0;
}
