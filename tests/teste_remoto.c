/* Os cenarios do controle remoto (firmware/src/remoto.c) no PC: o que o tests/teste_remoto.py confere. Cada linha
 * da saida e "nome valor..."; o espelho sai em "anel <hex>" para o Python decodificar com o decodificador dele. */
#include <stdio.h>
#define TESTE_REMOTO
#include "../firmware/src/remoto.c"

#define B(a)  memoria[a]
#define H(a)  (*(unsigned short *)(memoria + (a)))
#define W(a)  (*(unsigned *)(memoria + (a)))

/* Um quadro do PC no buffer da sessao, a partir de o. */
static void quadro(unsigned o, unsigned tecla, unsigned bits)
{
    unsigned char *q = memoria + 0x16671 + o, s = 0, i;

    q[0] = '~';
    q[1] = 'R';
    q[2] = (unsigned char)tecla;
    q[3] = (unsigned char)bits;
    q[4] = q[5] = 0;
    for (i = 0; i < 6; i++)
        s += q[i];
    q[6] = s;
}

/* O quadro com o no da fatia no quinto byte. */
static void quadro_no(unsigned tecla, unsigned bits, unsigned no)
{
    unsigned char *q = memoria + 0x16671;

    quadro(0, tecla, bits);
    q[4] = (unsigned char)no;
    q[6] = (unsigned char)(q[6] + no);
}

static void limpa(void)
{
    memset(memoria, 0, sizeof memoria);
    magic_devolve = 7;
    magic_vezes = tela_toda_vezes = motor_menu_vezes = respostas = resposta_n = travas = 0;
    tecla_fisica = 255;
    relogio_ms = 100000;
    passo_ms = 1;
    dreno = 0;
    B(0x14778) = 1;                       /* a UART ligada */
}

static void anel(unsigned tamanho, unsigned escrita)
{
    W(0x151AC) = (unsigned)(memoria + 0x18000);
    W(0x151AC + 4) = (unsigned)(memoria + 0x18000 + tamanho);
    W(0x151AC + 12) = (unsigned)(memoria + 0x18000 + escrita);
    H(0x151AC + 16) = 0;
    H(0x151AC + 18) = (unsigned short)tamanho;
}

static void mostra_anel(const char *nome, unsigned de)
{
    unsigned char *ini = (unsigned char *)W(0x151AC), *fim = (unsigned char *)W(0x151AC + 4), *p = ini + de;
    unsigned n = H(0x151AC + 16), i;

    printf("%s", nome);
    printf(" ");
    for (i = 0; i < n; i++) {
        printf("%02x", *p);
        if (++p == fim)
            p = ini;
    }
    printf(" %u\n", (unsigned)((unsigned char *)W(0x151AC + 12) - ini));
}

/* O redesenho imitado (o TELA_TODA do teste_remoto.h): a tela em 4 faixas de 160 x 32, cada uma no buffer de video e
 * mandada pelo envio, como o firmware faz; cada linha com a sua cor (o byte alto e o numero da linha). */
static unsigned vivo_no_meio;

static void faixas(void)
{
    unsigned f, k;

    for (f = 0; f < 4; f++) {
        for (k = 0; k < 160 * 32; k++) {
            memoria[0x10190 + k * 2] = (unsigned char)(f * 32 + k / 160);
            memoria[0x10190 + k * 2 + 1] = 0x55;
        }
        B(0x14608) = 0;
        B(0x1460C) = 160;
        B(0x1460A) = (unsigned char)(f * 32);
        B(0x1460E) = (unsigned char)(f * 32 + 32);
        remoto_envia();
    }
    /* um carimbo velho: o vigia nao pode zerar o REM no meio, e o PTT segue o do quadro (nao o fio): 3 */
    relogio_ms += 2000;
    vivo_no_meio = (ptt_c(12) != porta_falsa) + (B(0x14BEE) != 0) * 2;
}

