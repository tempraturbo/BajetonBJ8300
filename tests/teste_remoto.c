/* Os cenarios do controle remoto (firmware/src/remoto.c) no PC: o que o tests/teste_remoto.py confere. Cada linha
 * da saida e "nome valor..."; o que sai pelo caminho das respostas vai em "<nome> <hex>" para o Python decodificar. */
#include <stdio.h>
#define TESTE_REMOTO
#include "../firmware/src/remoto.c"

#define B(a)  memoria[a]
#define H(a)  (*(unsigned short *)(memoria + (a)))

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

static void limpa(void)
{
    memset(memoria, 0, sizeof memoria);
    magic_devolve = 7;
    magic_vezes = respostas = resposta_n = fluxo_n = maior = flash_fora = 0;
    tecla_fisica = 255;
    relogio_ms = 100000;
    passo_ms = 1;
    B(0x14778) = 1;                       /* a UART ligada */
}

static void mostra(const char *nome)
{
    unsigned i;

    printf("%s ", nome);
    for (i = 0; i < fluxo_n && i < sizeof fluxo; i++)
        printf("%02x", fluxo[i]);
    printf("\n");
}

/* A RAM e o chip com um padrao que muda byte a byte, e os valores do bancos.c com bytes diferentes uns dos outros. */
static void padrao(void)
{
    unsigned i;

    for (i = 0x7000; i < 0x16000; i++)
        memoria[i] = (unsigned char)(i * 7 + (i >> 8));
    for (i = 0; i < sizeof chip; i++)
        chip[i] = (unsigned char)(i * 13 + (i >> 9));
    vista = 0x80000103u;
    v_terceira = 0x0102002Au;             /* o canal 42, o MDF 2 e o bit 24 (a C em VFO) */
    v_freq = 0x08A1B2C3u;
    v_tudo = 0x00070203u;
    c_lugar = 1;
    v_outro = 1;
    v_nr = 3;
    v_porta = 4;
    v_canal = 1;
    v_lista = 6;
}

