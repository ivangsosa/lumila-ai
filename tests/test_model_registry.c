/*
 * Unit tests for model_registry - no Geany dependency needed.
 */
#include "../src/providers/model_registry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_registry_bounds(void)
{
    /* Valid IDs 0..lumila_model_count()-1 */
    for (gint i = 0; i < lumila_model_count(); i++) {
        const LumilaModelEntry *e = lumila_model_registry_lookup(i);
        assert(e != NULL);
        assert(e->display_name != NULL && e->display_name[0]);
        assert(e->model_name != NULL && e->model_name[0]);
        assert(e->provider_id == i);
        printf("  [%d] %s -> %s (type=%d, flags=%u)\n",
               i, e->display_name, e->model_name, e->type, e->flags);
    }
    printf("test_registry_bounds: PASS\n");
}

static void test_registry_invalid_ids(void)
{
    /* Invalid IDs should fall back to default (9 = OpenRouter Auto) */
    const LumilaModelEntry *e_neg = lumila_model_registry_lookup(-1);
    assert(e_neg != NULL);
    assert(e_neg->provider_id == 9);
    assert(strcmp(e_neg->display_name, "OpenRouter Auto") == 0);

    const LumilaModelEntry *e_big = lumila_model_registry_lookup(999);
    assert(e_big != NULL);
    assert(e_big->provider_id == 9);

    printf("test_registry_invalid_ids: PASS (fallback to OpenRouter Auto)\n");
}

static void test_registry_helpers(void)
{
    for (gint i = 0; i < lumila_model_count(); i++) {
        const gchar *dn = lumila_model_registry_get_display_name(i);
        const gchar *mn = lumila_model_registry_get_model_name(i);
        LumilaProviderType t = lumila_model_registry_get_type(i);
        const LumilaModelEntry *e = lumila_model_registry_lookup(i);
        assert(strcmp(dn, e->display_name) == 0);
        assert(strcmp(mn, e->model_name) == 0);
        assert(t == e->type);
    }
    printf("test_registry_helpers: PASS\n");
}

static void test_known_models(void)
{
    /* Oct-2026 model set (replaced in place, ids preserved) */
    assert(strcmp(lumila_model_registry_get_display_name(0), "Claude Sonnet 5.5") == 0);
    assert(lumila_model_registry_get_type(0) == LUMILA_PROVIDER_ANTHROPIC);
    assert(strcmp(lumila_model_registry_get_model_name(0), "claude-sonnet-5-5") == 0);

    assert(strcmp(lumila_model_registry_get_display_name(1), "Claude Opus 5.5") == 0);
    assert(strcmp(lumila_model_registry_get_model_name(1), "claude-opus-5-5") == 0);

    assert(strcmp(lumila_model_registry_get_display_name(2), "Claude Haiku 5.5") == 0);
    assert(strcmp(lumila_model_registry_get_model_name(2), "claude-haiku-5-5") == 0);

    assert(strcmp(lumila_model_registry_get_display_name(3), "GPT-6.1 Sol") == 0);
    assert(lumila_model_registry_get_type(3) == LUMILA_PROVIDER_OPENAI);
    assert(strcmp(lumila_model_registry_get_model_name(3), "gpt-6.1-sol") == 0);

    assert(strcmp(lumila_model_registry_get_model_name(4), "gpt-6-astra") == 0);
    assert(strcmp(lumila_model_registry_get_model_name(5), "gpt-6-luna") == 0);

    assert(strcmp(lumila_model_registry_get_model_name(6), "gemini-3.8-flash") == 0);
    assert(lumila_model_registry_get_type(6) == LUMILA_PROVIDER_GOOGLE);
    assert(strcmp(lumila_model_registry_get_model_name(8), "gemini-3.6-flash") == 0);

    assert(strcmp(lumila_model_registry_get_display_name(9), "OpenRouter Auto") == 0);
    assert(lumila_model_registry_get_type(9) == LUMILA_PROVIDER_OPENROUTER);
    assert(strcmp(lumila_model_registry_get_model_name(9), "openrouter/auto") == 0);

    /* DeepSeek canonical id is "deepseek-flash" (V4.1-Flash) */
    assert(strcmp(lumila_model_registry_get_model_name(10), "deepseek-flash") == 0);
    assert(lumila_model_registry_get_type(10) == LUMILA_PROVIDER_DEEPSEEK);
    assert(strcmp(lumila_model_registry_get_model_name(11), "deepseek-v4-pro") == 0);

    /* Mistral Large 4 preview uses the explicit id */
    assert(strcmp(lumila_model_registry_get_model_name(12), "mistral-large-4") == 0);
    assert(lumila_model_registry_get_type(12) == LUMILA_PROVIDER_MISTRAL);

    printf("test_known_models: PASS\n");
}

