"""Confere a busca do firmware (v167 e v168: a espera por canal e o THRESHOLD do firmware/src/bancos.c) no PC: o bloco
da busca sai do proprio bancos.c e roda sobre uma imitacao da tarefa de fabrica, como o desmontado a mostra
(docs/engenharia-reversa.md, secao 40):

  - a tarefa (0x01E46AC0), com o passo em [0x148BC]: no 1, a medida (busca_sinal no lugar de 0x01E44818); aberta, a
    conta (b[0x14691]) sobe, e na quarta seguida a busca para; fechada, o avanco (o gancho_scan no lugar de
    0x01E458E4) e a conta a 0. No 2 (a retomada TO) e no 4, o mesmo gancho;
  - a parada no TO: o passo 2 e h[0x14618] = 50 (0x01E480D8 e 0x01E46ECC);
  - o avanco de fabrica (0x01E458E4): volta se h[0x14618] nao e 0; senao sintoniza o proximo canal, poe o passo em 1 e,
    no lugar do "h = 2", chama o busca_sintonizou;
  - o tique de 100 ms (0x01E417B4), que desce o h[0x14618] se nao e 0; e o relogio de 2 em 2 ms (b[0x14810]);
  - as setas (0x01E4C2F4): h[0x14618] = 0 e o avanco.

O squelch imitado abre no canal do sinal um tempo depois da sintonia. Confere, com a espera (ESPERA_CANAL) tirada do
bancos.c: sem sinal, cada canal fica da espera a ela mais o intervalo da tarefa (a cada 25 ms e a cada 50); o sinal
que abre o squelch antes da espera acabar para a busca, no canal dele, e o que abre depois passa; parada, a retomada TO segue 5 s depois; a seta da a espera inteira ao
canal novo; o THRESHOLD pula o sinal abaixo do S escolhido; a hora guardada nunca e 0. Precisa do zig (pip install
ziglang). Uso: python -I tests/teste_busca.py
"""
import os
import re
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BANCOS = os.path.join(RAIZ, 'firmware', 'src', 'bancos.c')
falhas = []


def confere(nome, obtido, esperado):
    ok = obtido == esperado
    print(f'{nome}: {"ok" if ok else "FALHOU"}')
    if not ok:
        print(f'    obtido:   {obtido!r}\n    esperado: {esperado!r}')
        falhas.append(nome)


fonte = open(BANCOS, encoding='utf-8').read()
i = fonte.index('#define BUSCA_PASSO')
bloco = fonte[i:fonte.index('\n}\n', fonte.index('unsigned busca_sinal(void)', i)) + 3]
i = fonte.index('void gancho_scan(void)')
bloco += fonte[i:fonte.index('\n}\n', i) + 3]
# os enderecos da RAM do radio viram posicoes de um vetor
bloco = re.sub(r'\(\*\(volatile (u8|u16|u32) \*\)0x([0-9A-Fa-f]+)\)', r'(*(volatile \1 *)(ram + 0x\2 - 0x14000))', bloco)

