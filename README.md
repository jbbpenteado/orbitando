# Orbitando
Port para C (Windows, Linux e WASM) do programa em Basic do Livro: "Conhecendo e Utilizando o TK2000 Color" de Victor Mirshawka

# Orbitando — Versão Desktop

Port para linguagem C do programa Basic "Orbitando" do livro:
Compreendendo e Usando o TK2000 Color - Victor Mirshawka — (1984).

## Visão geral
Port em C que reproduz a UI de parâmetros e a simulação orbital do programa original em BASIC. Interface implementada com SDL2; SDL2_ttf é usada quando disponível para render de texto.

- Linguagem: C (compatível com C17)
- Plataforma alvo: Linux (instruções ), Windows (Code::Blocks/MinGW disponíveis)
- Principais fontes: `orbital.c`, `orbital_input.c`, `orbital_input.h`

## Arquivos principais
- `orbital.c` — aplicação principal e loop de animação  
- `orbital_input.c`, `orbital_input.h` — modal de entrada de parâmetros (UI SDL2)  
- Code::Blocks: `orbitando.cbp`, `orbitando.depend`, `orbitando.layout` (opcionais para Windows)  

## Requisitos (Linux)
- build tools: `gcc`, `make`, `pkg-config`  
- libs de desenvolvimento: `libsdl2-dev`, `libsdl2-ttf-dev`

Instalação (Debian/Ubuntu):
```bash
sudo apt update
sudo apt install build-essential pkg-config libsdl2-dev libsdl2-ttf-dev

Compilação (Linux)

Release:

gcc -O2 -Wall `sdl2-config --cflags` orbital.c orbital_input.c -o orbitando `sdl2-config --libs` `pkg-config --cflags --libs SDL2_ttf` -lm

Debug:

gcc -g -Wall `sdl2-config --cflags` orbital.c orbital_input.c -o orbitando_debug `sdl2-config --libs` `pkg-config --cflags --libs SDL2_ttf` -lm


Observações:

    Se pkg-config --cflags --libs SDL2_ttf falhar, tente pkg-config --cflags --libs sdl2_ttf dependendo da distro.

    Ajuste -O2/-O3 e -g conforme necessidade.

Execução

./orbitando

Fluxo: abre modal "Parâmetros de Entrada" para configurar objetos; pressione OK para iniciar a simulação; ESC fecha/volta.

Windows / Code::Blocks

    Inclua SDL2.dll e SDL2_ttf.dll junto ao executável ou no PATH.

Debug / Performance

    Erros de link: verifique pacotes de desenvolvimento instalados.

    Ferramentas úteis: perf, valgrind (Linux); Visual Studio Profiler (Windows).

Créditos

Base: Victor Mirshawka — Compreendendo e Usando o TK2000 Color (1984). Port para C e manutenção: João Penteado


---

### README_wasm.md

```markdown
# Orbitando — Versão WebAssembly (WASM)

Port para WebAssembly da implementação em C do programa Basic "Orbitando". Executa no navegador usando Emscripten + SDL2.

## Visão geral
Código C compilado para WASM com Emscripten; front-end HTML/JS integra controls e canvas. Artefatos gerados: `index.js`, `index.wasm`, `index.data`, `app.js`, `index.html`.

- Linguagem: C (C17 recomendado)
- Runtime: navegador moderno (Chrome/Edge/Firefox)

## Requisitos (desenvolvimento)
- Emscripten SDK (emcc) instalado e ativado:

```bash
source /path/to/emsdk_env.sh

    Python 3 (ou outro servidor HTTP) para testes locais

Build (rápido)

Torne o script executável e execute:

chmod +x build_wasm.sh
./build_wasm.sh


O script gera index.js, index.wasm e index.data e faz preload de assets/fonts se presente.

Notas sobre flags:

    Usamos -O3 -ffast-math para otimização.

    NÃO use -flto com as bibliotecas SDL pré-compiladas do Emscripten — causa erros de link.

    Opcional: -msimd128 (SIMD) para testes de performance, se o navegador suportar.

Servir e testar (local)

Sirva via HTTP (não abrir via file://):

python3 -m http.server 8080

Abra:

http://localhost:8080/

Integração front-end

    index.html — HTML que carrega index.js e app.js

    app.js — UI: construção de linhas, alinhamento responsivo das colunas, envio de buffers para WASM via Module._apply_inputs_from_js

    Controle: Build rows, Apply inputs, Start, Stop

Performance e troubleshooting

    Perfil: DevTools → Performance / Memory

        tempo em wasm-function → otimize C

        tempo em Scripting / HEAPF64.set / ccall → reduzir chamadas e cópias JS↔WASM

        tempo em Paint / Composite → reduzir resolução de canvas ou usar WebGL

    Limpar cache do Emscripten se ocorrerem erros de link:

	rm -rf ~/.emscripten_cache

Execução do fluxo

    ./build_wasm.sh

    Servir via HTTP (python3 -m http.server 8000)

    Abrir index.html, configurar e rodar simulação

Créditos

Base: Victor Mirshawka — Compreendendo e Usando o TK2000 Color (1984). Port e integração WASM: João Penteado.