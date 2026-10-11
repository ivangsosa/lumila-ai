# Lumila AI - Plugin para Geany v0.11.0

AI Assistant plugin for Geany editor with multi-provider support, persistent history, and real-time streaming.

## Features

- **Panel lateral de chat** con UI moderna oscura
  - Fondo moderno (`#0f0f23`) con mensajes estilo burbujas
  - Mensajes del usuario alineados a la derecha (azul `#82AAFF`)
  - Respuestas de la IA alineadas a la izquierda (blanco azulado `#C8D3F5`)
  - **Bloques de código separados** con fondo oscuro (`#16162a`), márgenes y separación visual
  - **Syntax highlighting inteligente** con detección automática de lenguaje (20+ lenguajes soportados)
- **Edición de archivos por IA**: la IA puede modificar archivos abiertos directamente
  - Usa el formato ` ```file:nombre.ext ` para editar (soporta rutas como `src/foo.c` y contenido con fences anidados)
  - Crea archivos nuevos si no existen (con filetype detectado por extensión)
  - **Confirmación antes de aplicar** (Apply/Discard, desactivable con `confirm_edits`)
  - Preserva el historial de undo del editor
  - Resumen de cambios en el chat
- **Streaming de respuestas** (todos los providers con libsoup-3.0): texto en tiempo real
  - Errores HTTP (401/429/5xx) visibles con el mensaje real de la API
  - Cancelar con **Esc** conserva el texto parcial y deja el provider listo para reenviar
  - **Retry automático con backoff** en errores transitorios (429/5xx; desactivable con `auto_retry`)
- **Modo Plan**: la IA genera un plan antes de editar; **Execute Plan** lo aplica
- **Contador de tokens**: estimación (chars/4) durante el envío; uso real cuando el provider lo reporta
- **Botón Copy** en cada bloque de código del chat
- **Modelos declarados en `config.json`** (`models[]`): agregá modelos al combo sin recompilar
- **Keybindings asignables**: *Lumila: Send selection to AI* y *Lumila: Send current file to AI* en Preferencias → Combinaciones de teclas
- **Slash commands** rápidos: `/explain`, `/refactor`, `/test`, `/doc`, `/fix`, `/commit`, `/review`
- **Send Selection**: envía el texto seleccionado en el editor como contexto
- **Send Current File**: envía el archivo activo completo como contexto
- **@referencias en el chat**: escribí `@main.c` en el input para incluir cualquier archivo como contexto
- **Modo Ask**: toggle que desactiva la edición de archivos para consultas rápidas sin modificaciones (persiste entre sesiones)
- **Exportar a Markdown**: guarda la conversación actual como `.md`
- **Modelos custom** vía `config.json`: sobrescribe el modelo de cualquier provider
- **Endpoints y timeouts configurables** vía `config.json`: sobrescribe la URL base y el timeout de cada provider (útil para proxies o instancias self-hosted de Ollama)
- **Historial de conversaciones persistente**
  - Guardado automático en `~/.config/geany/plugins/lumila-ai/history/`
  - Botón **History** para listar, continuar o eliminar conversaciones
  - **Auto-back to chat**: al presionar **Continue**, vuelve automáticamente al chat sin clickear "Back to Chat"
  - **Persistencia de títulos**: los títulos se guardan en el JSON del historial y se restauran al cargar
  - **Fuente pequeña** en la lista de historial para leer títulos completos sin estirar el panel
  - **Confirmación de eliminación**: diálogo modal con "Yes/No" antes de borrar una conversación
  - Contexto multi-mensaje enviado a los modelos
- **Atajos de teclado**
  - **Enter**: enviar mensaje
  - **Shift+Enter**: nueva línea en el input
  - **Ctrl+Enter**: también envía mensaje (compatibilidad)
  - **Escape**: cancelar request activo
  - Focus automático al input después de enviar/cancelar
- **Configuraciones avanzadas**: temperatura, max_tokens, top_p, repeat_penalty

## Modelos soportados (Oct-2026)

| # | Modelo                       | Proveedor   | Tipo          |
|---|------------------------------|-------------|---------------|
| 0 | Claude Sonnet 5.5            | Anthropic   | Pago          |
| 1 | Claude Opus 5.5              | Anthropic   | Pago          |
| 2 | Claude Haiku 5.5             | Anthropic   | Pago          |
| 3 | GPT-6.1 Sol                  | OpenAI      | Pago          |
| 4 | GPT-6 Astra                  | OpenAI      | Pago          |
| 5 | GPT-6 Luna                   | OpenAI      | Pago          |
| 6 | Gemini 3.8 Flash             | Google      | Pago          |
| 7 | Gemini 3.1 Pro Preview       | Google      | Pago          |
| 8 | Gemini 3.6 Flash             | Google      | Pago          |
| 9 | OpenRouter Auto              | OpenRouter  | Gratis        |
| 10 | DeepSeek V4.1 Flash         | DeepSeek    | Pago          |
| 11 | DeepSeek V4 Pro             | DeepSeek    | Pago          |
| 12 | Mistral Large 4 (preview)   | Mistral     | Pago          |
| 13 | Mistral Small               | Mistral     | Pago          |
| 14 | Ollama Llama 4 Scout        | Ollama      | Local/Gratis  |
| 15 | Ollama Qwen3.6 27B          | Ollama      | Local/Gratis  |
| 16 | Ollama Mistral Small 3.1    | Ollama      | Local/Gratis  |
| 17 | Moonshot Kimi K3            | Moonshot    | Pago          |
| 18 | OpenRouter GLM-4.6          | OpenRouter  | Pago          |
| 19 | OpenRouter Grok 4           | OpenRouter  | Pago          |
| 20 | OpenRouter Qwen3.5 Plus     | OpenRouter  | Pago          |
| 21 | OpenRouter Llama 3.3 Free   | OpenRouter  | Gratis        |

Cada modelo lleva flags de capacidad que ajustan el request automáticamente:
los modelos con razonamiento no envían `temperature`/`top_p`, los GPT-6 usan
`max_completion_tokens` + `reasoning_effort`, los Claude 5.5 usan
`output_config.effort` y los Gemini 3.x usan `thinkingConfig.thinkingLevel`.

## Dependencies

### Kubuntu 2026.04 / Ubuntu 24.04+

```bash
sudo apt install build-essential autoconf automake libtool pkg-config
sudo apt install libgtk-3-dev libglib2.0-dev
sudo apt install geany geany-plugins-common
sudo apt install libsoup-3.0-dev libjansson-dev
```

### Manjaro / Arch Linux

```bash
sudo pacman -S base-devel autoconf automake libtool pkgconf
sudo pacman -S geany gtk3 glib2
sudo pacman -S libsoup3 jansson
```

### Fedora

```bash
sudo dnf install geany geany-devel gtk3-devel glib2-devel
sudo dnf install libsoup3-devel jansson-devel
```

**Nota:** Si `libsoup-3.0` no está disponible, el `configure` usará `libsoup-2.4` como fallback. El streaming de respuestas requiere libsoup-3.0.

## Compilation

```bash
./bootstrap.sh
./configure
make

