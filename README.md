# FramePad

Editor de vídeo de código abierto para Windows, con plugins de audio (VST3 / CLAP) y de vídeo (OpenFX).

> Estado: **fase 1 (esqueleto)**. Ya hay ventana con los cinco paneles, proyectos que se guardan y se reabren, deshacer/rehacer, e interfaz en español e inglés. Todavía no importa ni reproduce medios.

## Descargar

Cada push compila FramePad en GitHub Actions. Para probar la última versión:

1. Abre la pestaña **Actions** del repositorio y entra en la ejecución más reciente de **CI**.
2. Descarga el artefacto `FramePad-…-win-x64`, descomprímelo y ejecuta `FramePad.exe`.

Cuando haya versiones publicadas aparecerán en **Releases**, con instalador y actualización automática.

## Compilar

La compilación oficial es la de GitHub Actions ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)). Para compilar en local hace falta Visual Studio 2022 (C++), CMake 3.21+ y Qt 6.8:

```bash
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
ctest --test-dir build -C Release
```

## Traducciones

Los textos de la interfaz están en inglés en el código y se traducen con Qt Linguist. Cada idioma es un archivo en [`translations/`](translations/). Para añadir un idioma, copia `FramePad_es.ts` con el código del nuevo idioma, añádelo a `TS_FILES` en `CMakeLists.txt` y abre el archivo con Qt Linguist.

## Licencia

[GNU GPL v3](LICENSE).
