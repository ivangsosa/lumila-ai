# Lumila AI - Plugin para Geany v0.9.3

AI Assistant plugin for Geany editor with multi-provider support, persistent history, and real-time streaming.

## Features

- **Panel lateral de chat** con UI moderna oscura
  - Fondo moderno (`#0f0f23`) con mensajes estilo burbujas
  - Mensajes del usuario alineados a la derecha (azul `#82AAFF`)
  - Respuestas de la IA alineadas a la izquierda (blanco azulado `#C8D3F5`)
  - **Bloques de código separados** con fondo oscuro (`#16162a`), márgenes y separación visual
  - **Syntax highlighting inteligente** con detección automática de lenguaje (20+ lenguajes soportados)
- **Edición de archivos por IA**: la IA puede modificar archivos abiertos directamente
  - Usa el formato ` ```file:nombre.ext ` para editar
  - Crea archivos nuevos si no existen
  - Resumen de cambios en el chat
- **Streaming de respuestas** (todos los providers con libsoup-3.0): texto en tiempo real
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

## Modelos soportados (2025-2026)

| # | Modelo                 | Proveedor   | Tipo          |
|---|------------------------|-------------|---------------|
| 0 | Claude Sonnet 4        | Anthropic   | Pago          |
| 1 | Claude Opus 4          | Anthropic   | Pago          |
| 2 | GPT-4.1                | OpenAI      | Pago          |
| 3 | GPT-4.1 mini           | OpenAI      | Pago          |
| 4 | Gemini 2.5 Flash       | Google      | Pago          |
| 5 | Gemini 2.5 Pro         | Google      | Pago          |
| 6 | Gemma 4 12B            | Google      | Pago          |
| 7 | Ollama Llama 3.3       | Ollama      | Local/Gratis  |
| 8 | Ollama Qwen3           | Ollama      | Local/Gratis  |
| 9 | OpenRouter Auto        | OpenRouter  | Gratis        |
| 10 | DeepSeek V3           | DeepSeek    | Pago          |
| 11 | DeepSeek R1           | DeepSeek    | Pago          |
| 12 | Mistral Large         | Mistral     | Pago          |
| 13 | Ollama Mistral Small  | Ollama      | Local/Gratis  |
| 14 | OpenRouter Free       | OpenRouter  | Gratis        |
| 15 | Moonshot Kimi K2.6    | Moonshot    | Pago          |
| 16 | MAI-Code-1            | OpenRouter  | Pago          |
| 17 | GPT-4.1 nano          | OpenAI      | Pago          |
| 18 | Mistral Small 3.1     | Mistral     | Pago          |
| 19 | OpenRouter GLM-4      | OpenRouter  | Pago          |
| 20 | OpenRouter Grok 3     | OpenRouter  | Pago          |
| 21 | OpenRouter Qwen3-235B | OpenRouter  | Pago          |

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
  "version": "0.9.3",
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
    "max_tokens": 2048,
    "top_p": 0.9,
    "repeat_penalty": 1.1,
    "default_provider_id": 9
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
    "openai": 60,
    "anthropic": 60,
    "google": 60,
    "moonshot": 60,
    "openrouter": 60,
    "ollama": 120,
    "deepseek": 60,
    "mistral": 60
  }
}
```

### Parámetros

- **temperature**: aleatoriedad (0.0 - 2.0, default 0.3)
- **max_tokens**: máximo de tokens (100 - 8000, default 2048)
- **top_p**: muestreo nucleus (0.0 - 1.0, default 0.9)
- **repeat_penalty**: penalización de repetición (default 1.1)
- **default_provider_id**: modelo por defecto (0-21, default 9 = OpenRouter Auto)
- **endpoints**: sobrescribe la URL base de cada provider (vacío = usar default). Útil para proxies o instancias self-hosted de Ollama
- **timeouts**: timeout en segundos por provider (default 60s, Ollama 120s)

### Configuraciones sugeridas por modelo

| Escenario | `temperature` | `max_tokens` | `top_p` | `repeat_penalty` | `default_provider_id` |
|---|---|---|---|---|---|
| **DeepSeek V3** (rápido, barato, bueno para código) | 0.3 | 2048 | 0.9 | 1.1 | 10 |
| **DeepSeek R1** (raciocinio profundo) | 0.2 | 4096 | 0.95 | 1.1 | 11 |
| **Ollama Qwen3:8b** (local, ligero) | 0.5 | 2048 | 0.9 | 1.05 | 8 |
| **OpenRouter Auto** (gratis, enrutamiento automático) | 0.3 | 2048 | 0.9 | 1.1 | 9 |
| **Genérica** (Claude, GPT-4.1, Gemini) | 0.3 | 2048 | 0.9 | 1.1 | según modelo |

