/*
 * Unit tests for provider_base JSON parsers - no Geany dependency needed.
 * Build: gcc -I../src -I../src/providers $(pkg-config --cflags glib-2.0 libsoup-3.0 jansson) \
 *        -o test_provider_base test_provider_base.c ../src/providers/provider_base.c \
 *        ../src/providers/provider.c $(pkg-config --libs glib-2.0 libsoup-3.0 jansson)
 */
#include "../src/providers/provider_base.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_parse_openai_success(void)
{
    const char *json = "{\"choices\":[{\"message\":{\"content\":\"Hello world\"}}]}";
    gchar *result = lumila_provider_base_parse_openai(json, strlen(json));
    assert(result != NULL);
    assert(strcmp(result, "Hello world") == 0);
    g_free(result);
    printf("test_parse_openai_success: PASS\n");
}

static void test_parse_openai_error(void)
{
    const char *json = "{\"error\":{\"message\":\"Invalid API key\"}}";
    gchar *result = lumila_provider_base_parse_openai(json, strlen(json));
    assert(result != NULL);
    assert(strstr(result, "Invalid API key") != NULL);
    g_free(result);
    printf("test_parse_openai_error: PASS\n");
}

static void test_parse_openai_invalid_json(void)
{
    const char *json = "not json at all";
    gchar *result = lumila_provider_base_parse_openai(json, strlen(json));
    assert(result != NULL);
    assert(strstr(result, "JSON Parse Error") != NULL);
    g_free(result);
    printf("test_parse_openai_invalid_json: PASS\n");
}

static void test_parse_openai_empty_choices(void)
{
    const char *json = "{\"choices\":[]}";
    gchar *result = lumila_provider_base_parse_openai(json, strlen(json));
    assert(result == NULL);
    printf("test_parse_openai_empty_choices: PASS\n");
}

static void test_parse_anthropic_success(void)
{
    const char *json = "{\"content\":[{\"text\":\"Anthropic response\"}]}";
    gchar *result = lumila_provider_base_parse_anthropic(json, strlen(json));
    assert(result != NULL);
    assert(strcmp(result, "Anthropic response") == 0);
    g_free(result);
    printf("test_parse_anthropic_success: PASS\n");
}

static void test_parse_google_success(void)
{
    const char *json = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Gemini response\"}]}}]}";
    gchar *result = lumila_provider_base_parse_google(json, strlen(json));
    assert(result != NULL);
    assert(strcmp(result, "Gemini response") == 0);
    g_free(result);
    printf("test_parse_google_success: PASS\n");
}

static void test_parse_ollama_success(void)
{
    /* Ollama NDJSON - last line is the final response */
    const char *json = "{\"response\":\"part1\"}\n{\"response\":\"final answer\"}";
    gchar *result = lumila_provider_base_parse_ollama(json, strlen(json));
    assert(result != NULL);
    assert(strcmp(result, "final answer") == 0);
    g_free(result);
    printf("test_parse_ollama_success: PASS\n");
}

static void test_parse_ollama_single_line(void)
{
    const char *json = "{\"response\":\"only line\"}";
    gchar *result = lumila_provider_base_parse_ollama(json, strlen(json));
    assert(result != NULL);
    assert(strcmp(result, "only line") == 0);
    g_free(result);
    printf("test_parse_ollama_single_line: PASS\n");
}

static void test_parse_ollama_error(void)
{
    const char *json = "{\"error\":\"model not found\"}";
    gchar *result = lumila_provider_base_parse_ollama(json, strlen(json));
    assert(result != NULL);
    assert(strstr(result, "model not found") != NULL);
    g_free(result);
    printf("test_parse_ollama_error: PASS\n");
}

#if SOUP_CHECK_VERSION(3, 0, 0)
static void test_parse_stream_openai(void)
{
    const char *json = "{\"choices\":[{\"delta\":{\"content\":\"chunk\"}}]}";
    gchar *result = lumila_provider_base_parse_stream_openai(json, strlen(json));
    assert(result != NULL);
    assert(strcmp(result, "chunk") == 0);
    g_free(result);
    printf("test_parse_stream_openai: PASS\n");
}

static void test_parse_stream_openai_no_content(void)
{
    const char *json = "{\"choices\":[{\"delta\":{}}]}";
    gchar *result = lumila_provider_base_parse_stream_openai(json, strlen(json));
    assert(result == NULL);
    printf("test_parse_stream_openai_no_content: PASS\n");
}

static void test_stream_state_lifecycle(void)
{
    LumilaStreamState *state = lumila_stream_state_new(NULL, NULL, NULL, NULL);
    assert(state != NULL);
    assert(state->stream_buffer != NULL);
    assert(state->parse_chunk == NULL);
    lumila_stream_state_free(state);
    printf("test_stream_state_lifecycle: PASS\n");
}
#endif

int main(void)
{
    printf("=== Provider Base Parser Tests ===\n");
    test_parse_openai_success();
    test_parse_openai_error();
    test_parse_openai_invalid_json();
    test_parse_openai_empty_choices();
    test_parse_anthropic_success();
    test_parse_google_success();
    test_parse_ollama_success();
    test_parse_ollama_single_line();
    test_parse_ollama_error();
#if SOUP_CHECK_VERSION(3, 0, 0)
    test_parse_stream_openai();
    test_parse_stream_openai_no_content();
    test_stream_state_lifecycle();
#endif
    printf("=== All tests passed ===\n");
    return 0;
}
