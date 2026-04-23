# Kernel Layout — VitaOS con IA

## Principio

Kernel común grande, capa de arquitectura delgada.

## Kernel común

- `kmain.c`: coordinación de arranque y estados globales.
- `console.c`: consola temprana y consola guiada.
- `panic.c`: rutas fatales, salida mínima y auditoría de panic.
- `memory.c`: memoria temprana y contratos del heap.
- `scheduler.c`: scheduler mínimo y tareas del núcleo.
- `ai_core.c`: percepción, evaluación y coordinación.
- `proposal.c`: ciclo de vida de propuestas y respuestas humanas.
- `audit_core.c`: auditoría append-mostly.
- `sqlite_vfs.c`: bridge entre SQLite y almacenamiento VitaOS.
- `node_core.c`: peers, tareas y replicación básica.
- `emergency_core.c`: clasificación inicial de emergencia y uso de knowledge packs.

## No objetivos del layout actual

- multitarea compleja;
- userspace completo;
- VFS general de propósito amplio.