int main(void)
{
    unsigned r, i, k;

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
    limpa();
    B(0x14BED) = 0xE0;                             /* o que sobrou no CONTA (o RETRATO e os bits velhos) */
    relogio_ms = 64 * 777 - 1;
    quadro(0, 255, 0);
    H(0x1666C) = 7;
    remoto_cabo();
    printf("carimbo %u\n", B(0x14BED));

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

    /* 3. o vigia: solta com 1 s sem quadro (16 carimbos de 64 ms), com e sem o retrato pendente */
    for (k = 0; k < 2; k++) {
        limpa();
        relogio_ms = 64 * 1000 - 1;                /* a proxima leitura e o carimbo 1000 */
        quadro(0, 4, 1);
        H(0x1666C) = 7;
        remoto_cabo();
        B(0x14BED) |= (unsigned char)(k * 32);
        relogio_ms = 64 * 1000 + 15 * 64 - 1;
        r = remoto_tecla();
        printf("vigia_15_%u %u %u\n", k, r, ptt_c(12)[1] & 0x1000);
        relogio_ms = 64 * 1000 + 16 * 64 - 1;
        r = remoto_tecla();
        printf("vigia_16_%u %u %u %u\n", k, r, ptt_c(12)[1] & 0x1000, B(0x14BEE));
    }

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
    /* v166: o inicio da TX so desliga o BLE sem ligacao aberta (o handle em h[0x146D2]) */
    H(0x146D2) = 0;
    ble_tx();
    printf("ble_sem_ligacao %u\n", ble_desligou);
    H(0x146D2) = 0x50;
    ble_tx();
    printf("ble_com_ligacao %u\n", ble_desligou);
    H(0x146D2) = 0;

    /* 5. o retrato: pedido (o bit 2 do quadro), fica pendente na resposta e sai no remoto_retrato, uma vez so; o
     * remoto_retrato devolve a tecla que recebe */
    limpa();
    padrao();
    quadro(0, 255, 2);
    H(0x1666C) = 7;
    remoto_cabo();
    r = remoto_retrato(77);
    printf("antes_da_resposta %u %u\n", r, fluxo_n);
    remoto_tecla();
    printf("pendente %u %u %u\n", fluxo_n, B(0x14BED) & 32, B(0x14BEE) & 64);
    r = remoto_retrato(5);
    printf("retrato_sai %u %u %u %u\n", r, B(0x14BED) & 32, maior, flash_fora);
    printf("estado_no_envio %u %u\n", B(0x14BED), B(0x14BEE));   /* o CONTA e o REM, que o retrato leva */
    mostra("fluxo");
    i = fluxo_n;
    remoto_retrato(5);
    remoto_tecla();
    remoto_retrato(5);
    printf("uma_vez %u\n", fluxo_n == i);
    printf("freq_pedida %u\n", freq_pedida == v_terceira);

    /* a terceira no canal 0xFFFF (nenhum): a flash do canal vem do 0; e a memoria do radio em cada banda */
    for (k = 0; k < 7; k++) {
        static const unsigned char bandas[7] = {0, 1, 2, 3, 4, 5, 255};

        limpa();
        padrao();
        if (k == 0)
            v_terceira = 0x0000FFFFu;
        B(0x12A1B) = bandas[k];
        B(0x14CB4) = (unsigned char)(k * 3 + 1);
        quadro(0, 255, 2);
        H(0x1666C) = 7;
        remoto_cabo();
        remoto_tecla();
        remoto_retrato(0);
        printf("banda%u ", bandas[k]);
        mostra("");
    }

    /* sem o pedido, nada; e o pedido de um quadro que um novo quadro sem pedido substitui antes da volta ao laco */
    limpa();
    padrao();
    quadro(0, 255, 0);
    H(0x1666C) = 7;
    remoto_cabo();
    remoto_tecla();
    remoto_retrato(0);
    printf("sem_pedido %u\n", fluxo_n);
    limpa();
    padrao();
    quadro(0, 255, 2);
    H(0x1666C) = 7;
    remoto_cabo();
    remoto_tecla();
    quadro(0, 255, 0);
    remoto_cabo();
    remoto_retrato(0);
    printf("substituido %u %u\n", fluxo_n, B(0x14BED) & 32);

    /* 6. quem recebe: (vista, evidencia B, dupla escuta no outro, rodizio na terceira) -> os bits crus (v159) */
    {
        static const unsigned casos[][4] = {
            {0, 0, 0, 0}, {0, 0, 1, 0}, {0, 1, 0, 0}, {0, 1, 1, 0},
            {2, 0, 1, 0}, {2, 0, 1, 1}, {3, 1, 0, 0}, {3, 0, 1, 0}, {3, 0, 1, 1}, {1, 0, 0, 0}};

        limpa();
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

    /* 7. o envio ao LCD: a lanterna o segura, e a de fabrica segue sem ela */
    limpa();
    B(0x14BDA) = 0x88;
    r = remoto_envia();
    B(0x14BDA) = 0x08;
    printf("lanterna %u %u\n", r, remoto_envia());

    /* 8. a tela apagada (v164): o bit 4 do quadro vira o ESCURO (o 6 do CONTA); a tecla do radio, o ACORDADO (o 7), que
     * o quadro seguinte guarda; e o vigia apaga o ESCURO com o REM */
    limpa();
    relogio_ms = 64 * 2000 - 1;
    quadro(0, 255, 4);
    H(0x1666C) = 7;
    remoto_cabo();
    printf("escuro %u %u\n", B(0x14BED) & 0xC0, B(0x14BED) & 31);
    r = remoto_tecla();                            /* nenhuma tecla do radio */
    printf("escuro_remota %u %u\n", r, B(0x14BED) & 0xC0);
    tecla_fisica = 6;
    r = remoto_tecla();
    printf("escuro_fisica %u %u\n", r, B(0x14BED) & 0xC0);
    tecla_fisica = 255;
    quadro(0, 255, 4);
    H(0x1666C) = 7;
    remoto_cabo();
    printf("escuro_guarda %u\n", B(0x14BED) & 0xC0);
    B(0x14BEE) |= 128;                             /* a resposta pendente: ela diz que acordou no escuro (v165) */
    remoto_tecla();
    printf("escuro_avisa %u\n", resposta[2] & 128);
    quadro(0, 255, 0);                             /* o PC acende: o ESCURO sai, o ACORDADO fica neste quadro */
    H(0x1666C) = 7;
    remoto_cabo();
    printf("escuro_desliga %u\n", B(0x14BED) & 0xC0);
    remoto_tecla();
    printf("escuro_desliga_avisa %u\n", resposta[2] & 128);
    quadro(0, 255, 0);                             /* e sai no seguinte */
    H(0x1666C) = 7;
    remoto_cabo();
    printf("escuro_segue_aceso %u\n", B(0x14BED) & 0xC0);
    tecla_fisica = 6;                              /* a tecla do radio sem o escuro: acorda, mas o quadro seguinte o tira */
    remoto_tecla();
    tecla_fisica = 255;
    quadro(0, 255, 4);                             /* o pedido novo: apaga na hora, sem o ACORDADO velho (v165) */
    H(0x1666C) = 7;
    remoto_cabo();
    printf("escuro_de_novo %u\n", B(0x14BED) & 0xC0);
    remoto_tecla();
    printf("escuro_de_novo_avisa %u\n", resposta[2] & 128);
    limpa();
    relogio_ms = 64 * 3000 - 1;
    quadro(0, 255, 4);
    H(0x1666C) = 7;
    remoto_cabo();
    relogio_ms = 64 * 3000 + 15 * 64 - 1;
    remoto_tecla();
    printf("escuro_15 %u\n", B(0x14BED) & 0xC0);
    relogio_ms = 64 * 3000 + 16 * 64 - 1;
    remoto_tecla();
    printf("escuro_16 %u %u\n", B(0x14BED) & 0xC0, B(0x14BEE));
    relogio_ms = 64 * 3000 + 33 * 64 - 1;          /* o bit 4 do tempo volta a 0 aos 32 carimbos: o ESCURO nao volta */
    remoto_tecla();
    printf("escuro_33 %u\n", B(0x14BED) & 0xC0);
    return 0;
}
