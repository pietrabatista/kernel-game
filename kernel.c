/*
* Copyright (C) 2014  Arjun Sreedharan
* License: GPL version 2 or higher http://www.gnu.org/licenses/gpl.html
*/
#include "keyboard_map.h"

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

#define ENTER_KEY_CODE 0x1C

extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

/* Entrada via IRQ: última tecla capturada */
volatile int last_char = -1;  // -1 = nenhuma tecla nova

/* Estado do jogo da velha */
char board[9];     // ' ', 'X', 'O'
int  turn = 0;     // 0='X', 1='O'
int  game_over = 0;

/* current cursor location */
unsigned int current_loc = 0;
/* video memory begins at address 0xb8000 */
char *vidptr = (char*)0xb8000;

struct IDT_entry {
	unsigned short int offset_lowerbits;
	unsigned short int selector;
	unsigned char zero;
	unsigned char type_attr;
	unsigned short int offset_higherbits;
};

struct IDT_entry IDT[IDT_SIZE];


void idt_init(void)
{
	unsigned long keyboard_address;
	unsigned long idt_address;
	unsigned long idt_ptr[2];

	/* populate IDT entry of keyboard's interrupt */
	keyboard_address = (unsigned long)keyboard_handler;
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = INTERRUPT_GATE;
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;

	/*     Ports
	*	 PIC1	PIC2
	*Command 0x20	0xA0
	*Data	 0x21	0xA1
	*/

	/* ICW1 - begin initialization */
	write_port(0x20 , 0x11);
	write_port(0xA0 , 0x11);

	/* ICW2 - remap offset address of IDT */
	/*
	* In x86 protected mode, we have to remap the PICs beyond 0x20 because
	* Intel have designated the first 32 interrupts as "reserved" for cpu exceptions
	*/
	write_port(0x21 , 0x20);
	write_port(0xA1 , 0x28);

	/* ICW3 - setup cascading */
	write_port(0x21 , 0x00);
	write_port(0xA1 , 0x00);

	/* ICW4 - environment info */
	write_port(0x21 , 0x01);
	write_port(0xA1 , 0x01);
	/* Initialization finished */

	/* mask interrupts */
	write_port(0x21 , 0xff);
	write_port(0xA1 , 0xff);

	/* fill the IDT descriptor */
	idt_address = (unsigned long)IDT ;
	idt_ptr[0] = (sizeof (struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16 ;

	load_idt(idt_ptr);
}

void kb_init(void)
{
	/* 0xFD is 11111101 - enables only IRQ1 (keyboard)*/
	write_port(0x21 , 0xFD);
}

void kprint(const char *str)
{
	unsigned int i = 0;
	while (str[i] != '\0') {
		vidptr[current_loc++] = str[i++];
		vidptr[current_loc++] = 0x07;
	}
}

void kprint_newline(void)
{
	unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
	current_loc = current_loc + (line_size - current_loc % (line_size));
}

void clear_screen(void)
{
	unsigned int i = 0;
	while (i < SCREENSIZE) {
		vidptr[i++] = ' ';
		vidptr[i++] = 0x07;
	}
}

/* JOGO DA VELHA */
/* usa as globais já adicionadas:
   char board[9]; int turn; int game_over; volatile int last_char; */

static char cell_char(int idx) {
    return (board[idx] == ' ') ? ('1' + idx) : board[idx];
}

static void game_reset(void) {
    for (int i=0;i<9;i++) board[i] = ' ';
    turn = 0;
    game_over = 0;
}

static int game_check_winner(void) {
    static const int W[8][3] = {
        {0,1,2},{3,4,5},{6,7,8},
        {0,3,6},{1,4,7},{2,5,8},
        {0,4,8},{2,4,6}
    };
    for (int i=0;i<8;i++){
        int a=W[i][0], b=W[i][1], c=W[i][2];
        if (board[a] != ' ' && board[a]==board[b] && board[b]==board[c]) return board[a];
    }
    return 0;
}

static int game_full(void){
    for (int i=0;i<9;i++) if (board[i]==' ') return 0;
    return 1;
}

/* redesenha sempre do topo da tela */
static void game_render(void) {
    current_loc = 0;
    clear_screen();

    kprint("JOGO DA VELHA (1-9)  X vs O");
    kprint_newline(); kprint_newline();

    /* linha 0 */
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=cell_char(0); vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]='|'; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=cell_char(1); vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]='|'; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=cell_char(2); vidptr[current_loc++]=0x07;
    kprint_newline();

    kprint("---+---+---"); kprint_newline();

    /* linha 1 */
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=cell_char(3); vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]='|'; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=cell_char(4); vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]='|'; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=cell_char(5); vidptr[current_loc++]=0x07;
    kprint_newline();

    kprint("---+---+---"); kprint_newline();

    /* linha 2 */
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=cell_char(6); vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]='|'; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=cell_char(7); vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]='|'; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=' '; vidptr[current_loc++]=0x07;
    vidptr[current_loc++]=cell_char(8); vidptr[current_loc++]=0x07;
    kprint_newline(); kprint_newline();

    if (!game_over) {
        kprint("Vez de: ");
        vidptr[current_loc++] = (turn==0 ? 'X' : 'O'); vidptr[current_loc++] = 0x07;
        kprint("   (pressione 1-9)   [R = reiniciar]");
    } else {
        kprint("[R = reiniciar]");
    }
}

static void game_step(int ch) {
    if (ch=='r' || ch=='R') { game_reset(); return; }
    if (game_over) return;

    if (ch<'1' || ch>'9') return;
    int idx = ch - '1';
    if (board[idx] != ' ') return;

    board[idx] = (turn==0 ? 'X' : 'O');

    int w = game_check_winner();
    if (w) {
        game_over = 1;
        kprint_newline(); kprint_newline();
        kprint("Vencedor: ");
        vidptr[current_loc++] = (char)w; vidptr[current_loc++] = 0x07;
        return;
    }
    if (game_full()) {
        game_over = 1;
        kprint_newline(); kprint_newline();
        kprint("Empate!");
        return;
    }
    turn ^= 1; // troca jogador
}

void keyboard_handler_main(void)
{
    unsigned char status = read_port(KEYBOARD_STATUS_PORT);
    if (status & 0x01) {
        unsigned char keycode = (unsigned char)read_port(KEYBOARD_DATA_PORT);

        // ignorar break codes (tecla solta = bit 7 ligado)
        if (!(keycode & 0x80)) {
            unsigned char ascii = keyboard_map[keycode];
            if (ascii) {
                last_char = (int)ascii;   // passa a tecla pro loop
            }
        }
    }
    write_port(0x20, 0x20); // EOI no final
}

void kmain(void)
{
	const char *str = "my first kernel with keyboard support";
	clear_screen();
	kprint(str);
	kprint_newline();
	kprint_newline();

	idt_init();
	kb_init();

    /*  loop do jogo  */
    game_reset();
    game_render();

    while (1) {
        if (last_char != -1) {
            int c = last_char; last_char = -1;
            game_step(c);
            game_render();
        }
        __asm__ __volatile__("hlt"); // dorme até próxima IRQ
    }
}