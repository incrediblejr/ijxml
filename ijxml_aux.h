#ifndef _IJXML_AUX_H_
#define _IJXML_AUX_H_

#include "ijxml.h"

typedef enum {
	IJXML_AUX_BUFFER_TRUNCATED = -1,
	IJXML_AUX_INVALID_TOKEN_INDEX = -2,

	IJXML_AUX_SUCCESS = 0,
} ijxml_aux_err_t;

#define IJXML_AUX_INVALID_TOKEN_OFFSET		((unsigned)-1)

typedef struct ijxml_aux_context {
	const char *xml;
	struct ijxml_token *tokens;
	unsigned num_tokens;
} ijxml_aux_context;

void ijxml_aux_init(struct ijxml_aux_context *context, const char *xml, struct ijxml_token *tokens, unsigned num_tokens);

unsigned ijxml_aux_object_by_tag(struct ijxml_aux_context *context, unsigned parent_object_index, const char *tag_name);

/* Returns the VALUE token index */
unsigned ijxml_aux_object_attribute(struct ijxml_aux_context *context, unsigned object_index, const char *attribute_name);

unsigned ijxml_aux_object_at(struct ijxml_aux_context *context, unsigned object_index, unsigned index);

/* returns the number of copied characters (including NULL terminator) */
unsigned ijxml_aux_token_copy(struct ijxml_aux_context *context, unsigned token_index, char *buffer, unsigned buffer_size, int *err);
int ijxml_aux_token_equals(struct ijxml_aux_context *context, unsigned token_index, const char *text);

unsigned ijxml_aux_tag(struct ijxml_aux_context *context, unsigned object_index);

struct ijxml_token *ijxml_aux_token(struct ijxml_aux_context *context, unsigned token_index);
unsigned ijxml_aux_token_index(struct ijxml_aux_context *context, struct ijxml_token *token);

#endif

#if defined(IJXML_AUX_IMPLEMENTATION)

#ifdef IJXML_AUX_USE_ASSERT
	#include <assert.h>
#endif

void ijxml_aux_init(struct ijxml_aux_context *context, const char *xml, struct ijxml_token *tokens, unsigned num_tokens)
{
	context->xml = xml;
	context->tokens = tokens;
	context->num_tokens = num_tokens;
}

struct ijxml_token *ijxml_aux_token(struct ijxml_aux_context *context, unsigned token_index)
{
	if (token_index >= context->num_tokens)
		return 0;

	return &context->tokens[token_index];
}

unsigned ijxml_aux_token_index(struct ijxml_aux_context *context, struct ijxml_token *token)
{
	struct ijxml_token *tokens = context->tokens;
	unsigned token_index = (unsigned)(token-tokens);

	if (token_index >= context->num_tokens)
		return IJXML_AUX_INVALID_TOKEN_OFFSET;

	return token_index;
}

unsigned ijxml_aux_tag(struct ijxml_aux_context *context, unsigned object_index)
{
	struct ijxml_token *token;
	if (object_index+1 >= context->num_tokens)
		return IJXML_AUX_INVALID_TOKEN_OFFSET;

	token = &context->tokens[object_index];

	if (token->type != IJXML_OBJECT)
		return IJXML_AUX_INVALID_TOKEN_OFFSET;

	token = &context->tokens[object_index+1];

	return (token->type == IJXML_TAG_NAME ? (object_index+1) : IJXML_AUX_INVALID_TOKEN_OFFSET);
}

static unsigned ijxml_aux__string_len(const char *s)
{
	const char *eos = s;
	while(*eos)
		++eos;

	return (unsigned)(eos - s);
}

static int ijxml_aux__token_equals(struct ijxml_token *tok, const char *xml, const char *str)
{
	unsigned string_len = ijxml_aux__string_len(str);
	unsigned token_len = tok->end - tok->start;
	const char *token_str;

	if (string_len != token_len)
		return 0;

	token_str = &xml[tok->start];

	while (string_len) {
		if (*token_str != *str)
			return 0;

		++token_str, ++str, --string_len;
	}

	return 1;
}

int ijxml_aux_token_equals(struct ijxml_aux_context *context, unsigned token_index, const char *text)
{
	if (token_index >= context->num_tokens)
		return 0;

	return ijxml_aux__token_equals(&context->tokens[token_index], context->xml, text);
}

static void ijxml_aux__buffer_copy(char *dest, const char *source, unsigned len)
{
	while (len--)
		*dest++ = *source++;
}

