# SQLite Contract — VitaOS con IA

## Rol

SQLite es el backend de auditoría obligatorio desde F1.

## Restricciones del milestone

- una sola base principal;
- un solo escritor local;
- acceso simple y predecible;
- prioridad a robustez sobre rendimiento.

## `sqlite_vfs.c`

Este módulo debe proveer:
- apertura/cierre del backend;
- lectura/escritura básica;
- fsync o equivalente si existe;
- manejo de errores degradando a diagnóstico restringido;
- abstracción mínima sobre el medio writable del USB.

## Reglas de implementación

- No modificar columnas de `schema/audit.sql` sin actualizar docs y código.
- No crear tablas experimentales fuera del contrato principal sin documentarlo.
- En caso de error fatal de auditoría, el sistema debe degradar de forma explícita.

## Pruebas mínimas

- abrir DB;
- crear esquema;
- insertar `boot_session`;
- insertar `audit_event`;
- validar lectura posterior.
