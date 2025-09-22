# Microkernel com Jogo da Velha

## Introdução:

Este projeto implementa um microkernel simples escrito em **Assembly** e **C**, capaz de:

- **Bootloader (Multiboot):** O código em Assembly (`kernel.asm`) inicializa a CPU e aponta para o kernel em C.
- **Exibir caracteres na tela (VGA):** O kernel escreve diretamente na memória de vídeo (`0xb8000`).
- **Interrupções do teclado:** Configura a IDT (Interrupt Descriptor Table) e um handler para capturar teclas.
- **Aplicação:** Um jogo da velha simples é o loop principal.

Foi utilizado o QEMU para emulação, evitando dependência de hardware real.

## Estrutura do Projeto:

```
.
├── kernel.asm      # código Assembly inicial (boot + funções auxiliares).
├── kernel.c        # lógica do kernel + jogo da velha.
├── keyboard_map.h  # mapa de teclas (scancode → caractere).
└── link.ld         # script do linker que organiza memória do kernel.
```
### Principais Arquivos:

- kernel.asm: define o ponto de entrada, configura a pilha, expõe funções I/O em portas e o handler de interrupções.
- kernel.c: contém a inicialização da IDT, tratamento do teclado e o loop do jogo.
- keyboard_map.h: traduz scancodes do teclado em caracteres ASCII.
- link.ld: script de link que organiza seções `.text`, `.data`, `.bss`.

## Como compilar e rodar:

### Pré-requisitos:

- linux (ou wsl)
- NASM
- gcc
- QEMU

### Compilação:
```
nasm -f elf32 kernel.asm -o kernel.o
gcc -m32 -ffreestanding -fno-stack-protector -c kernel.c -o kernel_c.o
ld -m elf_i386 -T link.ld -o kernel.elf kernel.o kernel_c.o
```
### Execução no QEMU:

```
qemu-system-i386 -kernel kernel.elf
```

## Jogo da Velha

- O tabuleiro é renderizado diretamente na memória de vídeo.

- Os jogadores alternam entre X e O.

- Teclas válidas:

    - 1..9 → marca a posição correspondente.

    - R → reinicia o jogo.

- O kernel detecta vitória ou empate e exibe a mensagem correspondente.

## Demonstração em vídeo

## 🙏 Créditos

Este projeto foi desenvolvido como atividade acadêmica, tendo como base o código didático disponibilizado por Arjun Sreedharan (2014), disponível em: arjunsreedharan/os.

O código original foi adaptado e expandido para incluir:

- Loop principal controlado em C.
- Implementação de um jogo da velha interativo em modo texto.