unsigned ijxml_aux_token_copy(struct ijxml_aux_context *context, unsigned token_index, char *buffer, unsigned buffer_size, int *err)
{
	unsigned copy_len;
	struct ijxml_token *token;

#if defined(IJXML_AUX_USE_ASSERT)
	assert(buffer_size != 0u);
#endif

	if (token_index >= context->num_tokens) {
		if (err)
			*err = IJXML_AUX_INVALID_TOKEN_INDEX;

		return 0;
	}

	token = &context->tokens[token_index];
	copy_len = (token->end - token->start);
	if (copy_len >= buffer_size) {
		copy_len = buffer_size-1;
		if (err)
			*err = IJXML_AUX_BUFFER_TRUNCATED;
	} else if(err) {
		*err = IJXML_AUX_SUCCESS;
	}

	ijxml_aux__buffer_copy(buffer, context->xml + token->start, copy_len);
	buffer[copy_len] = '\0';

	return copy_len+1;
}

unsigned ijxml_aux_object_by_tag(struct ijxml_aux_context *context, unsigned parent_object_index, const char *tag_name)
{
	unsigned i, num_tokens;
	struct ijxml_token *tokens, *current_token;
	
	num_tokens = context->num_tokens;

	if (parent_object_index >= num_tokens)
		return IJXML_AUX_INVALID_TOKEN_OFFSET;

	tokens = context->tokens;
	current_token = &tokens[parent_object_index];

	if (current_token->type != IJXML_OBJECT)
		return IJXML_AUX_INVALID_TOKEN_OFFSET;

	for (i=parent_object_index+1; i < num_tokens; ++i) {
		current_token = &tokens[i];
		
		if (current_token->parent != parent_object_index)
			continue;

		if (current_token->type != IJXML_OBJECT)
			continue;

		if (i == num_tokens)
			return IJXML_AUX_INVALID_TOKEN_OFFSET;

		++i, ++current_token;

#if defined(IJXML_AUX_USE_ASSERT)
		assert(current_token->type == IJXML_TAG_NAME);
#endif
		if (ijxml_aux__token_equals(current_token, context->xml, tag_name))
			return (i-1);
	}

	return IJXML_AUX_INVALID_TOKEN_OFFSET;
}

unsigned ijxml_aux_object_attribute(struct ijxml_aux_context *context, unsigned object_index, const char *attribute_name)
{
	unsigned i, num_tokens;
	struct ijxml_token *tokens, *current_token;

	num_tokens = context->num_tokens;
	if (object_index >= context->num_tokens)
		return IJXML_AUX_INVALID_TOKEN_OFFSET;

	tokens = context->tokens;

	current_token = &tokens[object_index];

	if (current_token->type != IJXML_OBJECT)
		return IJXML_AUX_INVALID_TOKEN_OFFSET;

	for (i=object_index+1; i < num_tokens; ++i) {
		current_token = &tokens[i];

		if (current_token->parent != object_index)
			return IJXML_AUX_INVALID_TOKEN_OFFSET;

		if (current_token->type == IJXML_COMMENT || current_token->type == IJXML_TAG_NAME)
			continue;

		if (current_token->type != IJXML_ATTRIBUTE_KEY)
			return IJXML_AUX_INVALID_TOKEN_OFFSET;

		if (i == num_tokens)
			return IJXML_AUX_INVALID_TOKEN_OFFSET;

#if defined(IJXML_AUX_USE_ASSERT)
		assert((current_token+1)->type == IJXML_ATTRIBUTE_VALUE);
#endif
		++i; /* consume attribute value */
		if (ijxml_aux__token_equals(current_token, context->xml, attribute_name))
			return i;
	}

	return IJXML_AUX_INVALID_TOKEN_OFFSET;
}

unsigned ijxml_aux_object_at(struct ijxml_aux_context *context, unsigned object_index, unsigned index)
{
	unsigned i, n, num_seen_children;
	struct ijxml_token *object_token, *current_token;

	if (object_index >= context->num_tokens)
		return IJXML_AUX_INVALID_TOKEN_OFFSET;

	object_token = &context->tokens[object_index];

	if (object_token->type != IJXML_OBJECT)
		return IJXML_AUX_INVALID_TOKEN_OFFSET;

	n = object_token->size;

	if (index > n)
		return IJXML_AUX_INVALID_TOKEN_OFFSET;

	num_seen_children = 0;
	i = object_index;

	while (i < context->num_tokens) {
		current_token = &context->tokens[++i];

		if (current_token->type != IJXML_OBJECT)
			continue;

		if (current_token->parent != object_index)
			continue;

		if (num_seen_children++ == index)
			return i;

		if (num_seen_children == n)
			break;
	}

	return IJXML_AUX_INVALID_TOKEN_OFFSET;
}

#endif