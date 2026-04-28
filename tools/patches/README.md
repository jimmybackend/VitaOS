# tools/patches (historical)

Los parches históricos de desarrollo (v20-v32 y previos) ya fueron consolidados en los archivos reales del repositorio.

Esta carpeta se conserva solo como marcador histórico para indicar que:

- VitaOS no debe depender de scripts de parche para funcionar o compilar;
- no se mantiene runtime/build Python en el flujo obligatorio;
- el flujo esperado es clonar, compilar (`make`, `make hosted`, `make smoke-audit`, `make smoke`, `make iso`) y validar.

Si aparece nueva lógica en `tools/patches/`, debe integrarse al árbol fuente real y eliminarse el helper temporal.
