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
    const char *json = "{\"content\":[{\"type\":\"text\",\"text\":\"Anthropic response\"}]}";
    gchar *result = lumila_provider_base_parse_anthropic(json, strlen(json));
    assert(result != NULL);
    assert(strcmp(result, "Anthropic response") == 0);
    g_free(result);
    printf("test_parse_anthropic_success: PASS\n");
}

static void test_parse_anthropic_thinking_block(void)
{
    /* Claude 5.5 with adaptive thinking returns a thinking block first;
     * only "text" blocks must be extracted. */
    const char *json =
        "{\"content\":["
        "{\"type\":\"thinking\",\"thinking\":\"Let me reason...\"},"
        "{\"type\":\"text\",\"text\":\"First part \"},"
        "{\"type\":\"thinking\",\"thinking\":\"more\"},"
        "{\"type\":\"text\",\"text\":\"Second part\"}]}";
    gchar *result = lumila_provider_base_parse_anthropic(json, strlen(json));
    assert(result != NULL);
    assert(strcmp(result, "First part Second part") == 0);
    assert(strstr(result, "reason") == NULL);
    g_free(result);
    printf("test_parse_anthropic_thinking_block: PASS\n");
}

static void test_parse_anthropic_max_tokens(void)
{
    const char *json =
        "{\"content\":[{\"type\":\"text\",\"text\":\"Partial\"}],"
        "\"stop_reason\":\"max_tokens\"}";
    gchar *result = lumila_provider_base_parse_anthropic(json, strlen(json));
    assert(result != NULL);
    assert(strstr(result, "Partial") != NULL);
    assert(strstr(result, "truncated") != NULL);
    g_free(result);
    printf("test_parse_anthropic_max_tokens: PASS\n");
}

static void test_parse_anthropic_error(void)
{
    const char *json =
        "{\"type\":\"error\",\"error\":{\"type\":\"overloaded_error\","
        "\"message\":\"Overloaded\"}}";
    gchar *result = lumila_provider_base_parse_anthropic(json, strlen(json));
    assert(result != NULL);
    assert(strstr(result, "Overloaded") != NULL);
    g_free(result);
    printf("test_parse_anthropic_error: PASS\n");
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

static void test_parse_google_multi_parts(void)
{
    /* Gemini thinking models emit a thought part first; all text parts
     * must be concatenated and thought parts skipped. */
    const char *json =
        "{\"candidates\":[{\"content\":{\"parts\":["
        "{\"text\":\"ignored\",\"thought\":true},"
        "{\"text\":\"Hello \"},"
        "{\"text\":\"world\"}]}}]}";
    gchar *result = lumila_provider_base_parse_google(json, strlen(json));
    assert(result != NULL);
    assert(strcmp(result, "Hello world") == 0);
    g_free(result);
    printf("test_parse_google_multi_parts: PASS\n");
}

static void test_parse_google_blocked(void)
{
    const char *json =
        "{\"promptFeedback\":{\"blockReason\":\"SAFETY\"}}";
    gchar *result = lumila_provider_base_parse_google(json, strlen(json));
    assert(result != NULL);
    assert(strstr(result, "SAFETY") != NULL);
    g_free(result);
    printf("test_parse_google_blocked: PASS\n");
}

static void test_parse_ollama_success(void)
{
    /* Ollama NDJSON - concatenate all "response" chunks */
    const char *json = "{\"response\":\"part1\"}\n{\"response\":\"part2\"}\n{\"response\":\"\"}";
    gchar *result = lumila_provider_base_parse_ollama(json, strlen(json));
    assert(result != NULL);
    assert(strcmp(result, "part1part2") == 0);
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

static void test_retriable_status(void)
{
    /* 429 and 5xx are retriable; also Anthropic's 529 overload. */
    assert(lumila_provider_base_is_retriable_status(429));
    assert(lumila_provider_base_is_retriable_status(500));
    assert(lumila_provider_base_is_retriable_status(502));
    assert(lumila_provider_base_is_retriable_status(503));
    assert(lumila_provider_base_is_retriable_status(504));
    assert(lumila_provider_base_is_retriable_status(529));
    assert(!lumila_provider_base_is_retriable_status(200));
    assert(!lumila_provider_base_is_retriable_status(401));
    assert(!lumila_provider_base_is_retriable_status(400));
    assert(!lumila_provider_base_is_retriable_status(404));
    printf("test_retriable_status: PASS\n");
}

static void test_extract_error_message(void)
{
    /* OpenAI shape */
    gchar *e1 = lumila_provider_base_extract_error_message(
        "{\"error\":{\"message\":\"Invalid key\"}}", 38);
    assert(e1 && strstr(e1, "Invalid key"));
    g_free(e1);

    /* Anthropic shape (top-level "type":"error") */
    const char *a = "{\"type\":\"error\",\"error\":{\"message\":\"Overloaded\"}}";
    gchar *e2 = lumila_provider_base_extract_error_message(a, strlen(a));
    assert(e2 && strstr(e2, "Overloaded"));
    g_free(e2);

    /* No error -> NULL */
    const char *ok = "{\"choices\":[]}";
    assert(lumila_provider_base_extract_error_message(ok, strlen(ok)) == NULL);

    printf("test_extract_error_message: PASS\n");
}

static void test_parse_usage(void)
{
    /* OpenAI */
    const char *o = "{\"choices\":[{\"message\":{\"content\":\"x\"}}],"
                    "\"usage\":{\"prompt_tokens\":10,\"completion_tokens\":5,"
                    "\"total_tokens\":15}}";
    assert(lumila_provider_base_parse_usage_total_tokens(o, strlen(o)) == 15);

    /* Anthropic: input + output */
    const char *a = "{\"usage\":{\"input_tokens\":20,\"output_tokens\":8}}";
    assert(lumila_provider_base_parse_usage_total_tokens(a, strlen(a)) == 28);

    /* Google */
    const char *g = "{\"usageMetadata\":{\"totalTokenCount\":42}}";
    assert(lumila_provider_base_parse_usage_total_tokens(g, strlen(g)) == 42);

    /* No usage -> -1 */
    const char *n = "{\"choices\":[]}";
    assert(lumila_provider_base_parse_usage_total_tokens(n, strlen(n)) == -1);

    printf("test_parse_usage: PASS\n");
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
    test_parse_anthropic_thinking_block();
    test_parse_anthropic_max_tokens();
    test_parse_anthropic_error();
    test_parse_google_success();
    test_parse_google_multi_parts();
    test_parse_google_blocked();
    test_parse_ollama_success();
    test_parse_ollama_single_line();
    test_parse_ollama_error();
#if SOUP_CHECK_VERSION(3, 0, 0)
    test_parse_stream_openai();
    test_parse_stream_openai_no_content();
    test_stream_state_lifecycle();
    test_retriable_status();
    test_extract_error_message();
    test_parse_usage();
#endif
    printf("=== All tests passed ===\n");
    return 0;
}
