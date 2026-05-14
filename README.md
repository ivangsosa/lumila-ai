# Lumila AI - Plugin para Geany

AI Assistant plugin for Geany editor with multi-provider support.

## Features

- **Panel lateral de chat**: Interfaz integrada en Geany con UI moderna
  - Fondo negro con mensajes estilo burbujas
  - Mensajes del usuario alineados a la derecha
  - Respuestas de la IA alineadas a la izquierda
- **Edición de archivos por IA**: La IA puede modificar archivos abiertos directamente
  - Usa el formato ```file:nombre.ext para editar
  - Crea archivos nuevos si no existen
  - Resumen de cambios en el chat
- **Configuraciones avanzadas**:
  - Temperatura (0.0 - 2.0, default: 0.7)
  - Max tokens (100 - 8000, default: 1024)
  - Top P (0.0 - 1.0, default: 1.0)
- **Historial de conversaciones**:
  - Guardado automático en `~/.config/geany/plugins/lumila-ai/history/`
  - Botón "New Chat" para iniciar nueva conversación
  - Contexto multi-mensaje enviado a los modelos
- **Múltiples proveedores de IA**:
  - ✅ **Anthropic Claude 3.5 Sonnet**
  - ✅ **Anthropic Claude 3 Opus**
  - ✅ **OpenAI GPT-4o**
  - ✅ **OpenAI GPT-4o mini**
  - ✅ **Google Gemini 1.5 Flash**
  - ✅ **Google Gemini 1.5 Pro**
  - ✅ **Ollama Llama 3.2** (local)
  - ✅ **Ollama Qwen 2.5** (local)
  - ✅ **OpenRouter** (modelos gratuitos)

## Dependencies

### Kubuntu 2026.04 / Ubuntu 24.04+

```bash
# Herramientas de compilación
sudo apt install build-essential autoconf automake libtool pkg-config

# Bibliotecas GTK y GLib (requeridas primero)
sudo apt install libgtk-3-dev libglib2.0-dev

# Geany (incluye headers de desarrollo)
sudo apt install geany geany-plugins-common

# Bibliotecas de red y JSON
sudo apt install libsoup-3.0-dev libjansson-dev
```

**Nota:** En Ubuntu / Kubuntu, los headers de desarrollo de Geany están incluidos en el paquete `geany`, no existe un paquete `libgeany-dev` separado.

### Manjaro / Arch Linux

```bash
# Herramientas de compilación
sudo pacman -S base-devel autoconf automake libtool pkgconf

# Bibliotecas GTK y Geany
sudo pacman -S geany gtk3 glib2

# Bibliotecas de red y JSON
sudo pacman -S libsoup3 jansson
```

**Nota para Manjaro/Arch:** El paquete `geany` de los repositorios oficiales puede no incluir los headers de desarrollo necesarios. Si `./configure` falla con errores de `geanyplugin.h`, necesitarás compilar Geany desde source o buscar `geany-git` en AUR.

### Fedora (alternativa)

```bash
# Herramientas de compilación y Geany
sudo dnf install geany geany-devel gtk3-devel glib2-devel

# Bibliotecas de red y JSON
sudo dnf install libsoup3-devel jansson-devel
```

## Compilation

```bash
# Generate build files
./bootstrap.sh

# Configure
./configure

# Compile
make

# Option 1: Install to system (requires sudo)
sudo make install

# Option 2: Copy manually to user directory (recommended)
mkdir -p ~/.config/geany/plugins
cp src/.libs/lumila-ai.so ~/.config/geany/plugins/
```

**Nota:** La instalación manual (Opción 2) es recomendada ya que no requiere permisos de root y el plugin se carga correctamente desde `~/.config/geany/plugins/`.

**Nota:** Si `libsoup-3.0` no está disponible en tu sistema, el script `configure` automáticamente usará `libsoup-2.4` como fallback.

### Alternativa si libsoup3 no está disponible

```bash
# Ubuntu / Kubuntu
sudo apt install libsoup2.4-dev

# Manjaro / Arch
sudo pacman -S libsoup

