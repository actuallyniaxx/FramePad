# FramePad

Editor de vídeo de código abierto para Windows, con plugins de audio (VST3 / CLAP) y de vídeo (OpenFX).

> Estado: **fase 2 (medios y visor)**. Importa vídeo y audio (arrastrando o con Archivo → Importar medios), muestra miniaturas y formas de onda, y reproduce clips en el visor de origen con la imagen en la GPU (Direct3D 11) y el sonido por WASAPI, sincronizados. Los proyectos guardan sus medios. Todavía no hay línea de tiempo con clips ni exportación.

En el visor: **Espacio** reproduce/pausa, **K**/**L** pausa/reproduce, **←**/**→** avanzan o retroceden un fotograma, **Inicio**/**Fin** saltan al principio o al final.

## Descargar

Cada push compila FramePad en GitHub Actions. Para probar la última versión:

1. Abre la pestaña **Actions** del repositorio y entra en la ejecución más reciente de **CI**.
2. Descarga el artefacto `FramePad-…-win-x64`, descomprímelo y ejecuta `FramePad.exe`.

Cuando haya versiones publicadas aparecerán en **Releases**, con instalador y actualización automática.

## Compilar

La compilación oficial es la de GitHub Actions ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)). Para compilar en local hace falta Visual Studio 2022 (C++), CMake 3.21+ , Qt 6.8 con el módulo Qt Shader Tools y un paquete de FFmpeg «shared» (por ejemplo `ffmpeg-n8.1-latest-win64-gpl-shared-8.1` de [BtbN/FFmpeg-Builds](https://github.com/BtbN/FFmpeg-Builds/releases)):

```bash
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 -DFFMPEG_ROOT=C:/ffmpeg
cmake --build build --config Release
ctest --test-dir build -C Release
```

## Traducciones

Los textos de la interfaz están en inglés en el código y se traducen con Qt Linguist. Cada idioma es un archivo en [`translations/`](translations/). Para añadir un idioma, copia `FramePad_es.ts` con el código del nuevo idioma, añádelo a `TS_FILES` en `CMakeLists.txt` y abre el archivo con Qt Linguist.

## Licencia

[GNU GPL v3](LICENSE). Las compilaciones incluyen las DLL de [FFmpeg](https://ffmpeg.org) (versión GPL); su licencia va en `licenses/FFmpeg.txt`.
