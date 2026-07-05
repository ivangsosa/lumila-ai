/*
 * Unit tests for model_registry - no Geany dependency needed.
 * Build: gcc -I../src -I../src/providers $(pkg-config --cflags glib-2.0 jansson) \
 *        -o test_model_registry test_model_registry.c ../src/providers/model_registry.c \
 *        ../src/providers/provider.c $(pkg-config --libs glib-2.0 jansson)
 */
#include "../src/providers/model_registry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_registry_bounds(void)
{
    /* Valid IDs 0..LUMILA_NUM_PROVIDERS-1 */
    for (gint i = 0; i < LUMILA_NUM_PROVIDERS; i++) {
        const LumilaModelEntry *e = lumila_model_registry_lookup(i);
        assert(e != NULL);
        assert(e->display_name != NULL && e->display_name[0]);
        assert(e->model_name != NULL && e->model_name[0]);
        assert(e->provider_id == i);
        printf("  [%d] %s -> %s (type=%d)\n", i, e->display_name, e->model_name, e->type);
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
    /* get_display_name / get_model_name / get_type should be consistent */
    for (gint i = 0; i < LUMILA_NUM_PROVIDERS; i++) {
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
    /* Spot-check a few known entries to catch regressions */
    assert(strcmp(lumila_model_registry_get_display_name(0), "Claude Sonnet 4") == 0);
    assert(lumila_model_registry_get_type(0) == LUMILA_PROVIDER_ANTHROPIC);

    assert(strcmp(lumila_model_registry_get_display_name(9), "OpenRouter Auto") == 0);
    assert(lumila_model_registry_get_type(9) == LUMILA_PROVIDER_OPENROUTER);

    assert(strcmp(lumila_model_registry_get_display_name(10), "DeepSeek V3") == 0);
    assert(lumila_model_registry_get_type(10) == LUMILA_PROVIDER_DEEPSEEK);

    /* Verify the bug fix: ID 8 is Ollama Qwen3, NOT OpenRouter */
    assert(strcmp(lumila_model_registry_get_display_name(8), "Ollama Qwen3") == 0);
    assert(lumila_model_registry_get_type(8) == LUMILA_PROVIDER_OLLAMA);

    printf("test_known_models: PASS\n");
}

int main(void)
{
    printf("=== Model Registry Tests ===\n");
    test_registry_bounds();
    test_registry_invalid_ids();
    test_registry_helpers();
    test_known_models();
    printf("=== All tests passed ===\n");
    return 0;
}