# Opción recomendada: instalación manual (sin root)
mkdir -p ~/.config/geany/plugins
cp src/.libs/lumila-ai.so ~/.config/geany/plugins/
```

## Configuration

El archivo `~/.config/geany/plugins/lumila-ai/config.json` se crea automáticamente al iniciar. Usa `config-example.json` como plantilla.

```json
{
  "version": "0.11.0",
  "api_keys": {
    "openai": "",
    "anthropic": "",
    "google": "",
    "moonshot": "",
    "openrouter": "",
    "ollama": "",
    "deepseek": "",
    "mistral": ""
  },
  "defaults": {
    "temperature": 0.3,
    "max_tokens": 8192,
    "top_p": 0.9,
    "repeat_penalty": 1.1,
    "default_provider_id": 9,
    "default_model": "",
    "ask_mode": false,
    "plan_mode": false,
    "confirm_edits": true,
    "auto_retry": true,
    "context_max_chars": 20000,
    "history_max_messages": 40,
    "reasoning_effort": "medium",
    "thinking_level": "medium"
  },
  "custom_models": {
    "openai": "",
    "anthropic": "",
    "google": "",
    "moonshot": "",
    "openrouter": "",
    "ollama": "",
    "deepseek": "",
    "mistral": ""
  },
  "endpoints": {
    "openai": "",
    "anthropic": "",
    "google": "",
    "moonshot": "",
    "openrouter": "",
    "ollama": "",
    "deepseek": "",
    "mistral": ""
  },
  "timeouts": {
    "openai": 180,
    "anthropic": 180,
    "google": 180,
    "moonshot": 180,
    "openrouter": 180,
    "ollama": 120,
    "deepseek": 180,
    "mistral": 180
  },
  "models": [
    {
      "name": "Mi modelo local",
      "provider": "ollama",
      "model": "my-model:latest",
      "flags": []
    }
  ]
}
```

### Parámetros

- **temperature**: aleatoriedad (0.0 - 2.0, default 0.3). Se omite automáticamente en modelos que no lo soportan
- **max_tokens**: máximo de tokens de salida (default 8192 — los modelos con razonamiento gastan parte del presupuesto en *thinking*)
- **top_p**: muestreo nucleus (0.0 - 1.0, default 0.9). Omitido cuando el modelo no lo soporta
- **repeat_penalty**: penalización de repetición (default 1.1)
- **default_provider_id**: modelo por defecto por índice (legacy; default 9 = OpenRouter Auto)
- **default_model**: modelo por defecto por nombre de API (ej. `"claude-sonnet-5-5"`). Tiene prioridad sobre `default_provider_id`
- **ask_mode** / **plan_mode**: modo persistido al iniciar
- **confirm_edits**: pide confirmación antes de aplicar bloques ` ```file: ` (default true)
- **auto_retry**: reintenta errores transitorios 429/5xx/529 con backoff 1/2/4 s (default true)
- **context_max_chars**: presupuesto de caracteres para el contexto de archivos abiertos (default 20000)
- **history_max_messages**: máximo de mensajes de historial enviados por request (default 40, 0 = sin límite)
- **reasoning_effort**: `"low"|"medium"|"high"|"xhigh"|"max"` para GPT-6 y Claude 5.5 (default `"medium"`)
- **thinking_level**: `"low"|"medium"|"high"` para Gemini 3.x (default `"medium"`)
- **endpoints**: sobrescribe la URL base de cada provider (vacío = usar default). Útil para proxies o instancias self-hosted de Ollama
- **timeouts**: timeout en segundos por provider (default 180s, Ollama 120s — los modelos con razonamiento pueden tardar antes del primer token)
- **models**: lista de modelos extra que aparecen en el combo. Cada entrada: `{"name": ..., "provider": ..., "model": ..., "flags": [...]}` con flags opcionales `no_sampling`, `max_completion_tokens`, `reasoning_effort`, `anthropic_effort`, `thinking_level`

