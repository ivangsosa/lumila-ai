# Changelog

El formato sigue [Keep a Changelog](https://keepachangelog.com/) y el
proyecto usa versionado semántico.

## [Unreleased]

## [0.11.0] - 2026-10-11

### Added

- Modelos de octubre de 2026: Claude Sonnet/Opus/Haiku 5.5, GPT-6.1 Sol, GPT-6 Astra y Luna, Gemini 3.8/3.6 Flash y 3.1 Pro, DeepSeek V4.1 Flash y V4 Pro, Mistral Large 4.
- Modelos personalizados desde `config.json` (sección `models`), sin recompilar.
- Modo Plan: la IA propone un plan sin tocar archivos, y el botón "Execute Plan" lo implementa después.
- Confirmación antes de aplicar ediciones a archivos (se puede desactivar).
- Reintento automático cuando el proveedor responde errores temporales (límite de uso, servidor saturado).
- Estimador de tokens junto al selector de modelo, y uso real cuando el proveedor lo reporta.
- Botón "Copy" en cada bloque de código del chat.
- Atajos de teclado configurables para enviar la selección o el archivo actual.
### Fixed

- El plugin no compilaba desde una descarga limpia del repositorio (faltaba un archivo).
- Cancelar un pedido ya no deja al proveedor inutilizable hasta cambiar de modelo.
- Los errores del proveedor (clave inválida, límite de uso, fallas del servidor) ahora se muestran con detalle en vez de un mensaje genérico.
- Al cancelar y reenviar rápido, ya no puede colarse la respuesta vieja sobre la nueva.
- Editar archivos que contienen bloques de código propios (como un README) ya no los trunca.
- Las respuestas con error ya no se guardan como si fueran respuestas válidas.
- Se bloquean las referencias a archivos sensibles (`~/.ssh`, `.env`, etc.).
- Las referencias `@archivo` ahora se resuelven dentro del proyecto y tienen límite de tamaño.

### Changed

- Tiempo de espera por defecto subido a 180 s (los modelos con razonamiento tardan más en responder).
- Ahora se requiere Geany 2.0 o superior.
- La elección de modelo se guarda por nombre, así que sigue funcionando aunque la lista cambie.

## [0.10.0] - 2026-08-17

### Added

- Conversación multi-turno: la IA recuerda los mensajes anteriores de la charla.
- Modelos actualizados: Claude 5, GPT-5.6, Gemini 3.x, DeepSeek V4, Mistral Large 3.
- Tiempos de espera configurables por proveedor.

### Fixed

- Correcciones en la respuesta del endpoint y en el manejo del tiempo de espera.

## [0.9.3] - 2026-07-05

### Added

- El modo elegido (Code/Ask) se recuerda entre sesiones.
- Límite de contexto para no exceder la ventana del modelo.
- Detección de errores temporales (base del reintento automático).

## [0.9.2] - 2026-07-05

### Fixed

- Fugas de memoria, errores HTTP que no se mostraban y problemas de robustez en el streaming.

## [0.9.1] - 2026-06-12

### Changed

- Mejoras visuales en la barra lateral.
- Instrucciones internas (system prompt) mejoradas para mejores respuestas.

## [0.9.0] - 2026-06-12

### Added

- Resaltado de sintaxis en los bloques de código del chat.
- Envío del mensaje con Enter (Shift+Enter para nueva línea).
- Referencias a archivos con `@nombre.ext`.
- Modo Ask para consultar sin modificar archivos.
- Búsqueda en el historial de conversaciones.
- Botón "Check Updates" con instrucciones de actualización.
- Nuevos comandos `/fix`, `/commit` y `/review`.

## [0.8.0] - 2026-06-12

### Added

- Comandos de barra para tareas comunes (explicar, refactorizar, generar tests, documentar).
- Exportación de la conversación a Markdown.
- Modelos personalizados por proveedor en la configuración.

### Fixed

- Auditoría general: correcciones críticas, limpieza de los providers y mejoras de streaming.

## [0.7.0] - 2026-06-12

### Added

- Nuevos modelos: Moonshot Kimi K2.6, GLM-4, Grok 3, Qwen3-235B, GPT-4.1 nano, Mistral Small 3.1.
- Actualización de los modelos existentes a sus versiones vigentes.

## [0.6.0] - 2026-05-15

### Added

- Streaming: las respuestas se van escribiendo en tiempo real.
- Historial de conversaciones persistente con opción de continuarlas.
- Nuevos proveedores: DeepSeek, Mistral y Moonshot.
- Modelos actualizados a la línea 2025-2026.
- Atajos de teclado y apariencia renovada del chat.

### Changed

- "OpenRouter Auto" reemplaza al modelo Quasar discontinuado.

## [0.5.0] - 2026-05-14

### Added

- Edición directa de archivos con bloques ` ```file:nombre ` y creación de archivos nuevos.

### Fixed

- Corrección en las cabeceras y el modelo usado para OpenRouter.

## [0.4.0] - 2026-05-07

### Added

- Registro de modelos que alimenta el selector de la barra lateral.

### Fixed

- Mejoras y correcciones de errores generales.

## [0.1.0] - 2026-03-25

Primera versión de desarrollo.

### Added

- Chat con IA dentro de un panel lateral de Geany.
- Proveedores: OpenAI, Anthropic, Google, OpenRouter y Ollama (local).
- Claves de API configurables por proveedor.
- Selector de modelo y parámetros básicos (temperatura, tokens, penalización).