static void test_model_flags(void)
{
    /* Claude 5.5 models: no sampling, anthropic effort */
    const LumilaModelEntry *claude = lumila_model_registry_lookup(0);
    assert(claude->flags & LUMILA_MODEL_FLAG_NO_SAMPLING);
    assert(claude->flags & LUMILA_MODEL_FLAG_ANTHROPIC_EFFORT);
    assert(!(claude->flags & LUMILA_MODEL_FLAG_MAX_COMPLETION_TOKENS));

    /* GPT-6: no sampling + max_completion_tokens + reasoning_effort */
    for (gint i = 3; i <= 5; i++) {
        const LumilaModelEntry *e = lumila_model_registry_lookup(i);
        assert(e->flags & LUMILA_MODEL_FLAG_NO_SAMPLING);
        assert(e->flags & LUMILA_MODEL_FLAG_MAX_COMPLETION_TOKENS);
        assert(e->flags & LUMILA_MODEL_FLAG_REASONING_EFFORT);
    }

    /* Gemini 3.x: thinking level */
    assert(lumila_model_registry_lookup(6)->flags & LUMILA_MODEL_FLAG_THINKING_LEVEL);
    assert(lumila_model_registry_lookup(7)->flags & LUMILA_MODEL_FLAG_THINKING_LEVEL);

    /* Plain OpenAI-compatible models have no special flags */
    assert(lumila_model_registry_lookup(10)->flags == LUMILA_MODEL_FLAG_NONE);

    printf("test_model_flags: PASS\n");
}

static void test_find_by_model_name(void)
{
    assert(lumila_model_registry_find_by_model_name("claude-opus-5-5") == 1);
    assert(lumila_model_registry_find_by_model_name("gpt-6.1-sol") == 3);
    assert(lumila_model_registry_find_by_model_name("deepseek-flash") == 10);
    assert(lumila_model_registry_find_by_model_name("nonexistent") == -1);
    assert(lumila_model_registry_find_by_model_name("") == -1);
    assert(lumila_model_registry_find_by_model_name(NULL) == -1);

    printf("test_find_by_model_name: PASS\n");
}

static void test_config_models(void)
{
    gint before = lumila_model_count();
    gint id = lumila_model_registry_add("Custom Model", LUMILA_PROVIDER_OPENAI,
                                        "my-fine-tuned-1", LUMILA_MODEL_FLAG_NO_SAMPLING);
    assert(id == before);
    assert(lumila_model_count() == before + 1);

    const LumilaModelEntry *e = lumila_model_registry_lookup(id);
    assert(e != NULL);
    assert(strcmp(e->display_name, "Custom Model") == 0);
    assert(strcmp(e->model_name, "my-fine-tuned-1") == 0);
    assert(e->type == LUMILA_PROVIDER_OPENAI);
    assert(e->flags == LUMILA_MODEL_FLAG_NO_SAMPLING);

    /* find_by_model_name covers config models too */
    assert(lumila_model_registry_find_by_model_name("my-fine-tuned-1") == id);

    /* Flags parsed from JSON */
    json_t *arr = json_pack("[ssss]", "no_sampling", "reasoning_effort",
                            "unknown_flag", "thinking_level");
    guint flags = lumila_model_flags_from_json(arr);
    json_decref(arr);
    assert(flags & LUMILA_MODEL_FLAG_NO_SAMPLING);
    assert(flags & LUMILA_MODEL_FLAG_REASONING_EFFORT);
    assert(flags & LUMILA_MODEL_FLAG_THINKING_LEVEL);
    assert(!(flags & LUMILA_MODEL_FLAG_ANTHROPIC_EFFORT));
    assert(lumila_model_flags_from_json(NULL) == LUMILA_MODEL_FLAG_NONE);

    printf("test_config_models: PASS\n");
}

int main(void)
{
    printf("=== Model Registry Tests ===\n");
    test_registry_bounds();
    test_registry_invalid_ids();
    test_registry_helpers();
    test_known_models();
    test_model_flags();
    test_find_by_model_name();
    test_config_models();
    printf("=== All tests passed ===\n");
    return 0;
}
