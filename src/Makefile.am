plugin_LTLIBRARIES = lumila-ai.la

lumila_ai_la_SOURCES = \
	main.c \
	plugin.h \
	sidebar.c \
	sidebar.h \
	chat.c \
	chat.h \
	chat_ui.c \
	chat_ui.h \
	history.c \
	history.h \
	history_ui.c \
	history_ui.h \
	config.c \
	config.h \
	providers/provider.c \
	providers/provider.h \
	providers/model_registry.c \
	providers/model_registry.h \
	providers/openai.c \
	providers/openai.h \
	providers/anthropic.c \
	providers/anthropic.h \
	providers/google.c \
	providers/google.h \
	providers/ollama.c \
	providers/ollama.h \
	providers/openrouter.c \
	providers/openrouter.h \
	providers/deepseek.c \
	providers/deepseek.h \
	providers/mistral.c \
	providers/mistral.h \
	utils.c \
	utils.h

lumila_ai_la_CFLAGS = \
	$(GEANY_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(SOUP_CFLAGS) \
	$(JANSSON_CFLAGS) \
	-fPIC \
	-Wall \
	-Wextra \
	-O2

lumila_ai_la_LDFLAGS = \
	-module \
	-avoid-version \
	-shared \
	$(GEANY_LIBS) \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(SOUP_LIBS) \
	$(JANSSON_LIBS)

plugindir = $(PLUGIN_DIR)