### API Keys

| Proveedor         | URL                                         |
|-------------------|---------------------------------------------|
| Anthropic Claude  | https://console.anthropic.com/settings/keys |
| OpenAI            | https://platform.openai.com/api-keys        |
| Google Gemini     | https://makersuite.google.com/app/apikey    |
| DeepSeek          | https://platform.deepseek.com/api_keys      |
| Mistral           | https://console.mistral.ai/api-keys         |
| OpenRouter        | https://openrouter.ai/keys                  |
| Ollama            | Local (sin key) — http://localhost:11434    |

## Usage

1. Compila e instala el plugin
2. Configura tus API keys en `config.json`
3. Reinicia Geany completamente
4. Activa **Lumila AI** en *Herramientas → Administrador de complementos*
5. El panel lateral **Lumila** aparecerá en la barra lateral
6. Selecciona un modelo y escribe tu mensaje
7. Presiona **Send** o **Enter** para enviar. **Shift+Enter** para nueva línea

### Historial

- Clic en **History** para ver conversaciones guardadas
- **Continue**: carga una conversación anterior en el chat activo y vuelve automáticamente al chat
- **Delete**: elimina una conversación del historial (con confirmación previa)
- **Búsqueda**: filtrá conversaciones por título, nombre de archivo o modelo desde el campo de búsqueda en la parte superior
- Los títulos se guardan en el JSON y se restauran al recargar, sin perderse al reiniciar Geany
- Las conversaciones se guardan automáticamente al crear una nueva o cerrar Geany

### Slash Commands

Escribí un comando rápido en el input para transformar tu mensaje. Los comandos `/explain`, `/refactor`, `/test` y `/doc` incluyen automáticamente el archivo activo como contexto:

- `/explain` — Explica el código del archivo activo
- `/refactor` — Refactoriza para mejorar legibilidad
- `/test` — Genera tests unitarios
- `/doc` — Genera documentación
- `/fix` — Encuentra y corrige bugs en el archivo activo; si puede, adjunta la salida real del compilador/linter (gcc/g++/python/node/bash/php)
- `/commit` — Genera un mensaje de commit convencional desde `git diff` (ejecutado en el directorio del proyecto)
- `/review` — Revisa el archivo activo buscando bugs, seguridad y performance

Ejemplo: `/explain esta función`

### Send Selection

Seleccioná texto en el editor y presioná **Send Selection**. El plugin enviará solo esa porción como contexto, sin necesidad de copiar y pegar.

### Send Current File

Presioná **Send File** para enviar el contenido completo del archivo activo como contexto. Útil cuando necesitás que la IA vea todo el archivo, no solo una selección.

### @referencias en el chat

Incluí cualquier archivo del proyecto escribiendo `@nombre.ext` en el input del chat. El plugin busca el archivo relativo al directorio del proyecto Geany, luego del documento activo, y también acepta rutas absolutas y `~/...`:

```
Revisá @utils.c y decime si hay memory leaks
```

Funciona con rutas relativas (`@src/main.c`) o nombres de archivo simples (`@main.c`). Los archivos binarios se ignoran y hay un límite de ~20 KB por archivo. Por seguridad, rutas sensibles (`~/.ssh`, `~/.gnupg`, `.env`, etc.) se omiten con un aviso — nunca se envían al proveedor.

### Modos Ask y Plan

El botón de modo (abajo a la izquierda) ofrece tres estados que persisten entre sesiones:

- **Code** (default): la IA puede responder y editar archivos con ` ```file: `
- **Ask**: responde **sin modificar archivos** — ideal para consultas rápidas sin riesgo
- **Plan**: la IA genera un plan paso a paso *sin* tocar archivos; cuando el plan te convenga, usá **... → Execute Plan** para implementarlo (las ediciones pasan por la confirmación normal)

### Syntax Highlighting

El plugin detecta automáticamente el lenguaje en los bloques de código (` ```lang `) y aplica resaltado de sintaxis con fondo oscuro, márgenes y colores específicos:

