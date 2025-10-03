#!/usr/bin/env bash
set -euo pipefail

# Ajuste conforme seu projeto
SRC="wasm_orbital.c"
OUT_JS="index.js"
OUT_HTML=""        # deixamos vazio, não gerar HTML automático
ASSETS_DIR="assets/fonts"
FONT_FILE="${ASSETS_DIR}/LiberationSans-Regular.ttf"

# Funções C exportadas (apenas as que seu JS chama)
EXPORTED_FUNCTIONS='["_apply_inputs_from_js","_start_animation","_stop_animation","_set_canvas_size","_malloc","_free"]'
# Métodos do runtime JS que usamos
EXPORTED_RUNTIME_METHODS='["ccall","cwrap","getValue","setValue","UTF8ToString","HEAPF64","HEAP32","HEAPU8","FS_createDataFile"]'

# Verificações básicas
if ! command -v emcc >/dev/null 2>&1; then
  echo "Erro: emcc não encontrado no PATH. Ative o emsdk (source /path/to/emsdk_env.sh) e tente novamente."
  exit 1
fi

if [ ! -f "${SRC}" ]; then
  echo "Erro: fonte não encontrada: ${SRC}"
  exit 1
fi

if [ ! -d "${ASSETS_DIR}" ]; then
  echo "Aviso: ${ASSETS_DIR} não existe — criando (coloque a fonte lá se precisar)."
  mkdir -p "${ASSETS_DIR}"
fi

if [ ! -f "${FONT_FILE}" ]; then
  echo "Aviso: fonte ${FONT_FILE} não encontrada. Copie LiberationSans-Regular.ttf para ${ASSETS_DIR} se precisar."
fi

echo "Compilando ${SRC} -> ${OUT_JS} (com preload ${ASSETS_DIR}) ..."
emcc "${SRC}" -O2 \
  -s USE_SDL=2 -s USE_SDL_TTF=2 \
  -s ALLOW_MEMORY_GROWTH=1 \
  --preload-file "${ASSETS_DIR}@/assets/fonts" \
  -s EXPORTED_RUNTIME_METHODS="${EXPORTED_RUNTIME_METHODS}" \
  -s EXPORTED_FUNCTIONS="${EXPORTED_FUNCTIONS}" \
  -s WASM=1 \
  -o "${OUT_JS}"

echo "Compilação finalizada."
echo
echo "Arquivos esperados (verifique):"
ls -1 index.js index.wasm index.data 2>/dev/null || true
echo
echo "Servir localmente:"
echo "  python3 -m http.server 8000"
echo "Abrir: http://localhost:8000/"
echo
echo "Dicas:"
echo "- No index.html certifique-se de carregar index.js antes de app_debug.js."
echo "- No DevTools Network marque 'Disable cache' e faça Hard Reload (Ctrl+F5)."