# Fedora
sudo dnf install libsoup-devel
```

## Configuration

El archivo de configuración se crea de forma automática en:
`~/.config/geany/plugins/lumila-ai/config.json`

Puedes usar el archivo `config-example.json` como plantilla.

### Configuration of API Keys

Edita el archivo `config.json` y agrega tus API keys y configuraciones:

```json
{
  "version": "0.5.0",
  "api_keys": {
    "openai": "",
    "anthropic": "sk-ant-api03-...",
    "google": "",
    "kimi": "",
    "openrouter": "",
    "ollama": ""
  },
  "defaults": {
    "temperature": 0.7,
    "max_tokens": 1024,
    "top_p": 1.0,
    "default_provider_id": 8
  }
}
```

### Configuration Parameters

- **temperature**: Controla la aleatoriedad de las respuestas (0.0 = determinista, 2.0 = muy creativo)
- **max_tokens**: Número máximo de tokens en la respuesta (100-8000)
- **top_p**: Muestreo nucleus para diversidad de respuestas (0.0-1.0)
- **default_provider_id**: Modelo por defecto al iniciar (0-8, default: 8 = OpenRouter)
  - 0-1: Claude 3.5 Sonnet / Claude 3 Opus
  - 2-3: GPT-4o / GPT-4o mini
  - 4-5: Gemini 1.5 Flash / Gemini 1.5 Pro
  - 6-7: Ollama Llama 3.2 / Qwen 2.5
  - 8: OpenRouter

### Getting API Keys

- **Anthropic Claude**: https://console.anthropic.com/settings/keys
  - Modelos: Claude 3.5 Sonnet, Claude 3 Opus

- **OpenAI**: https://platform.openai.com/api-keys
  - Modelos: GPT-4o, GPT-4o mini

- **Google Gemini**: https://makersuite.google.com/app/apikey
  - Modelos: Gemini 1.5 Flash, Gemini 1.5 Pro

- **Ollama**: Local, sin API key necesaria
  - Instalación: `curl -fsSL https://ollama.com/install.sh | sh`
  - Descargar modelos: `ollama pull llama3.2:3b` o `ollama pull qwen2.5:7b`
  - Modelos: Llama 3.2 (3B), Qwen 2.5 (7B)
  - Endpoint: http://localhost:11434

- **OpenRouter**: https://openrouter.ai/keys
  - Acceso a múltiples modelos con una sola API key
  - Modelo por defecto: `openrouter/free` (modelos gratuitos)

## Usage

1. **Instala el plugin** (ver sección Compilación)
2. **Configura tu API key** en `~/.config/geany/plugins/lumila-ai/config.json`
3. **Reinicia Geany** completamente:
   ```bash
   killall geany
   geany &
   ```
4. Ve a **Herramientas → Administrador de complementos** y activa "Lumila AI"
5. Aparecerá un panel lateral "Lumila" en la barra lateral
6. Selecciona el modelo deseado en el selector:
   - Claude 3.5 Sonnet
   - Claude 3 Opus
   - GPT-4o
   - GPT-4o mini
   - Gemini 1.5 Flash
   - Gemini 1.5 Pro
   - Ollama Llama 3.2 (local, gratis)
   - Ollama Qwen 2.5 (local, gratis)
   - **OpenRouter** (por defecto, modelos gratuitos)
7. Escribe tu mensaje y presiona "Send"

### Edición de archivos

Para que la IA modifique un archivo, simplemente pedile que lo haga. La IA responderá con bloques de código anotados:

\`\`\`file:main.c
#include <stdio.h>
int main() { return 0; }
\`\`\`

El plugin detectará estos bloques y:
- Si el archivo está abierto: **reemplaza su contenido** en el editor
- Si no existe: **crea un nuevo archivo** sin título

Podés editar múltiples archivos en una sola respuesta.

### Troubleshooting

**Error: "API key not configured"**
- Verifica que el archivo `config.json` existe y tiene tu API key
- Reinicia Geany completamente después de editar el config

**El plugin no aparece en el administrador**
- Verifica que `lumila-ai.so` está en `~/.config/geany/plugins/`
- Intenta copiar también a `~/.local/share/geany/plugins/`
- Como última opción: `sudo make install` para instalar en el sistema

## License

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