- `temperature` baja (0.2-0.3) = respuestas más directas y reproducibles, ideal para código
- `temperature` media (0.5) = equilibrada, útil para modelos locales que tienden a repetirse
- `max_tokens` 2048 = suficiente para respuestas con código; subir a 4096 solo si necesitás respuestas muy largas

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
- **Búsqueda**: filtrá conversaciones por título o contenido desde el campo de búsqueda en la parte superior
- Los títulos se guardan en el JSON y se restauran al recargar, sin perderse al reiniciar Geany
- Las conversaciones se guardan automáticamente al crear una nueva o cerrar Geany

### Slash Commands

Escribí un comando rápido en el input para transformar tu mensaje. Los comandos `/explain`, `/refactor`, `/test` y `/doc` incluyen automáticamente el archivo activo como contexto:

- `/explain` — Explica el código del archivo activo
- `/refactor` — Refactoriza para mejorar legibilidad
- `/test` — Genera tests unitarios
- `/doc` — Genera documentación
- `/fix` — Encuentra y corrige bugs en el archivo activo
- `/commit` — Genera un mensaje de commit convencional desde `git diff`
- `/review` — Revisa el archivo activo buscando bugs, seguridad y performance

Ejemplo: `/explain esta función`

### Send Selection

Seleccioná texto en el editor y presioná **Send Selection**. El plugin enviará solo esa porción como contexto, sin necesidad de copiar y pegar.

### Send Current File

Presioná **Send File** para enviar el contenido completo del archivo activo como contexto. Útil cuando necesitás que la IA vea todo el archivo, no solo una selección.

### @referencias en el chat

Incluí cualquier archivo del proyecto escribiendo `@nombre.ext` en el input del chat. El plugin busca el archivo en el directorio del documento actual o en la ruta absoluta, lo lee y lo incluye automáticamente como contexto:

```
Revisá @utils.c y decime si hay memory leaks
```

Funciona con rutas relativas (`@src/main.c`) o nombres de archivo simples (`@main.c`).

### Modo Ask

Activá el toggle **Ask** en la barra de botones para entrar en modo consulta. En este modo:
- La IA responde **sin modificar archivos** (ignora bloques ` ```file: `)
- Ideal para explicaciones, brainstorming o preguntas rápidas sin riesgo de que toque tu código
- Desactivá el toggle para volver al modo normal con edición de archivos habilitada

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
- Si el archivo está abierto: **reemplaza su contenido**
- Si no existe: **crea un nuevo archivo**

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

- [ ] **Error-aware / fix build**: capturar la salida de la ventana *Messages* de Geany para que `/fix` envíe error + archivo + línea al modelo
- [ ] **Contexto multi-archivo automático**: al enviar un mensaje, incluir automáticamente el archivo activo + archivos recientes + archivos del proyecto (`*.geany`)
- [ ] **Menú contextual en el editor**: integrar acciones al menú derecho de Geany — *Explain this*, *Refactor selection*, *Generate docstring*, *Add type hints*
- [ ] **Ejecución de comandos**: detectar bloques ` ```bash ` y mostrar botón *Run* que ejecute el comando en el directorio del proyecto (`g_spawn_async`)
- [ ] **Modo Plan**: la IA genera un plan paso a paso antes de ejecutar cambios, permitiendo al usuario aprobar, mejorar o rechazar cada paso individualmente
- [ ] **Previsualización diff inline**: mostrar cambios propuestos como anotaciones de Scintilla (verde/rojo) con botones *Apply* / *Discard* antes de modificar el archivo
- [ ] **Estimación de tokens**: mostrar en la UI cuántos tokens aprox. consume el contexto actual (chars/4) y alertar al acercarse al límite del modelo

### Avanzadas (diferenciadoras, mayor esfuerzo)

- [ ] **Índice de proyecto (vector search local)**: indexar archivos del proyecto (TF-IDF o embeddings livianos) para encontrar automáticamente definiciones relevantes al preguntar "¿dónde se define X?"
- [ ] **Modo Agent / Composer**: la IA puede leer múltiples archivos, proponer cambios, pedir confirmación y aplicarlos en pasos iterativos
- [ ] **Contexto LSP**: integrar con `geany-lsp` para capturar diagnostics (errores de tipo, lint) y enviarlos como contexto
- [ ] **Autocompletado con IA**: sugerencias inline mientras se escribe (estilo Copilot) vía hooks de Scintilla (`char-added` + ghost text)
- [ ] **Retry automático con backoff**: reenviar automáticamente requests en 429/5xx con backoff exponencial (actualmente sólo muestra mensaje de retry manual)

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