| Lenguaje | Detecta (` ``` `) | Características |
|---|---|---|
| **Python** | `python`, `py` | Keywords, `#` comentarios, strings triples |
| **JavaScript/TypeScript** | `javascript`, `js`, `typescript`, `ts` | Keywords, `//`, `/* */`, template literals `` ` `` |
| **C/C++** | `c`, `cpp`, `c++`, `cxx` | Keywords, `//`, `/* */`, números hex/bin |
| **Rust** | `rust`, `rs` | Keywords, `//`, `/* */`, macros `!` |
| **Go** | `go`, `golang` | Keywords, raw strings `` ` `` |
| **PHP** | `php` | Keywords, `//`, `#`, `/* */`, `$variables` |
| **HTML/XML** | `html`, `htm`, `xml` | Tags `< >`, `<!-- -->` comentarios |
| **CSS** | `css` | Propiedades, `//`, `/* */`, colores |
| **Bash/Shell** | `bash`, `sh`, `zsh`, `shell` | Keywords, `#` comentarios, `$VAR`, backticks |
| **SQL** | `sql` | 100+ keywords, `--` comentarios, `/* */` |
| **Java** | `java` | Keywords, `//`, `/* */` |
| **Ruby** | `ruby`, `rb` | Keywords, `#`, `=begin` comentarios |
| **JSON** | `json` | Literales `true`, `false`, `null` |
| **Markdown** | `markdown`, `md` | Sin resaltado específico |
| **YAML/TOML/Dockerfile/Makefile** | `yaml`, `yml`, `toml`, `dockerfile`, `makefile` | Keywords genéricos, `#` comentarios |

**Características del lexer:**
- Comentarios de línea (`//`, `#`, `--`) y multilínea (`/* */`, `<!-- -->`, `=begin/end`)
- Strings: `""`, `''`, template literals `` ` ``, raw strings
- Números: enteros, flotantes, hex (`0xFF`), octal (`0o755`), binario (`0b1010`), notación científica (`1.5e-10`)
- Variables shell: `$VAR`, `${VAR}`
- HTML tags completos: `<div class="...">`
- CSS properties detectadas: `propiedad: valor;`

### Exportar conversación

Clic en **Export MD** para guardar el chat actual como archivo Markdown en `~/.config/geany/plugins/lumila-ai/exports/`.

### Edición de archivos

Pedile a la IA que modifique un archivo. Responderá con bloques:

```file:main.c
#include <stdio.h>
int main() { return 0; }
```

El plugin detecta estos bloques y:
- Si `confirm_edits` está activo: **muestra un diálogo Apply/Discard** con la lista de archivos y líneas afectadas
- Si el archivo está abierto: **reemplaza su contenido** (preservando undo)
- Si existe en disco pero no está abierto: lo abre y reemplaza
- Si no existe: **crea un nuevo archivo** con el filetype correcto

Los bloques ` ```file: ` soportan rutas (`src/foo.c`) y contenido que contiene otros code fences (el parser rastrea la profundidad).

### Modelos custom

Agregá un modelo propio en `config.json` sin recompilar:

```json
"custom_models": {
  "openai": "gpt-4-turbo",
  "ollama": "codellama:13b"
}
```

Dejá vacío (`""`) para usar el modelo por defecto del provider.

## Roadmap

### Medias (requieren trabajo adicional)

- [ ] **Contexto multi-archivo automático**: al enviar un mensaje, incluir automáticamente el archivo activo + archivos recientes + archivos del proyecto (`*.geany`)
- [ ] **Menú contextual en el editor**: integrar acciones al menú derecho de Geany — *Explain this*, *Refactor selection*, *Generate docstring*, *Add type hints* (los keybindings de v0.11 cubren parte de esto)
- [ ] **Ejecución de comandos**: detectar bloques ` ```bash ` y mostrar botón *Run* que ejecute el comando en el directorio del proyecto (`g_spawn_async`)
- [ ] **Previsualización diff inline**: mostrar cambios propuestos como anotaciones de Scintilla (verde/rojo) con botones *Apply* / *Discard* antes de modificar el archivo
- [ ] **Refresh dinámico de modelos**: poblar el combo desde `/v1/models` (OpenAI-compatibles) y `/api/tags` (Ollama)

### Avanzadas (diferenciadoras, mayor esfuerzo)

- [ ] **Índice de proyecto (vector search local)**: indexar archivos del proyecto (TF-IDF o embeddings livianos) para encontrar automáticamente definiciones relevantes al preguntar "¿dónde se define X?"
- [ ] **Modo Agent / Composer**: la IA puede leer múltiples archivos, proponer cambios, pedir confirmación y aplicarlos en pasos iterativos
- [ ] **Contexto LSP**: integrar con `geany-lsp` para capturar diagnostics (errores de tipo, lint) y enviarlos como contexto
- [ ] **Autocompletado con IA**: sugerencias inline mientras se escribe (estilo Copilot) vía hooks de Scintilla (`char-added` + ghost text)

### Infraestructura

- [ ] **Tests de integración**: mock de providers, tests end-to-end con automatización de UI (dogtail o similar)
- [ ] **Build multiplataforma**: compilar y distribuir binarios para Linux (AppImage/deb/rpm), Windows (MSYS2/MinGW), macOS (Homebrew)

## Troubleshooting

**Error: "API key not configured"**
- Verifica que `config.json` existe y contiene la key del proveedor seleccionado
- Reinicia Geany después de editar el config

**El plugin no aparece en el administrador**
- Verifica que `lumila-ai.so` está en `~/.config/geany/plugins/`
- Como alternativa: `sudo make install`

**Streaming no funciona**
- Requiere `libsoup-3.0`. Si tienes `libsoup-2.4`, las respuestas llegarán completas al final

### Actualización del plugin

Para actualizar sin perder tu configuración (`config.json`) ni historial:

```bash
# 1. Cerrá Geany completamente (obligatorio, el .so está bloqueado en memoria)
killall geany

# 2. Descargá la última versión
wget https://github.com/usuario/lumila-ai/releases/latest/download/lumila-ai.so \
  -O ~/.config/geany/plugins/lumila-ai/lumila-ai.so.new

# 3. Hacé backup y reemplazá
mv ~/.config/geany/plugins/lumila-ai.so ~/.config/geany/plugins/lumila-ai.so.bak
mv ~/.config/geany/plugins/lumila-ai/lumila-ai.so.new ~/.config/geany/plugins/lumila-ai.so

# 4. Reiniciá Geany
geany &
```

> **Nota:** El botón **Check for Updates** en el panel de Lumila muestra estas instrucciones directamente en el editor.

## License

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
