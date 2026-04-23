# Build Tools — VitaOS con IA

Scripts actuales:

- `tools/build/build-x86_64.sh`
  - limpia y compila `BOOTX64.EFI`.
- `tools/build/qemu-x86_64.sh`
  - ejecuta en QEMU + OVMF usando `build/efi` como volumen FAT.

Targets make:

```bash
make            # bootx64.efi
make hosted     # binario hosted para validación SQLite
make run        # arranque QEMU
make smoke      # smoke de banner (QEMU)
make smoke-audit # smoke de auditoría SQLite
```