int main(void)
{
    unsigned r, i, x, y;

    /* 1. o quadro: em qualquer posicao, o mais novo, e o resto vai ao conferidor */
    limpa();
    memcpy(memoria + 0x16671, "lixo", 4);
    quadro(4, 3, 0);
    quadro(11, 17, 1);
    memcpy(memoria + 0x16671 + 18, "~R", 2);       /* um comeco de quadro cortado no fim */
    H(0x1666C) = 20;
    r = remoto_cabo();
    printf("quadro_novo %u %u %u\n", r, magic_vezes, B(0x14BEE));
    limpa();
    memcpy(memoria + 0x16671, "PROGRAMBT11U", 12);
    H(0x1666C) = 12;
    magic_devolve = 1;
    r = remoto_cabo();
    printf("programbt %u %u %u\n", r, magic_vezes, B(0x14BEE));
    limpa();
    quadro(0, 5, 0);
    memoria[0x16671 + 6] ^= 1;                     /* a soma errada */
    H(0x1666C) = 7;
    r = remoto_cabo();
    printf("soma_errada %u %u %u\n", r, magic_vezes, B(0x14BEE));
    limpa();
    quadro(0, 18, 0);                              /* tecla fora da tabela: nenhuma */
    H(0x1666C) = 7;
    remoto_cabo();
    printf("tecla_18 %u\n", B(0x14BEE));
    limpa();
    quadro(0, 3, 0);
    H(0x1666C) = 6;                                /* so 6 bytes: o firmware nem chamaria, e nada se le */
    r = remoto_cabo();
    printf("seis_bytes %u %u %u\n", r, magic_vezes, B(0x14BEE));

    /* 2. a tecla: a do remoto so com nenhuma apertada; a resposta sai uma vez */
    limpa();
    quadro(0, 9, 0);
    H(0x1666C) = 7;
    B(0x1533C) = 0;
    B(0x15367) = 4;
    medidor = 13;
    B(0x149BC) = 167;
    remoto_cabo();
    r = remoto_tecla();
    printf("tecla_remota %u %u %u\n", r, respostas, resposta_n);
    printf("resposta");
    for (i = 0; i < resposta_n; i++)
        printf(" %u", resposta[i]);
    printf("\n");
    tecla_fisica = 2;
    r = remoto_tecla();
    printf("tecla_fisica %u %u\n", r, respostas);

    /* 3. o vigia: solta com 1 s sem quadro (16 carimbos de 64 ms) */
    limpa();
    relogio_ms = 64 * 1000 - 1;                    /* a proxima leitura e o carimbo 1000 */
    quadro(0, 4, 1);
    H(0x1666C) = 7;
    remoto_cabo();
    relogio_ms = 64 * 1000 + 15 * 64 - 1;
    r = remoto_tecla();
    printf("vigia_15 %u %u\n", r, ptt_c(12)[1] & 0x1000);
    relogio_ms = 64 * 1000 + 16 * 64 - 1;
    r = remoto_tecla();
    printf("vigia_16 %u %u %u\n", r, ptt_c(12)[1] & 0x1000, B(0x14BEE));

    /* 4. o PTT: a porta falsa com o PA12 em 0; sem ele, a de fabrica */
    limpa();
    quadro(0, 255, 1);
    H(0x1666C) = 7;
    remoto_cabo();
    printf("ptt %u %u\n", ptt_c(12)[1] & 0x1000, ptt_c(12) == (volatile unsigned *)porta_falsa);
    limpa();
    printf("ptt_solto %u %u\n", ptt_c(12)[1] & 0x1000, ptt_c(12) == (volatile unsigned *)porta_falsa);
    /* v157: o fio do PTT apertado (os bytes do PC no RX da serial); com o remoto pelo cabo nao vale, pelo BLE vale */
    porta_falsa[1] = 0;
    printf("fio_sem_remoto %u\n", ptt_c(12)[1] & 0x1000);
    quadro(0, 255, 0);
    memoria[0x16671 + 4] = memoria[0x16671 + 5] = 0xFF;   /* os reservados em 0xFF, como o app manda */
    memoria[0x16671 + 6] = (unsigned char)(memoria[0x16671 + 6] + 0xFE);
    H(0x1666C) = 7;
    r = remoto_cabo();
    printf("fio_cabo %u %u %u\n", r, B(0x14BEE), ptt_c(12)[1] & 0x1000);
    B(0x153A0) = 1;
    printf("fio_ble %u\n", ptt_c(12)[1] & 0x1000);
    B(0x153A0) = 0;
    quadro(0, 255, 1);
    remoto_cabo();
    printf("fio_cabo_ptt %u\n", ptt_c(12)[1] & 0x1000);
    porta_falsa[1] = 0x1000;

    /* 5. a tela inteira: pelo cabo, fora da TX, a principal ou o menu, e a perda fica para tras; pelo BLE, na TX ou
     * noutra tela, nada, e a fatia fica perdida (v158) */
    limpa();
    B(0x14BED) = 64;
    quadro(0, 255, 4);
    H(0x1666C) = 7;
    remoto_cabo();
    printf("tela_principal %u %u %u\n", tela_toda_vezes, motor_menu_vezes, B(0x14BED) & 64);
    limpa();
    B(0x1533D) = 1;
    quadro(0, 255, 4);
    H(0x1666C) = 7;
    remoto_cabo();
    printf("tela_menu %u %u %u\n", tela_toda_vezes, motor_menu_vezes, B(0x14BED) & 64);
    limpa();
    B(0x153A0) = 1;
    quadro(0, 255, 4);
    H(0x1666C) = 7;
    remoto_cabo();
    printf("tela_ble %u %u %u\n", tela_toda_vezes, motor_menu_vezes, B(0x14BED) & 64);
    limpa();
    B(0x1533C) = 1;
    quadro(0, 255, 4);
    H(0x1666C) = 7;
    remoto_cabo();
    printf("tela_tx %u %u\n", tela_toda_vezes, B(0x14BED) & 64);
    limpa();
    B(0x1533D) = 2;
    quadro(0, 255, 4);
    H(0x1666C) = 7;
    remoto_cabo();
    printf("tela_outra %u %u %u\n", tela_toda_vezes, motor_menu_vezes, B(0x14BED) & 64);
    limpa();
    B(0x14A35) = 1;                                /* v161: um digito digitado */
    quadro(0, 255, 4);
    H(0x1666C) = 7;
    remoto_cabo();
    printf("tela_digitando %u %u\n", tela_toda_vezes, B(0x14BED) & 64);
    limpa();
    B(0x14BED) = 64;
    quadro(0, 255, 2);
    H(0x1666C) = 7;
    remoto_cabo();
    printf("perda_fica %u\n", B(0x14BED) & 64);

    /* 6. quem recebe: (vista, evidencia B, dupla escuta no outro, rodizio na terceira) -> os bits crus (v159) */
    {
        static const unsigned casos[][4] = {
            {0, 0, 0, 0}, {0, 0, 1, 0}, {0, 1, 0, 0}, {0, 1, 1, 0},
            {2, 0, 1, 0}, {2, 0, 1, 1}, {3, 1, 0, 0}, {3, 0, 1, 0}, {3, 0, 1, 1}, {1, 0, 0, 0}};

        printf("quem");
        for (i = 0; i < sizeof casos / sizeof casos[0]; i++) {
            vista = casos[i][0];
            B(0x15E04) = (unsigned char)casos[i][1];
            B(0x15378) = (unsigned char)casos[i][2];
            c_lugar = casos[i][3];
            printf(" %u", quem());
        }
        printf("\n");
    }

    /* 7. o espelho: uma janela com corridas, o anel dando a volta no meio */
    limpa();
    anel(2400, 2300);
    B(0x14608) = 10;
    B(0x1460C) = 74;                               /* 64 de largura */
    B(0x1460A) = 40;
    B(0x1460E) = 48;                               /* 8 linhas */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 64; x++) {
            unsigned cor = x < 8 + y ? 0xF800 : (x / 5) % 2 ? 0x07E0 : 0x001F + y;

            memoria[0x10190 + (y * 64 + x) * 2] = (unsigned char)(cor >> 8);
            memoria[0x10190 + (y * 64 + x) * 2 + 1] = (unsigned char)cor;
        }
    quadro(0, 255, 2);
    H(0x1666C) = 7;
    remoto_cabo();
    r = remoto_envia();
    printf("espelho %u %u %u\n", r, travas, B(0x14BED) & 64);
    mostra_anel("anel", 2300);
    printf("video");
    for (i = 0; i < 64 * 8; i++)
        printf(" %u", memoria[0x10190 + i * 2] << 8 | memoria[0x10190 + i * 2 + 1]);
    printf("\n");

    /* 8. v158: o anel cheio perde a linha na hora, sem esperar (quem o esvazia nao roda enquanto a tela se desenha), e
     * o que ainda cabe vai */
    limpa();
    anel(2400, 0);
    H(0x151AC + 16) = 2390;
    B(0x14608) = 0;
    B(0x1460C) = 160;
    B(0x1460A) = 0;
    B(0x1460E) = 2;
    quadro(0, 255, 2);
    H(0x1666C) = 7;
    remoto_cabo();
    i = relogio_ms;
    remoto_envia();
    printf("anel_cheio %u %u %u\n", B(0x14BED) & 0x60, relogio_ms - i < 5, H(0x151AC + 16));
    H(0x151AC + 16) = 2400 - 488 - 10;             /* cabe uma linha de pior caso, e nao duas */
    B(0x14BED) &= ~64;
    remoto_envia();
    printf("anel_uma %u %u\n", B(0x14BED) & 64, H(0x151AC + 16) > 2400 - 488 - 10);

    /* 8b. v158: a fatia por no: o redesenho manda as 4 faixas, e so as linhas do no vao (o 11: 48 a 63; o 45: 52 a
     * 55; o 1: todas); o CONTA volta com o carimbo e sem a marca, e o vigia nao corre no meio (o carimbo velho nao
     * zera o REM, e o PTT do quadro, solto, vale) */
    {
        static const unsigned nos[] = {11, 45, 1};

        for (i = 0; i < 3; i++) {
            limpa();
            anel(2400, 0);
            redesenha = faixas;
            quadro_no(4, 2 | 4, nos[i]);
            H(0x1666C) = 7;
            remoto_cabo();
            redesenha = 0;
            printf("fatia%u %u %u %u %u %u\n", nos[i], tela_toda_vezes, B(0x14BEE), B(0x14BED) & 0xE0,
                   (B(0x14BED) & 31) == ((relogio_ms - 2000) >> 6 & 31), vivo_no_meio);
            mostra_anel(nos[i] == 11 ? "anel_fatia11" : nos[i] == 45 ? "anel_fatia45" : "anel_fatia1", 0);
        }
    }
    /* a que nao coube: perdida, e o CONTA sai da fatia */
    limpa();
    anel(2400, 0);
    H(0x151AC + 16) = 2000;
    redesenha = faixas;
    quadro_no(4, 2 | 4, 11);
    H(0x1666C) = 7;
    remoto_cabo();
    redesenha = 0;
    printf("fatia_cheia %u %u\n", B(0x14BED) & 0xE0, H(0x151AC + 16) == 2000);

    /* 8c. v159: a perda de fora das fatias marca o bit 5, que passa por uma fatia que coube, vai na resposta (o bit 5
     * do lado) e se limpa nela: a resposta seguinte ja nao o leva */
    limpa();
    anel(2400, 0);
    H(0x151AC + 16) = 2390;
    B(0x14608) = 0;
    B(0x1460C) = 160;
    B(0x1460A) = 0;
    B(0x1460E) = 1;
    quadro(0, 255, 2);
    H(0x1666C) = 7;
    remoto_cabo();
    remoto_tecla();                                /* a resposta deste quadro, antes da perda */
    remoto_envia();
    anel(2400, 0);
    redesenha = faixas;
    quadro_no(255, 2 | 4, 11);
    H(0x1666C) = 7;
    remoto_cabo();
    redesenha = 0;
    relogio_ms -= 2000;                            /* o faixas anda 2 s (o vigia no meio); aqui o quadro e novo */
    i = B(0x14BED) & 0x60;
    remoto_tecla();
    r = resposta[6];
    x = B(0x14BED) & 0x20;
    quadro(0, 255, 2);
    H(0x1666C) = 7;
    remoto_cabo();
    remoto_tecla();
    printf("sujo %u %u %u %u %u\n", i, r & 32, resposta[2] >> 7, x, resposta[6] & 32);

    /* 9. sem espelho: a lanterna segura o LCD; o BLE e a sessao nao recebem a tela */
    limpa();
    anel(2400, 0);
    B(0x14608) = 0;
    B(0x1460C) = 16;
    B(0x1460A) = 0;
    B(0x1460E) = 1;
    quadro(0, 255, 2);
    H(0x1666C) = 7;
    remoto_cabo();
    B(0x14BDA) = 0x88;
    r = remoto_envia();
    printf("lanterna %u %u\n", r, H(0x151AC + 16));
    B(0x14BDA) = 0;
    B(0x153A0) = 1;
    r = remoto_envia();
    printf("ble %u %u\n", r, H(0x151AC + 16));
    B(0x153A0) = 0;
    anel(2400, 0);                                 /* o que o caso do BLE pos no anel sai da conta */
    B(0x1666A) = 1;
    r = remoto_envia();
    printf("sessao %u %u\n", r, H(0x151AC + 16));
    B(0x1666A) = 0;
    B(0x14BEE) = 0;                                /* espelho desligado */
    r = remoto_envia();
    printf("desligado %u %u\n", r, H(0x151AC + 16));
    return 0;
}
