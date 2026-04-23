# framebuffer driver — VitaOS con IA

## Objetivo

Definir el contrato del driver `framebuffer` para F1.

## Reglas

- no asumir hardware no detectado;
- exponer capacidades al kernel común;
- registrar errores importantes en auditoría si afectan operación;
- evitar lógica de política aquí.

## Pendientes para Codex

- definir `init()`;
- definir `probe()`;
- definir estructura de estado;
- definir eventos de auditoría relevantes;
- documentar limitaciones del driver en F1.