programa = r'''#include <stdio.h>
#include <stdlib.h>
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
static u8 ram[0x2000] __attribute__((aligned(4)));
static unsigned redesenhos, pedido;  /* os pedidos de redesenho que o gancho_scan faz, um por chamada da tarefa */
#define REDESENHA1 (*(volatile u8 *)(ram + 0x15367 - 0x14000))
#define REDESENHA2 pedido
#define FIRMWARE(nome, endereco)
#define NA_LIVRE_COD
#define DADO_NA_LIVRE
#define VALOR (*(volatile u16 *)(ram + 0x14BFC - 0x14000))
#define H     (*(volatile u16 *)(ram + 0x14618 - 0x14000))
#define PASSO (*(volatile u32 *)(ram + 0x148BC - 0x14000))
#define CONTA (*(volatile u8 *)(ram + 0x14691 - 0x14000))
#define TIQUE (*(volatile u32 *)(ram + 0x14810 - 0x14000))
static unsigned canal, sintonia, alvo = 1000, abre_ms, segmentos = 24, sintonias;
unsigned segmentos_rssi(void) { return segmentos; }
unsigned SQL_ABERTO(void) { return canal == alvo && TIQUE - sintonia >= abre_ms; }
u32 busca_sintonizou(void);
void AVANCA_SCAN(void)              /* 0x01E458E4 */
{
    if (H != 0)
        return;
    canal++;
    sintonia = TIQUE;
    sintonias++;
    PASSO = 1;
    busca_sintonizou();
}
void AVANCA_NOAA(void) { AVANCA_SCAN(); }
static unsigned busca_espera(void);
''' + bloco + r'''
static unsigned parou;
static void tarefa(void)            /* 0x01E46AC0, no modo 3 */
{
    switch (PASSO) {
    case 1:
        if (busca_sinal() == 1) {
            if (CONTA > 2) {        /* 0x01E47748, o TO: 0x01E480D8 e 0x01E46ECC */
                PASSO = 2;
                H = 50;
                parou = canal;
                return;
            }
            CONTA++;
            return;
        }
        gancho_scan();
        CONTA = 0;
        return;
    case 2:
    case 4:
        gancho_scan();
        CONTA = 0;
        return;
    }
}
/* roda ms milissegundos (ate parar, com para_no_sinal): o relogio de 2 em 2, o tique a cada 100 e a tarefa a cada
 * cada_ms */
static void roda(unsigned ms, unsigned cada_ms, int para_no_sinal)
{
    static unsigned t;
    for (unsigned fim = t + ms; t < fim && !(para_no_sinal && parou); t++) {
        TIQUE = (t & ~1u) + 0xFF00;  /* perto da volta dos 16 bits, de proposito */
        if (t % 100 == 0 && H)
            H--;
        if (t % cada_ms == 0) {
            tarefa();
            redesenhos += pedido;
            pedido = 0;
        }
    }
}
int main(int argc, char **argv)
{
    unsigned cada = atoi(argv[1]), modo = atoi(argv[2]);
    alvo = atoi(argv[3]);
    abre_ms = atoi(argv[4]);
    if (argc > 5) {
        *(volatile u8 *)(ram + 0x14BCE - 0x14000) = (u8)atoi(argv[5]);
        segmentos = atoi(argv[6]);
    }
    PASSO = 4;                      /* a busca comeca avancando */
    if (modo == 0) {                /* sem sinal: o tempo por canal */
        roda(10000, cada, 0);
        printf("ms_por_canal %u\n", 10000 * 10 / sintonias);
        printf("redesenhos %u\n", redesenhos == sintonias);
    } else if (modo == 1) {         /* com o sinal: onde para, e a retomada */
        roda(10000, cada, 1);
        printf("parou %u\n", parou);
        unsigned antes = canal;
        alvo = 100000;              /* o sinal some; o TO segue 5 s depois de parar */
        roda(4800, cada, 0);
        printf("antes_de_5s %u\n", canal == antes);
        roda(400, cada, 0);
        printf("depois_de_5s %u\n", canal != antes);
    } else {                        /* a seta: h = 0 e o avanco, e a espera inteira no canal novo */
        roda(1010, cada, 0);
        H = 0;
        AVANCA_SCAN();
        unsigned c = canal;
        roda(ESPERA_CANAL - 10, cada, 0);
        printf("seta_espera %u\n", canal == c);
        roda(40, cada, 0);
        printf("seta_sai %u\n", canal != c);
    }
    printf("hora %u %u\n", busca_sintonizou(), H & 1);
    return 0;
}
'''

with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as pasta:
    c = os.path.join(pasta, 'busca.c')
    exe = os.path.join(pasta, 'busca.exe')
    open(c, 'w', encoding='utf-8').write(programa)
    subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-O1', '-w', '-o', exe, c], check=True)

    def roda(*args):
        saida = subprocess.run([exe] + [str(a) for a in args], capture_output=True, text=True, check=True).stdout
        return {l.split()[0]: [int(x) for x in l.split()[1:]] for l in saida.splitlines()}

    E = int(re.search(r'#define ESPERA_CANAL\s+(\d+)u', fonte).group(1))
    print(f'a espera por canal: {E} ms')
    for cada in (25, 50):
        r = roda(cada, 0, 100000, 0)
        confere(f'sem sinal, com a tarefa a cada {cada} ms, cada canal fica de {E} a {E + cada} ms',
                E * 10 <= r['ms_por_canal'][0] <= (E + cada) * 10, True)
        confere(f'  o busca_sintonizou devolve 0, e a hora guardada e impar (nunca 0)', r['hora'], [0, 1])
        confere(f'  um redesenho pedido por canal, nao por medida', r['redesenhos'], [1])
    for abre in (E * 2 // 5, E * 3 // 5, E - 10):
        r = roda(25, 1, 20, abre)
        confere(f'o squelch abrindo {abre} ms depois da sintonia para a busca no canal do sinal', r['parou'], [20])
        confere('  parada, a retomada TO espera os 5 s', (r['antes_de_5s'], r['depois_de_5s']), ([1], [1]))
    confere(f'o squelch abrindo {E + 30} ms depois (mais que a espera) passa', roda(25, 1, 20, E + 30)['parou'], [0])
    r = roda(25, 2, 100000, 0)
    confere('a seta da ao canal novo a espera inteira', (r['seta_espera'], r['seta_sai']), ([1], [1]))
    # o THRESHOLD: 0xD0 + o S - 1; S n com 2n - 1 segmentos ou mais
    confere('S5, sinal de 8 segmentos (S4): pula', roda(25, 1, 20, 40, 0xD4, 8)['parou'], [0])
    confere('S5, sinal de 9 segmentos (S5): para', roda(25, 1, 20, 40, 0xD4, 9)['parou'], [20])
    confere('S1 (0xD0), sinal de 1 segmento: para', roda(25, 1, 20, 40, 0xD0, 1)['parou'], [20])
    confere('sem a marca (0xFF, o S1), sinal de 1 segmento: para', roda(25, 1, 20, 40, 0xFF, 1)['parou'], [20])

print('\ntodos os testes passaram' if not falhas else f'\n{len(falhas)} FALHARAM: ' + ', '.join(falhas))
sys.exit(1 if falhas else 0)
