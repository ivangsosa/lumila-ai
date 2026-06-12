# Lumila AI - Plugin para Geany v0.8.0

AI Assistant plugin for Geany editor with multi-provider support, persistent history, and real-time streaming.

## Features

- **Panel lateral de chat** con UI moderna oscura
  - Fondo moderno (`#0f0f23`) con mensajes estilo burbujas
  - Mensajes del usuario alineados a la derecha (azul `#82AAFF`)
  - Respuestas de la IA alineadas a la izquierda (blanco azulado `#C8D3F5`)
  - Syntax highlighting en bloques de código
- **Edición de archivos por IA**: la IA puede modificar archivos abiertos directamente
  - Usa el formato ` ```file:nombre.ext ` para editar
  - Crea archivos nuevos si no existen
  - Resumen de cambios en el chat
- **Streaming de respuestas** (OpenAI, DeepSeek, Mistral, OpenRouter, Moonshot con libsoup-3.0): texto en tiempo real
- **Slash commands** rápidos: `/explain`, `/refactor`, `/test`, `/doc`
- **Send Selection**: envía el texto seleccionado en el editor como contexto
- **Exportar a Markdown**: guarda la conversación actual como `.md`
- **Modelos custom** vía `config.json`: sobrescribe el modelo de cualquier provider
- **Historial de conversaciones persistente**
  - Guardado automático en `~/.config/geany/plugins/lumila-ai/history/`
  - Botón **History** para listar, continuar o eliminar conversaciones
  - Título extraído automáticamente del primer mensaje
  - Contexto multi-mensaje enviado a los modelos
- **Atajos de teclado**
  - **Ctrl+Enter**: enviar mensaje
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
  "version": "0.8.0",
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
  }
}
```

### Parámetros

- **temperature**: aleatoriedad (0.0 - 2.0, default 0.3)
- **max_tokens**: máximo de tokens (100 - 8000, default 2048)
- **top_p**: muestreo nucleus (0.0 - 1.0, default 0.9)
- **repeat_penalty**: penalización de repetición (default 1.1)
- **default_provider_id**: modelo por defecto (0-21, default 9 = DeepSeek V3)

### Configuraciones sugeridas por modelo

| Escenario | `temperature` | `max_tokens` | `top_p` | `repeat_penalty` | `default_provider_id` |
|---|---|---|---|---|---|
| **DeepSeek V3** (rápido, barato, bueno para código) | 0.3 | 2048 | 0.9 | 1.1 | 10 |
| **DeepSeek R1** (raciocinio profundo) | 0.2 | 4096 | 0.95 | 1.1 | 11 |
| **Ollama Qwen3:8b** (local, ligero) | 0.5 | 2048 | 0.9 | 1.05 | 7 |
| **OpenRouter Auto** (gratis, enrutamiento automático) | 0.3 | 2048 | 0.9 | 1.1 | 8 |
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
7. Presiona **Send** o **Ctrl+Enter** para enviar

### Historial

- Clic en **History** para ver conversaciones guardadas
- **Continue**: carga una conversación anterior en el chat activo
- **Delete**: elimina una conversación del historial
- Las conversaciones se guardan automáticamente al crear una nueva o cerrar Geany

### Slash Commands

Escribí un comando rápido en el input para transformar tu mensaje:

- `/explain` — Explica el código seleccionado o del contexto
- `/refactor` — Refactoriza para mejorar legibilidad
- `/test` — Genera tests unitarios
- `/doc` — Genera documentación

Ejemplo: `/explain esta función`

### Send Selection

Seleccioná texto en el editor y presioná **Send Selection**. El plugin enviará solo esa porción como contexto, sin necesidad de copiar y pegar.

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

## Roadmap / Características futuras

- **Historial — Auto-back to chat**: al presionar **Continue**, volver automáticamente a la pestaña de chat sin tener que clickear *Back to Chat*
- **Historial — Persistencia de títulos**: actualmente el título se pierde al reiniciar Geany (muestra `conversation-20260612-030343.json` por ejemplo); guardar el título extraído en el JSON del historial para mostrarlo siempre
- **Historial — Eliminar conversaciones**: mejorar la UI de eliminación (actualmente hay un botón *Delete* básico; agregar confirmación)
- **Chat — Bloques de código separados**: renderizar bloques de código generados por la IA en contenedores visuales distintos del texto, con fondo oscuro y bordes redondeados para mejor lectura
- **Chat — Syntax highlighting mejorado**: ampliar el resaltado de sintaxis a más lenguajes (Rust, Go, TypeScript, etc.) y soportar resaltado de bloques anidados
- **Modo Ask**: modo de consulta rápida donde la IA responde sin editar archivos (desactiva `file:` blocks)
- **Modo Plan**: la IA genera un plan paso a paso antes de ejecutar cambios, permitiendo al usuario aprobar o rechazar cada paso

## Troubleshooting

**Error: "API key not configured"**
- Verifica que `config.json` existe y contiene la key del proveedor seleccionado
- Reinicia Geany después de editar el config

**El plugin no aparece en el administrador**
- Verifica que `lumila-ai.so` está en `~/.config/geany/plugins/`
- Como alternativa: `sudo make install`

**Streaming no funciona**
- Requiere `libsoup-3.0`. Si tienes `libsoup-2.4`, las respuestas llegarán completas al final

## License

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
