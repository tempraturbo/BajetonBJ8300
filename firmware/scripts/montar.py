"""Monta o firmware do BJ-8300 com os bancos de canais.

Uso: python -I montar.py <original.anc> <bancos.bin> <saida.anc> [simbolos.txt]

Os enderecos das funcoes novas sao lidos do arquivo de simbolos gerado pelo llvm-nm (por padrao
build/simbolos.txt, que o build.sh escreve). Antes eles ficavam escritos aqui, e bastava recompilar
para o menu passar a apontar para o lugar errado sem ninguem perceber.

O que faz:
  1. grava o codigo novo no espaco livre do fim de cada imagem de flash;
  2. grava a tabela do menu principal nova NO LUGAR da original - os itens de fabrica, menos o
     ZONE e o Zone Or Channel (a pedido do usuario), com os tres dos bancos no lugar deles - e fixa
     a quantidade de itens;
  3. troca as chamadas (e a instrucao do espectro) em que os ganchos entram: partida, scan, cabo,
     realce, limpeza e teclas do espectro; desvia a entrada das treze rotinas da tela principal para
     a tela B (TELA_DESVIOS); inverte as setas do menu e revisa os textos;
  4. grava o espectro no espaco das vozes (so no C0 e no C1) e a tela B no lugar do decodificador
     G.729, dentro do app.bin, zerando o tipo dele no registro dos decodificadores;
  5. recalcula todos os CRCs e escreve o .anc.

Dentro do app.bin so muda o que esta na lista de escritas, que passa pela trava antes; o
conferir.py prova depois que nenhum byte mudou fora dela.

TRAVA DE SEGURANCA: recusa qualquer escrita que caia nas funcoes que o radio executa antes de
checar o combo PTT2+atalho, ou que fazem o modo UPDATE funcionar. Elas sao o caminho de
recuperacao pelo cabo: se um firmware ruim quebrasse esse caminho, o radio so voltaria abrindo o
aparelho. Os limites foram lidos no Ghidra e estao em PROTEGIDAS.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jlfw

BASE_APP = 0x01E00100

PROTEGIDAS = [
    (0x01E57472, 0x01E57A40, 'main do app: inicializa o hardware e escolhe o modo'),
    (0x01E56BB6, 0x01E56CF7, 'inicializacao do app: le as teclas do combo de recuperacao'),
    (0x01E432FC, 0x01E43355, 'desenha a tela UPDATE'),
    (0x01E4DEA8, 0x01E4DF60, 'tick periodico que decide se a interface roda'),
    (0x01E55AF4, 0x01E55C99, 'parte do modo UPDATE'),
    (0x01E3CFD2, 0x01E3CFD5, 'parte do modo UPDATE'),
]

# Onde o codigo novo roda (igual nas 6 imagens; ver bancos.ld). Tem de ser o endereco para onde o
# linker ligou o .text - conferido no ELF por codigo_ligado(), porque na v20 o linker alinhou a
# secao 2 bytes adiante e o radio entrou em bootloop.
ENDERECO_CODIGO = 0x01E96124

# Vozes fora (v30, a pedido do dono). Os sons moram no diretorio "tone" logo depois do app.bin:
# index.idx, as vozes C0..C49 (chines) e E0..E67 (ingles) e os bipes B0..B7. Toda voz toca por um de
# tres caminhos (mapeados em docs/engenharia-reversa.md, secao 14), e cada um vira o "sem voz" que ja
# existe nele (sitio, bytes de fabrica, bytes novos, o que e):
#  - a fila de sons (0x01E405E2), a unica que le a tabela de caminhos (0x01E5E8FC, lida so em
#    0x01E40634): "ifs (r5 >= 118) {" vira "if (r5 < 118) goto" o retorno - numero de voz (0..117)
#    sai da fila sem tocar; os bipes (118..125) seguem como eram, inclusive o bloco que o ifs guardava;
#  - a rotina terminal que o main escolhe (0x01E55AAE) e toca o C16 se b[0x153F3]: o desvio do caso
#    "sem voz" vira incondicional;
#  - a intermediaria dos eventos (0x01E555C6, chamada so pelo tratador 0x01E55740 com C1, C2, C3 e
#    C15): o desvio que volta sem tocar vira incondicional.
# Codificacoes conferidas no llvm-objdump; nenhum desvio cai no meio dos trechos.
VOZES_REMENDOS = (
    (0x01E40628, bytes.fromhex('35e97680'), bytes.fromhex('85f91cec'), 'fila de sons: voz sai sem tocar'),
    (0x01E55ABE, bytes.fromhex('0153'), bytes.fromhex('0493'), 'C16 do main: sempre sem voz'),
    (0x01E555D6, bytes.fromhex('0350'), bytes.fromhex('0490'), 'vozes dos eventos: sempre volta sem tocar'),
)
# O espaco dos arquivos de voz, do C0 (0x01E7979C, alinhado a 4) ate o fim do E67. Mesmo endereco nas
# 6 imagens (o diretorio "tone" vem logo depois do app.bin, que tem o mesmo tamanho em todas).
VOZES, VOZES_FIM = 0x01E797A0, 0x01E956F0
# As rotinas que o firmware chama e que moram ali desde a v33: as do espectro, porque todo caminho
# ate os sitios delas exige o modo de tela 20 (docs/engenharia-reversa.md, secao 15). O conferir.py
# exige que estas, e so estas, estejam no .vozes, e que so os sitios do espectro desviem para la.
# O carregador de atualizacao recusa a imagem inteira se os arquivos de voz C0..C11 mudam (v35); com
# so o C0 e o C1 mudando (o espectro cabe neles) ele grava (v33, v34).
NAS_VOZES = ('gancho_tecla_espectro', 'gancho_espectro', 'gancho_registra', 'gancho_avanca',
             'gancho_janela', 'gancho_coluna', 'mostra_pico', 'espectro_pontos', 'espectro_passo',
             'espectro_rotulos')
VOZES_ACEITAS_FIM = 0x01E79BCA          # o fim do C1: o .vozes nao passa dele

# O decodificador G.729 (v36): o das vozes .wtg, que nao tocam mais. Codigo e tabelas dele ocupam
# G729..G729_FIM, dentro do app.bin, e ali mora o tela.o (secao .g729 do bancos.ld). Provado no
# desmontado de fabrica (docs/engenharia-reversa.md, secao 16):
#  - a unica porta e o registro dos decodificadores (G729_REGISTRO, 3 de 68 B), lido so pelas duas
#    buscas 0x01E2F1D2 (tipo exato) e 0x01E2F2CE (mascara), que so chamam um decodificador cujo
#    tipo bate com o pedido;
#  - o tipo 0x40 so e pedido pelo tocador de sons (0x01E40318) para arquivo que nao e .sin: a
#    extensao vira tipo pela tabela 0x01E60D40 (wtg = 0x40), e o .sin vai ao decodificador 0x100;
#    no diretorio de sons, os unicos que nao sao .sin sao as vozes;
#  - nenhum desvio, imediato ou valor de 32 bits de fora da faixa aponta para dentro dela, a nao
#    ser o registro (os dez "desvios" que o desmontador mostra sao tabela que nada alcanca).
# A trava a mais: o tipo do G.729 no registro vira 0, que nenhuma das buscas casa (a exata recebe
# sempre um bit so; a de mascara exige tipo & mascara != 0). Assim ele nao e escolhido nem por um
# caminho que tenha escapado do mapa - um pedido de .wtg sai pelo "sem decodificador" de fabrica.
G729, G729_FIM = 0x01E257E0, 0x01E278C8
G729_REGISTRO = 0x01E34838
G729_TIPO_ANTES, G729_TIPO_DEPOIS = bytes.fromhex('40000000'), bytes(4)
G729_ENTRADAS = (0x01E257E0, 0x01E25838, 0x01E2587E, 0x01E258C8)   # os 4 primeiros ponteiros do registro

# A tela principal B (v35, src/tela.c): treze rotinas de fabrica que desenham a tela de dois VFOs
# ganham um desvio de 4 bytes na entrada (goto para o desvio_* do tela.c, no lugar do G.729). O
# desvio chama o gancho em C (b_*) com os argumentos da rotina; se ele desenhou a tela B, volta a quem
# chamou com r0..r3 intactos; se nao, refaz as instrucoes de fabrica que o goto cobriu e segue a
# rotina. (entrada, bytes de fabrica cobertos - instrucoes inteiras, sem nada relativo ao endereco -,
# desvio, gancho em C, o que e.) Conferido no desmontado de fabrica: nenhum desvio nem constante cai
# dentro dos bytes cobertos, e nenhuma das treze le argumento da pilha de quem chama.
TELA_DESVIOS = (
    (0x01E3C132, bytes.fromhex('7704e29e'), 'desvio_tela', 'b_tela', 'a tela de dois VFOs inteira'),
    (0x01E3B05C, bytes.fromhex('7604e29d'), 'desvio_barra', 'b_barra', 'a barra de cima'),
    (0x01E3B03E, bytes.fromhex('c1ff80460100'), 'desvio_nivel_bateria', 'b_barra', 'o nivel da bateria'),
    (0x01E3AE50, bytes.fromhex('7504e29d'), 'desvio_cadeado', 'b_barra', 'o icone do cadeado'),
    (0x01E3AF1E, bytes.fromhex('7904e29a'), 'desvio_bateria', 'b_bateria', 'a pilha (e a bateria fraca piscando)'),
    (0x01E3AEB0, bytes.fromhex('7604e29d'), 'desvio_monitor', 'b_monitor', 'o icone MO'),
    (0x01E453E0, bytes.fromhex('760440e09217'), 'desvio_evidencia', 'b_evidencia', 'o lado em evidencia (troca de canal)'),
    (0x01E4B29C, bytes.fromhex('7504e29b'), 'desvio_numero', 'b_digitando', 'o numero de canal sendo digitado'),
    (0x01E4B1DE, bytes.fromhex('7804e29a'), 'desvio_freq_digitada', 'b_digitando', 'a frequencia sendo digitada'),
    (0x01E413BE, bytes.fromhex('7604e29e'), 'desvio_lado', 'b_lado', 'um lado'),
    (0x01E3B2C4, bytes.fromhex('7a04e29c'), 'desvio_informacoes', 'b_lado', 'a linha de informacoes'),
    (0x01E3B592, bytes.fromhex('7c04e29c'), 'desvio_medidor', 'b_medidor', 'o medidor (RX)'),
    (0x01E40F1C, bytes.fromhex('7604e29e'), 'desvio_transmissao', 'b_transmissao', 'a transmissao (inicio e fim)'),
)
TELA_SIMBOLOS = tuple(d for _, _, d, _, _ in TELA_DESVIOS) + tuple(sorted({g for _, _, _, g, _ in TELA_DESVIOS}))
# O que o firmware chama no tela.o: os desvios, os ganchos e o envio e a limpeza da tela principal.
NO_G729 = TELA_SIMBOLOS + ('gancho_envia', 'gancho_limpa')

PRECISA = ('gancho_cabo', 'gancho_scan', 'gancho_envia', 'gancho_limpa', 'gancho_tecla_espectro',
           'gancho_espectro', 'gancho_registra', 'gancho_avanca', 'gancho_janela', 'gancho_coluna', 'mostra_pico',
           'espectro_pontos', 'espectro_passo', 'espectro_rotulos',
           'roger_le', 'roger_grava', 'roger_opcoes', 'roger_opcoes_cn', 'gancho_roger_tons',
           'scramble_le', 'scramble_grava', 'scramble_freqs', 'scramble_valor', 'scramble_posicao',
           'scramble_opcoes', 'scramble_opcoes_cn', 'bank_exibe', 'bank_le', 'bank_grava', 'nome_exibe', 'nome_le', 'nome_grava',
           'gancho_partida', 'canal_exibe', 'canal_le', 'canal_grava',
           'titulo_bank', 'titulo_nome', 'titulo_canal') + TELA_SIMBOLOS


def codigo_ligado(elf, secao='.text'):
    """(endereco, conteudo) de uma secao do bancos.elf, lidos do cabecalho de secoes do ELF32."""
    e = open(elf, 'rb').read()
    shoff = struct.unpack_from('<I', e, 0x20)[0]
    tam, quantas, nomes = struct.unpack_from('<HHH', e, 0x2E)
    secoes = [struct.unpack_from('<10I', e, shoff + i * tam) for i in range(quantas)]
    base_nomes = secoes[nomes][4]
    for nome, _, _, endereco, deslocamento, tamanho, *_ in secoes:
        if e[base_nomes + nome:e.index(b'\0', base_nomes + nome)] == secao.encode():
            return endereco, e[deslocamento:deslocamento + tamanho]
    raise SystemExit(f'{elf} nao tem secao {secao}')


def secao_ligada(codigo, secao, inicio, fim):
    """Uma secao do bancos.elf, conferida: ligada para inicio e cabendo ate fim."""
    endereco, conteudo = codigo_ligado(os.path.splitext(codigo)[0] + '.elf', secao)
    if endereco != inicio or endereco + len(conteudo) > fim:
        raise SystemExit(f'{secao} ligada para {endereco:#x}..{endereco + len(conteudo):#x}, fora de '
                         f'{inicio:#x}..{fim:#x}. Recusado.')
    return conteudo


def confere_ligacao(codigo, blob):
    """O bloco gravado em ENDERECO_CODIGO tem de ser o .text, ligado exatamente para ali."""
    elf = os.path.splitext(codigo)[0] + '.elf'
    endereco, conteudo = codigo_ligado(elf)
    if endereco != ENDERECO_CODIGO:
        raise SystemExit(f'o .text foi ligado para {endereco:#x}, mas o codigo e gravado em '
                         f'{ENDERECO_CODIGO:#x}: todo gancho cairia fora do lugar. Recusado.')
    if conteudo != blob:
        raise SystemExit(f'{codigo} nao e o .text de {elf}. Recusado.')
    return endereco


def le_simbolos(caminho):
    """Le a saida do llvm-nm ("endereco T nome") e confere que nao falta nenhum simbolo."""
    tabela = {}
    with open(caminho) as f:
        for linha in f:
            partes = linha.split()
            if len(partes) == 3 and partes[1] in 'Tt':
                tabela[partes[2]] = int(partes[0], 16)
    faltando = [n for n in PRECISA if n not in tabela]
    if faltando:
        raise SystemExit(f'faltam simbolos em {caminho}: {", ".join(faltando)}')
    return tabela


S = {}   # preenchido em main() a partir do arquivo de simbolos

# Campos de um item de menu (28 bytes, compactado, confirmado no motor 0x01E489E0):
#   +0 flags | +1 valor maximo (u16) | +3 get | +7 set | +0xb nome | +0xf nome_cn
#   +0x13 exibe (ou lista de opcoes em ingles) | +0x17 opcoes em chines | +0x1b id do item
# Os dois ultimos campos nao sao tocados: ficam como estavam no item aproveitado. O motor le o
# maximo como u16 (h[0x14C0E]) e aceita 960 - e o que ele mesmo faz com o MEMCH em Full Channel.
CAMPO = {'get': 3, 'set': 7, 'nome': 0xB, 'nome_cn': 0xF, 'exibe': 0x13, 'opcoes_cn': 0x17}
FLAGS_NUMERO = 0x80   # +0x13 e funcao de exibicao, chamada por linha desenhada (como era a zona)
FLAGS_EDITOR = 0x07   # abre o editor de texto (como o CH-NAME)

# A tabela viva do menu principal, com 53 itens de 28 bytes (o item 0 e o SQL), seguida do item
# zerado que a encerra. Quem a carrega e o laco da interface, em PONTEIRO_EM, com "r0 = 13368;
# r5 = r6 + r0" (r6 = 0x01E5FB60) - por ser soma de registradores, procurar o endereco pronto no
# desmontado nao acha nada.
#
# A tabela nova (54 itens) e gravada NO LUGAR da original: 53 itens + o item zerado = 54 vagas,
# exatamente. Ate a v18 ela ia para a area livre da imagem, depois do codigo, com o ponteiro trocado
# - e ocupava 1512 dos 3294 bytes da area livre. Provado em 2026-10-24 que mais nada le a tabela
# original: o unico acesso e este ponteiro (a unica carga de 13368 no firmware); nenhum ponteiro de
# dados nem constante de 32 bits cai dentro dela; as outras duas contagens de itens (0x01E4B3FE,
# 0x01E4C3C4) apontam a estrutura do menu para OUTRAS tabelas antes de contar; e as tres constantes
# com valor na faixa de deslocamentos (13720, 13912, 14357) sao conta e registradores do chip de
# radio, nao enderecos. O ponteiro fica como esta e so e conferido.
MENU_NOVO = True          # False = menu de fabrica (sem os itens BANK): corte de diagnostico
TABELA = 0x01E62F98
ITENS_ORIGINAIS = 53
TAM_ITEM = 0x1C
VAGAS = ITENS_ORIGINAIS + 1                        # os itens de fabrica + o item zerado do fim
PONTEIRO_EM = 0x01E4CBB2
PONTEIRO_ANTES = bytes.fromhex('40e03834651c')     # r0 = 13368 ; r5 = r6 + r0

# Como o firmware conta os itens varrendo a tabela ate o item zerado, e a tabela nova ocupa a vaga
# dele, a contagem e trocada por uma constante na propria chamada que a pedia (4 bytes, no lugar
# exato) - assim a varredura nunca passa do fim.
CONTAGEM_EM = 0x01E4CBE8
CONTAGEM_ANTES = bytes.fromhex('bfea17ee')         # call FUN_01E4A81A (conta itens da tabela)

# Itens dos bancos na tabela nova. Eles entram JUNTOS, no lugar do Zone Or Channel e do ZONE (que
# saem, ver ITENS_FORA), com todos os outros itens de fabrica. Cada um copia o molde de um item do
# mesmo tipo que funciona (o
# ZONE para numero com exibicao propria, o CH-NAME para o editor de texto), o que traz de graca os
# campos que nao mexemos: opcoes em chines (+0x17) e numero do aviso falado (+0x1B, 0 nos dois
# moldes = nao fala nada).
ZONE, ZONA_OU_CANAL, CH_NAME = 11, 10, 19
# Itens de fabrica que NAO entram na copia, a pedido do usuario: o ZONE e o "Zone Or Channel"
# (escolher entre navegar por zona ou por canal cheio), que sem o ZONE nao tem mais serventia. Com
# eles fora, os 54 itens da tabela nova cabem exatamente nas 54 vagas da original (ver VAGAS).
# O VOICE (indice 21, liga e desliga as vozes) sai tambem (v30): as vozes nao tocam mais.
VOICE_ITEM = 21
ITENS_FORA = (ZONA_OU_CANAL, ZONE, VOICE_ITEM)
# Itens de fabrica que ficam, mas com campos trocados na tabela nova. O ROGER (indice 20) ganha o
# BEEP 2 e o BEEP 3: quantidade de opcoes 5 (o "maximo" de um item de lista e a quantidade de
# opcoes - era 3), get e set nossos (posicao no menu <-> valor gravado) e as duas tabelas de nomes
# das opcoes, em ingles e chines (as de fabrica moram na RAM, em 0x6E3C/0x6E30, com outros dados
# colados depois - nao crescem no lugar). Detalhes no bancos.c.
ROGER_ITEM = 20
# O SCRAMBLE (indice 47) passa a ter 9 opcoes, OFF e de 2700 a 3400 Hz em ordem crescente, com get
# e set nossos (posicao no menu <-> valor gravado; o set grava pelo de fabrica). Detalhes no bancos.c.
SCRAMBLE_ITEM = 47
ITENS_MUDADOS = {ROGER_ITEM: dict(maximo=5, get='roger_le', set='roger_grava',
                                  exibe='roger_opcoes', opcoes_cn='roger_opcoes_cn'),
                 SCRAMBLE_ITEM: dict(maximo=9, get='scramble_le', set='scramble_grava',
                                     exibe='scramble_opcoes', opcoes_cn='scramble_opcoes_cn')}

# Na rotina que aplica o scramble (0x01E408E2), o trecho que escolhe a frequencia de inversao:
#   0x01E4092E  40 21              r0 = 1
#   0x01E40930  34 e8 01 00        if (r4 == 1) {
#   0x01E40934  40 20                  r0 = 0 }
#   0x01E40936  c1 ff 18 8b e6 01  r1 = 0x01E68B18          (tabela de fabrica, 2 entradas)
#   0x01E4093C  d8 ed 18 10        r1 = h[r1 + r0<<1]       (fica: le a frequencia)
# r4 e o valor gravado no canal (chega em r0 e a rotina o guarda em r4, que a chamada de 0x01E4092A
# preserva). Vira "r0 = r4 & 0xF; nop; r1 = scramble_freqs", e a frequencia sai da nossa tabela
# pelo valor. As codificacoes sao as do proprio firmware: 40 16 (r0 = r4, 0x01E005E6), 60 e1 0f 00
# (r0 = r0 & 0xF, 0x01E00350) e 00 00 (nop, 0x01E0019A). A tabela de fabrica fica intacta.
SCRAMBLE_EM = 0x01E4092E
SCRAMBLE_ANTES = bytes.fromhex('4021' '34e80100' '4020' 'c1ff188be601')


def monta_scramble():
    """Os 14 bytes novos do trecho que escolhe a frequencia de inversao."""
    return bytes.fromhex('4016' '60e10f00' '0000' 'c1ff') + struct.pack('<I', S['scramble_freqs'])
ITENS = [
    dict(nome='BANK', molde=ZONE, flags=FLAGS_NUMERO, maximo=16,
         get='bank_le', set='bank_grava', titulo='titulo_bank', exibe='bank_exibe'),
    dict(nome='BANK NAME', molde=CH_NAME, flags=FLAGS_EDITOR, maximo=0,
         get='nome_le', set='nome_grava', titulo='titulo_nome', exibe='nome_exibe'),
    dict(nome='BANK CH', molde=ZONE, flags=FLAGS_NUMERO, maximo=960,
         get='canal_le', set='canal_grava', titulo='titulo_canal', exibe='canal_exibe'),
]

# ---- revisao dos textos em ingles do menu (ver notas/textos-revisao.md) ----
# As cadeias vem coladas na flash, sem folga: trocar no lugar so pode ter tamanho igual ou menor
# (o resto vai a zero, e o texto termina no zero). O padrao e CAIXA ALTA, que e o que 90% dos
# textos ja usa, com a unidade fisica mantendo a grafia certa (Hz). Duas cadeias do STEP sao
# sufixo de outras - "2.5 K" dentro de "12.5 K" e "5.0 K" dentro de "25.0 K" -, entao so a maior
# entra aqui e a menor sai corrigida junto.
TEXTOS_REVISADOS = True
TEXTOS = [
    (0x01E5A560, 'Middle', 'MIDDLE'),
    (0x01E5A4A0, 'Normal Mode', 'NORMAL MODE'),
    (0x01E5A4AC, 'Super  Mode', 'SUPER MODE'),
    (0x01E5A4B8, 'Deep   Mode', 'DEEP MODE'),
    (0x01E5A581, '6.25 K', '6.25K'),
    (0x01E5A588, '8.33 K', '8.33K'),
    (0x01E5A58F, '10.0 K', '10.0K'),
    (0x01E5A596, '12.5 K', '12.5K'),
    (0x01E5A59D, '20.0 K', '20.0K'),
    (0x01E5A5A4, '25.0 K', '25.0K'),
    (0x01E5A5AB, '50.0 K', '50.0K'),
    (0x01E5A7B3, 'Zone Or Channel', 'ZONE OR CHANNEL'),
    (0x01E5A71A, 'Zone Mode', 'ZONE MODE'),
    (0x01E5A724, 'Full Channel', 'FULL CHANNEL'),
    (0x01E5A8F4, 'POWER ON MSG', 'POWER-ON MSG'),
    (0x01E5A6A1, 'Delet', 'DEL'),
    (0x01E5A6A7, 'Add', 'ADD'),
    (0x01E5A613, '1000hz', '1000Hz'),
    (0x01E5A61A, '1450hz', '1450Hz'),
    (0x01E5A621, '1750hz', '1750Hz'),
    (0x01E5A628, '2100hz', '2100Hz'),
    (0x01E5A60F, 'All', 'ALL'),
    (0x01E5AA81, 'RX Modulation', 'RX MODULATION'),
    # versao do firmware, que o item VERSION mostra (o hardware V01T32 fica como esta)
    (0x01E5FD10, 'V0.12', 'V1.0T'),
    # o nome chines do item e 语音加密 (criptografia de voz): VOICEPRI engana
    (0x01E5A82F, 'VOICEPRI', 'ENCRYPT'),
]

# Chamada no fim do carregamento de configuracao, trocada pelo gancho que aplica o filtro.
GANCHO_EM = 0x01E56AE8
GANCHO_ERA = 0x01E3951C

# Gancho do cabo: as DUAS chamadas que o tratador faz para a rotina de dados 0x01E43970 - a do 'R'
# (le, argumento 0) e a do 'W' (grava, argumento 1). O gancho repassa o argumento, entao um unico
# gancho atende os dois. Com o tamanho de sempre (bit 7 em zero) nada muda e o CHIRP continua
# funcionando; com o bit 7 ligado, os outros 7 bits do tamanho sao o terceiro byte do endereco e o
# bloco e de 64 B - a leitura alcanca o chip inteiro e a gravacao so a area dos bancos.
CABO_ESTENDIDO = True
CABO_SITIOS = ((0x01E4BB66, 'R, leitura'), (0x01E4C0FA, 'W, gravacao'))
CABO_ERA = 0x01E43970

# Gancho do scan: a chamada que avanca o canal durante a varredura passa a pedir o redesenho da
# tela, senao a barra de sinal fica no nivel do ultimo canal achado e o numero descasa do nome.
SCAN_REDESENHA = True
SCAN_EM = 0x01E46EB6
SCAN_ERA = 0x01E458E4

# Gancho das teclas do espectro: parado num sinal achado, a seta continua a varredura em vez de
# mexer no limite. Entra no lugar do "r1 = r0 - 16" que escolhe o caso do tbh das teclas do modo
# 20 (0x01E4BFBE, destino do caso 20 do tbh dos modos em 0x01E4B988 e de nenhum outro desvio).
# Nao e chamada trocada: e uma instrucao de 4 bytes que vira uma chamada de 4 bytes, e o gancho
# devolve em r0 e r1 o que a instrucao deixava. r2 e r3, que a chamada passa a sujar, estao
# mortos dali em diante em todos os caminhos (conferido no desmontado, inclusive dentro das
# rotinas chamadas antes de qualquer escrita deles).
ESPECTRO_TECLAS = True
ESPECTRO_EM = 0x01E4BFBE
ESPECTRO_ANTES = bytes.fromhex('31e1f00f')         # r1 = r0 + -16

# O resto do espectro (passo de 10 kHz, 320 e 640 pontos, um ponto por volta do laco, pico seguro):
# chamadas trocadas pelos ganchos (sitio, para onde ia, gancho) - todas call de 4 bytes - e tres
# rotinas trocadas inteiras por um goto de 4 bytes na entrada (a rotina nossa volta direto para
# quem chamou, porque o goto nao mexe no rets). Ninguem desvia para o meio dessas tres rotinas:
# todo desvio para elas cai na entrada (conferido no desmontado). Detalhes no bancos.c.
ESPECTRO_CHAMADAS = (
    (0x01E4D5B2, 0x01E4AEAC, 'gancho_espectro', 'varredura no laco principal: um ponto por volta'),
    (0x01E4AEE0, 0x01E4AD7C, 'gancho_registra', 'registro do ponto, parado num achado'),
    (0x01E4AF7A, 0x01E4AD7C, 'gancho_registra', 'registro do ponto, varrendo'),
    (0x01E4AF86, 0x01E4AE44, 'gancho_avanca', 'proximo ponto: varredura e retomada'),
    (0x01E4C6B8, 0x01E4AE44, 'gancho_avanca', 'proximo ponto depois da tecla 2'),
    (0x01E4C054, 0x01E4AA3C, 'gancho_janela', 'janela pela frequencia digitada'),
    (0x01E4CFBC, 0x01E4AA3C, 'gancho_janela', 'janela na entrada do espectro'),
    (0x01E4AEA4, 0x01E45D96, 'mostra_pico', 'pico da passada, na segunda linha'),
    (0x01E4D600, 0x01E4AAA8, 'gancho_coluna', 'barras da faixa 0 do grafico'),
    (0x01E4D644, 0x01E4AAA8, 'gancho_coluna', 'barras da faixa 1 do grafico'),
    (0x01E4D688, 0x01E4AAA8, 'gancho_coluna', 'barras da faixa 2 do grafico'),
    (0x01E4D6CC, 0x01E4AAA8, 'gancho_coluna', 'barras da faixa 3 do grafico'),
    (0x01E4D710, 0x01E4AAA8, 'gancho_coluna', 'barras da faixa 4 do grafico'),
)
# O dBm (0x01E4ADFE) sai do meio da segunda linha do espectro, que passa a ser do pico da passada,
# para a direita dela, embaixo do AM/FM (que vai ate y 31: janela 127..159 x 17..49, texto em y 20).
# A janela vai de x 40..120 para 112..160, na mesma altura (36..48), e o texto de x 56 para 116:
# "-111dBm", 7 caracteres de 6 px, termina em 158. E a janela do AM/FM (0x01E4B04C, chamada so pela
# tecla 23 e pela entrada do espectro) ia ate y 49 e limpava de preto o dBm a partir de x 127; o
# texto dela e de 12 px em y 20, entao ela passa a terminar em 33, como a do "x160" (v34). Quatro
# "rX = imm8" de 2 bytes (sitio, registrador, de fabrica, novo, o que e).
DBM_IMEDIATOS = (
    (0x01E4AE12, 0, 40, 112, 'x0 da janela'),
    (0x01E4AE14, 1, 120, 160, 'x1 da janela'),
    (0x01E4AE30, 1, 56, 116, 'x do texto'),
    (0x01E4B066, 3, 49, 33, 'y1 da janela do AM/FM, que apagava o dBm'),
)
# Roger BEEP 2 e BEEP 3, na rotina de fim de transmissao 0x01E44604 (sitio, bytes de fabrica, o que vira):
#  - 0x01E4461E: "if (r0 != 1) goto 388" vira "if (r0 == 0) goto 388" (mesmo destino, 0x01E447A6):
#    os valores 3 e 4 passam a entrar no caminho do BEEP; o 2 ja foi desviado antes, para o TONE1200.
#    Codificacao conferida no llvm-objdump ("00 f8 c2 00" = if (r0 == 0) goto 388).
#  - 0x01E44638: "r5 = r4 + 2051; r6 = 17; goto 18" (o comeco do laco dos dois tons) vira
#    "call gancho_roger_tons; goto 0x01E44656" (o fim do laco). Ninguem de fora desvia para dentro
#    do laco; r5 e r6 sao escritos antes de lidos dali em diante; r0..r3 ja chegavam ali como lixo
#    de chamada (o laco termina numa espera).
ROGER_DESVIO_EM = 0x01E4461E
ROGER_DESVIO_ANTES, ROGER_DESVIO_DEPOIS = bytes.fromhex('80f8c202'), bytes.fromhex('00f8c200')
ROGER_LACO_EM, ROGER_LACO_FIM = 0x01E44638, 0x01E44656
ROGER_LACO_ANTES = bytes.fromhex('05e1034846310489')

ESPECTRO_FUNCOES = (
    (0x01E4A9C2, bytes.fromhex('c0ffc06e'), 'espectro_pontos', 'quantos pontos a varredura tem'),
    (0x01E4A99C, bytes.fromhex('c0ffc06e'), 'espectro_passo', 'passo entre colunas'),
    (0x01E4ABDC, bytes.fromhex('7904e29b'), 'espectro_rotulos', 'rotulos dos pontos e do passo'),
)

# Realce do bloco do VFO em evidencia na tela principal. O gancho entra no ENVIO do pedaco para o
# LCD (envia(), 0x01E3ADBC) e nao no preenchimento da janela, porque o fundo do texto nao vem de
# janela(): as rotinas de texto enchem a celula de cada caractere com uma cor propria, preta fixa.
# Trocar so a cor da janela - o que a v14 fazia - deixava nome e frequencia em caixas pretas.
# Detalhes e enderecos em docs/mapa-da-tela.md; a cor fica em COR_REALCE, no bancos.c.
#
# Os sitios abaixo sao TODAS as chamadas de envia() dentro das seis funcoes que desenham a tela de
# dois VFOs, lidas uma por uma no desmontado. Sao call de 4 bytes trocadas no lugar: nao cresce o
# codigo. O gancho ainda confere na rotina do firmware (0x01E3B1A2) se a tela e mesmo a de dois
# VFOs, entao nenhuma outra tela e pintada - nem a de um VFO so, que usa estas mesmas funcoes.
#
# O que faltava na v14 estava aqui: ela so tinha os tres sitios de atualizacao PARCIAL (barra,
# numero e nome), que correm quando se troca de canal. O redesenho completo - que e o que acontece
# ao trocar de A para B - passa por 0x01E3C132 e pelo desenhador de bloco 0x01E3BD04, que nao
# tinham gancho nenhum. Era por isso que o verde so aparecia na barra e so ao mudar de canal.
REALCE = True
REALCE_ERA = 0x01E3ADBC
REALCE_SITIOS_GANCHO = (
    # desenhador de bloco (0x01E3BD04), um bloco por lado: frequencia, nome e o que o modo pedir
    (0x01E3BF38, 'bloco: modo 2, linha do nome'),
    (0x01E3BF8E, 'bloco: modos 1 e 3, linha do nome'),
    (0x01E3C014, 'bloco: modo 4, frequencia'),
    (0x01E3C066, 'bloco: frequencia (saida comum)'),
    # complemento do bloco (0x01E3C06E): a faixa da direita, y = 56/95 no lado em evidencia e
    # 19/91 no compacto - os mesmos limites, por outra tabela (0x01E3C0BE). O primeiro sitio so
    # corre na tela de um VFO (com r0 == 0 o desvio em 0x01E3C07C pula janela e envio); fica na
    # lista por completude, e o gancho nao pinta nada la.
    (0x01E3C0A4, 'complemento: marca do lado, (0,18,41,55), so na tela de um VFO'),
    (0x01E3C12A, 'complemento: faixa da direita, (132,160,y,y+22)'),
    # linha de informacoes do lado em evidencia (0x01E3B2C4: o "H" de potencia e companhia), y = 18
    # com o A e 57 com o B; so e desenhada para o lado expandido (layout 0 e 3 pulam janela e envio)
    (0x01E3B4AE, 'informacoes: linha do H, (0,160,y,y+12), y=18 no A e 57 no B'),
    # tela principal (0x01E3C132): as tres faixas de barra de sinal
    (0x01E3C176, 'tela: barra do A compacto, (0,160,41,55)'),
    (0x01E3C19E, 'tela: barra do A expandido, (0,160,74,88)'),
    (0x01E3C1D0, 'tela: barra do B, (0,160,114,128)'),
    # recepcao e transmissao (0x01E40F1C com argumento 1 ou 2): a barra do lado que recebe ou
    # transmite e redesenhada em janela propria, com os segmentos e o texto RX/TX em x=127; e,
    # no modo frequencia, a linha grande tambem
    (0x01E3B7CA, 'RX/TX: barra e texto RX/TX, janela propria (19,140,y,y+9), y=77 no A e 115 no B'),
    (0x01E41008, 'RX/TX: linha grande no modo frequencia, (0,160,y,y+24), y=30 no A e 69 no B'),
    # atualizacoes parciais, que correm ao trocar de canal
    (0x01E41452, 'parcial: barra de sinal'),
    (0x01E4147E, 'parcial: barra de sinal (nivel)'),
    (0x01E4B294, 'parcial: nome e frequencia, y=30 no A e 69 no B'),
    (0x01E4B322, 'parcial: numero do canal, y=62 no A e 101 no B'),
)
# As janelas nao cobrem a area inteira do lado em evidencia: entre o nome grande e a frequencia, e
# entre a frequencia e a barra, sobram faixas que ninguem desenha e que a limpeza deixa pretas. Por
# isso as duas chamadas que limpam a tela e a redesenham inteira passam pelo gancho_limpa, que deixa
# a limpeza de fabrica correr (preto e a linha separadora) e enche de verde a area do lado em
# evidencia - 72 linhas a partir de 16 (A) ou 56 (B), a mesma area que a limpeza usa. A entrada do
# menu tambem limpa no modo 2, mas por outra chamada (0x01E4CBAE), e nao e tocada.
LIMPA_ERA = 0x01E3ACBA
LIMPA_SITIOS = (
    (0x01E3C294, 'limpa e redesenha (0x01E3C290): partida, saida do menu e mais 14 chamadores'),
    (0x01E40F66, 'limpa e redesenha na troca de lado (0x01E40F1C)'),
)
# Diagnostico (pintava faixas inteiras trocando so a cor, 2 bytes por sitio) fica desligado: serviu
# para descobrir qual chamada pinta o que. Os sitios conferidos estao no historico do git.
REALCE_DIAG = False
REALCE_SITIOS = ()

# Sentido das setas no menu. De fabrica a seta de cima AVANCA o valor/item - que esta desenhado
# mais abaixo na tela - e a de baixo retrocede: logico pelo numero, estranho pela posicao. As duas
# rotinas abaixo atendem as duas telas (lista de itens quando b[0x14BF9] != 1, lista de valores
# quando == 1) e so mexem na estrutura do menu em 0x14BF4, entao inverter nao afeta o sobe/desce
# de canal da tela inicial nem a busca. Sao quatro sitios de chamada (duas de cada), um par por
# contexto de tecla; trocando os quatro, cada contexto passa a andar para o outro lado. O editor
# de texto tem caminho proprio no tratador e nao passa por aqui.
SETAS_INVERTIDAS = True
AVANCA, RETROCEDE = 0x01E49F60, 0x01E49FF4
SETAS = ((0x01E4D094, AVANCA), (0x01E4D12A, AVANCA),
         (0x01E4D0A4, RETROCEDE), (0x01E4D0AA, RETROCEDE))


def monta_call(origem, destino):
    """Codifica a instrucao de chamada do pi32v2 (4 bytes, deslocamento de 23 bits em palavras)."""
    d = (destino - origem - 4) // 2
    if not -0x200000 <= d < 0x200000:
        raise SystemExit(f'chamada de {origem:#x} para {destino:#x} esta longe demais')
    d &= 0x3FFFFF
    return bytes([0x80 | ((d >> 16) & 0x3F), 0xEA, d & 0xFF, (d >> 8) & 0xFF])


def monta_goto(origem, destino):
    """O desvio de 4 bytes: mesmo formato da chamada, com 0xC0 no lugar de 0x80 no primeiro byte
    (conferido contra o llvm-objdump: "c0 ea 79 08" = goto 4338, "ff ea c7 f5" = goto -5234)."""
    return bytes([0x40 | monta_call(origem, destino)[0]]) + monta_call(origem, destino)[1:]


def confere_trava(escritas):
    for endereco, tamanho, descricao in escritas:
        for ini, fim, oque in PROTEGIDAS:
            if endereco <= fim and ini <= endereco + tamanho - 1:
                raise SystemExit(f'TRAVA: "{descricao}" escreveria em {endereco:#x}..'
                                 f'{endereco + tamanho - 1:#x}, dentro de {ini:#x}..{fim:#x} ({oque}). Recusado.')
    print(f'trava: {len(escritas)} escritas conferidas contra {len(PROTEGIDAS)} areas protegidas - ok')


def monta_imm8(reg, valor):
    """Instrucao de 2 bytes "rX = imm8" (codificacao conferida em scripts/retangulos.py)."""
    if not 0 <= reg <= 7 or not 0 <= valor <= 0xFF:
        raise SystemExit(f'r{reg} = {valor} nao cabe na instrucao de 2 bytes')
    return bytes((0x40 | ((valor >> 5) & 7) << 3 | reg, 0x20 | (valor & 0x1F)))


def ordem_da_copia():
    """Posicoes da copia: os itens de fabrica (menos o ZONE) com os dos bancos no lugar dele.

    Cada elemento e o indice do item original ou o dicionario de um item novo.

    Saem dois itens de fabrica, a pedido do usuario: o **ZONE** (indice 11) e o **Zone Or Channel**
    (indice 10), que escolhe entre navegar por zona ou por canal cheio - sem o ZONE ele nao tem mais
    serventia. O banco faz o papel dos dois, com muito mais liberdade. Com os dois fora, a tabela
    nova tem 54 itens e cabe exatamente nas 54 vagas da original. As FUNCOES continuam no
    firmware; o que sai sao os itens de menu delas.
    """
    de_fabrica = [i for i in range(ITENS_ORIGINAIS) if i not in ITENS_FORA]
    return de_fabrica[:ZONA_OU_CANAL] + list(ITENS) + de_fabrica[ZONA_OU_CANAL:]


def monta_tabela(app):
    """Copia da tabela do menu principal, com os itens dos bancos no meio e nada perdido."""
    inicio = TABELA - BASE_APP
    original = app[inicio:inicio + ITENS_ORIGINAIS * TAM_ITEM]
    tabela = bytearray()
    for quem in ordem_da_copia():
        if isinstance(quem, int):
            o = len(tabela)
            tabela += original[quem * TAM_ITEM:(quem + 1) * TAM_ITEM]
            for campo, valor in ITENS_MUDADOS.get(quem, {}).items():
                if campo == 'maximo':
                    struct.pack_into('<H', tabela, o + 1, valor)
                else:
                    struct.pack_into('<I', tabela, o + CAMPO[campo], S[valor])
            continue
        o = len(tabela)
        tabela += original[quem['molde'] * TAM_ITEM:(quem['molde'] + 1) * TAM_ITEM]
        tabela[o] = quem['flags']
        struct.pack_into('<H', tabela, o + 1, quem['maximo'])
        for campo, chave in (('get', quem['get']), ('set', quem['set']), ('nome', quem['titulo']),
                             ('nome_cn', quem['titulo']), ('exibe', quem['exibe'])):
            struct.pack_into('<I', tabela, o + CAMPO[campo], S[chave])
    return bytes(tabela)


def monta_contagem(itens):
    """Instrucao de 4 bytes que poe a quantidade de itens em r0, no lugar da chamada que a contava."""
    return b'\x40\xe0' + struct.pack('<H', itens)


def main():
    origem, codigo, destino = sys.argv[1], sys.argv[2], sys.argv[3]
    simbolos = sys.argv[4] if len(sys.argv) > 4 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'simbolos.txt')
    S.update(le_simbolos(simbolos))
    print(f'simbolos lidos de {os.path.normpath(simbolos)}: {len(S)}')

    dados = jlfw.kdh_scramble(open(origem, 'rb').read())
    _, entradas = jlfw.ufw_entries(dados)
    img = next(e for e in entradas if e['type'] == 0)
    flash = dados[img['offset']:img['offset'] + img['size']]
    chave = jlfw.find_chipkey(flash)
    _, base, sub = jlfw.app_dir(flash, chave)
    ent = next(s for s in sub if s['name'] == 'app.bin')
    app = bytearray(jlfw.sfc(flash[base + ent['offset']:base + ent['offset'] + ent['size']],
                             chave, ent['offset']))

    blob = open(codigo, 'rb').read()
    print(f'  .text ligado para {confere_ligacao(codigo, blob):#x} = onde o codigo e gravado: ok')
    tela = secao_ligada(codigo, '.g729', G729, G729_FIM)
    vozes = secao_ligada(codigo, '.vozes', VOZES, VOZES_ACEITAS_FIM)
    tabela = monta_tabela(app)

    # Monta a lista de escritas no app.bin e passa pela trava antes de tocar em qualquer byte.
    escritas = [(GANCHO_EM, 4, 'gancho do filtro na partida')]
    if SCAN_REDESENHA:
        escritas.append((SCAN_EM, 4, 'gancho do redesenho no scan'))
    if ESPECTRO_TECLAS:
        escritas.append((ESPECTRO_EM, len(ESPECTRO_ANTES), 'gancho das teclas do espectro'))
        escritas += [(s, 4, f'espectro: {d}') for s, _, _, d in ESPECTRO_CHAMADAS]
        escritas += [(e, 4, f'espectro: {d}') for e, _, _, d in ESPECTRO_FUNCOES]
    escritas += [(e, 2, f'dBm: {d}') for e, _, _, _, d in DBM_IMEDIATOS]
    escritas += [(s, len(a), f'vozes: {d}') for s, a, _, d in VOZES_REMENDOS]
    escritas += [(ROGER_DESVIO_EM, 4, 'roger: valores 3 e 4 no caminho do BEEP'),
                 (ROGER_LACO_EM, len(ROGER_LACO_ANTES), 'roger: laco dos tons'),
                 (SCRAMBLE_EM, len(SCRAMBLE_ANTES), 'scramble: frequencia pelo valor')]
    if CABO_ESTENDIDO:
        escritas += [(s, 4, f'gancho do cabo ({p})') for s, p in CABO_SITIOS]
    if MENU_NOVO:
        escritas += [(TABELA, len(tabela), 'tabela de menus, no lugar da original'),
                     (CONTAGEM_EM, len(CONTAGEM_ANTES), 'quantidade de itens do menu')]
    if SETAS_INVERTIDAS:
        escritas += [(s, 4, 'sentido das setas no menu') for s, _ in SETAS]
    if REALCE_DIAG:
        escritas += [(e, 2, 'cor de faixa (diagnostico)') for e, _ in REALCE_SITIOS]
    if REALCE:
        escritas += [(e, 4, f'gancho do realce ({o})') for e, o in REALCE_SITIOS_GANCHO]
    escritas += [(e, 4, f'tela B: {o}') for e, _, _, _, o in TELA_DESVIOS]
    escritas += [(G729, len(tela), 'tela B no lugar do G.729'),
                 (G729_REGISTRO, len(G729_TIPO_ANTES), 'tipo do G.729 no registro dos decodificadores')]
    if TEXTOS_REVISADOS:
        escritas += [(e, len(v) + 1, f'texto "{v}"') for e, v, _ in TEXTOS]
    confere_trava(escritas)

    # 1. gancho do filtro
    o = GANCHO_EM - BASE_APP
    antes = bytes(app[o:o + 4])
    if antes != monta_call(GANCHO_EM, GANCHO_ERA):
        raise SystemExit(f'{GANCHO_EM:#x} nao e a chamada esperada para {GANCHO_ERA:#x}: {antes.hex()}')
    app[o:o + 4] = monta_call(GANCHO_EM, S['gancho_partida'])
    print(f"  gancho {GANCHO_EM:#x}: chamada para {GANCHO_ERA:#x} -> {S['gancho_partida']:#x}")

    if CABO_ESTENDIDO:
        for sitio, papel in CABO_SITIOS:
            o = sitio - BASE_APP
            antes = bytes(app[o:o + 4])
            if antes != monta_call(sitio, CABO_ERA):
                raise SystemExit(f'{sitio:#x} nao e a chamada esperada para {CABO_ERA:#x}: {antes.hex()}')
            app[o:o + 4] = monta_call(sitio, S['gancho_cabo'])
            print(f"  gancho do cabo {sitio:#x} ({papel}): chamada para {CABO_ERA:#x} -> "
                  f"{S['gancho_cabo']:#x}")

    if SCAN_REDESENHA:
        o = SCAN_EM - BASE_APP
        antes = bytes(app[o:o + 4])
        if antes != monta_call(SCAN_EM, SCAN_ERA):
            raise SystemExit(f'{SCAN_EM:#x} nao e a chamada esperada para {SCAN_ERA:#x}: {antes.hex()}')
        app[o:o + 4] = monta_call(SCAN_EM, S['gancho_scan'])
        print(f"  gancho do scan {SCAN_EM:#x}: chamada para {SCAN_ERA:#x} -> {S['gancho_scan']:#x} "
              '(pede redesenho a cada canal da varredura)')

    if ESPECTRO_TECLAS:
        o = ESPECTRO_EM - BASE_APP
        antes = bytes(app[o:o + len(ESPECTRO_ANTES)])
        if antes != ESPECTRO_ANTES:
            raise SystemExit(f'{ESPECTRO_EM:#x} nao e o "r1 = r0 - 16" das teclas do espectro: {antes.hex()}')
        app[o:o + 4] = monta_call(ESPECTRO_EM, S['gancho_tecla_espectro'])
        print(f"  gancho das teclas do espectro {ESPECTRO_EM:#x} -> {S['gancho_tecla_espectro']:#x} "
              '(setas parado num achado, OK e #)')
        for sitio, era, gancho, oque in ESPECTRO_CHAMADAS:
            o = sitio - BASE_APP
            antes = bytes(app[o:o + 4])
            if antes != monta_call(sitio, era):
                raise SystemExit(f'{sitio:#x} nao e a chamada esperada para {era:#x}: {antes.hex()}')
            app[o:o + 4] = monta_call(sitio, S[gancho])
            print(f'  espectro {sitio:#x}: chamada para {era:#x} -> {gancho} ({oque})')
        for entrada, antes_esperado, nova, oque in ESPECTRO_FUNCOES:
            o = entrada - BASE_APP
            if bytes(app[o:o + 4]) != antes_esperado:
                raise SystemExit(f'{entrada:#x} nao comeca como esperado: {bytes(app[o:o + 4]).hex()}')
            app[o:o + 4] = monta_goto(entrada, S[nova])
            print(f'  espectro {entrada:#x}: rotina trocada por {nova} ({oque})')

    # 3b. o dBm vai para a direita da segunda linha do espectro
    for endereco, reg, antes, depois, oque in DBM_IMEDIATOS:
        o = endereco - BASE_APP
        if bytes(app[o:o + 2]) != monta_imm8(reg, antes):
            raise SystemExit(f'{endereco:#x} nao e "r{reg} = {antes}": {bytes(app[o:o + 2]).hex()}')
        app[o:o + 2] = monta_imm8(reg, depois)
        print(f'  dBm do espectro {endereco:#x}: {oque} {antes} -> {depois}')

    # 3b2. vozes fora: os tres caminhos que tocam voz viram o "sem voz" de cada um
    for sitio, antes, depois, oque in VOZES_REMENDOS:
        o = sitio - BASE_APP
        if bytes(app[o:o + len(antes)]) != antes:
            raise SystemExit(f'{sitio:#x} nao e o esperado: {bytes(app[o:o + len(antes)]).hex()}')
        app[o:o + len(antes)] = depois
        print(f'  vozes {sitio:#x}: {oque} ({antes.hex()} -> {depois.hex()})')

    # 3c. roger BEEP 2 e 3: os valores 3 e 4 entram no caminho do BEEP, e o laco dos tons e nosso
    o = ROGER_DESVIO_EM - BASE_APP
    if bytes(app[o:o + 4]) != ROGER_DESVIO_ANTES:
        raise SystemExit(f'{ROGER_DESVIO_EM:#x} nao e o desvio esperado: {bytes(app[o:o + 4]).hex()}')
    app[o:o + 4] = ROGER_DESVIO_DEPOIS
    o = ROGER_LACO_EM - BASE_APP
    if bytes(app[o:o + len(ROGER_LACO_ANTES)]) != ROGER_LACO_ANTES:
        raise SystemExit(f'{ROGER_LACO_EM:#x} nao e o laco esperado: {bytes(app[o:o + 8]).hex()}')
    app[o:o + 8] = (monta_call(ROGER_LACO_EM, S['gancho_roger_tons'])
                    + monta_goto(ROGER_LACO_EM + 4, ROGER_LACO_FIM))
    print(f"  roger: valores 3 e 4 no caminho do BEEP ({ROGER_DESVIO_EM:#x}) e laco dos tons -> "
          f"gancho_roger_tons ({ROGER_LACO_EM:#x}, depois desvia para {ROGER_LACO_FIM:#x})")

    # 3d. scramble: a frequencia de inversao sai da nossa tabela, pelo valor gravado
    o = SCRAMBLE_EM - BASE_APP
    if bytes(app[o:o + len(SCRAMBLE_ANTES)]) != SCRAMBLE_ANTES:
        raise SystemExit(f'{SCRAMBLE_EM:#x} nao e a escolha da frequencia do scramble: '
                         f'{bytes(app[o:o + len(SCRAMBLE_ANTES)]).hex()}')
    app[o:o + len(SCRAMBLE_ANTES)] = monta_scramble()
    print(f"  scramble: frequencia de inversao pelo valor, de scramble_freqs ({S['scramble_freqs']:#x}), "
          f'em {SCRAMBLE_EM:#x}')

    # 2 e 3. tabela de menus no lugar da original, e a quantidade de itens fixa.
    # Com MENU_NOVO desligado nada disso entra: o menu fica o de fabrica (sem os itens BANK), o
    # que serve de corte de diagnostico - o gancho, o filtro, as setas e os textos continuam.
    itens = len(tabela) // TAM_ITEM
    if MENU_NOVO:
        o = PONTEIRO_EM - BASE_APP
        if bytes(app[o:o + len(PONTEIRO_ANTES)]) != PONTEIRO_ANTES:
            raise SystemExit(f'{PONTEIRO_EM:#x} nao e o calculo esperado do endereco da tabela')
        if itens > VAGAS:
            raise SystemExit(f'a tabela tem {itens} itens e a original so tem {VAGAS} vagas')
        o = TABELA - BASE_APP
        fim = o + ITENS_ORIGINAIS * TAM_ITEM
        if any(app[fim:fim + TAM_ITEM]):
            raise SystemExit(f'{BASE_APP + fim:#x} nao e o item zerado que encerra a tabela')
        app[o:o + len(tabela)] = tabela
        print(f'  tabela de menus em {TABELA:#x}, no lugar da original ({itens} itens: os '
              f'{ITENS_ORIGINAIS - len(ITENS_FORA)} de fabrica que ficam e os {len(ITENS)} dos bancos)')
        for pos, quem in enumerate(ordem_da_copia()):
            if not isinstance(quem, int):
                print(f"    item {pos:2d}: \"{quem['nome']}\" (max {quem['maximo']}, "
                      f"flags {quem['flags']:#04x}, no lugar dos itens {ZONA_OU_CANAL} e {ZONE} de fabrica)")

        o = CONTAGEM_EM - BASE_APP
        antes = bytes(app[o:o + len(CONTAGEM_ANTES)])
        if antes != CONTAGEM_ANTES:
            raise SystemExit(f'{CONTAGEM_EM:#x} nao e a chamada esperada da contagem: {antes.hex()}')
        app[o:o + len(CONTAGEM_ANTES)] = monta_contagem(itens)
        print(f'  quantidade de itens {CONTAGEM_EM:#x}: contagem na tabela -> {itens} fixo')
    else:
        print('  MENU_NOVO desligado: menu de fabrica, sem os itens BANK (corte de diagnostico)')

    # 4. sentido das setas no menu
    if SETAS_INVERTIDAS:
        for sitio, era in SETAS:
            o = sitio - BASE_APP
            antes = bytes(app[o:o + 4])
            if antes != monta_call(sitio, era):
                raise SystemExit(f'{sitio:#x} nao e a chamada esperada para {era:#x}: {antes.hex()}')
            app[o:o + 4] = monta_call(sitio, RETROCEDE if era == AVANCA else AVANCA)
        print(f'  setas do menu invertidas em {len(SETAS)} sitios: a de cima passa a subir na lista')

    # 4b. realce do VFO selecionado: o gancho entra na chamada que pinta a faixa da barra
    if REALCE:
        for sitio, oque in REALCE_SITIOS_GANCHO:
            o = sitio - BASE_APP
            antes = bytes(app[o:o + 4])
            if antes != monta_call(sitio, REALCE_ERA):
                raise SystemExit(f'{sitio:#x} nao e a chamada esperada para {REALCE_ERA:#x}: '
                                 f'{antes.hex()}')
            app[o:o + 4] = monta_call(sitio, S['gancho_envia'])
            print(f"  gancho do realce {sitio:#x} -> {S['gancho_envia']:#x}: {oque}")
        for sitio, oque in LIMPA_SITIOS:
            o = sitio - BASE_APP
            antes = bytes(app[o:o + 4])
            if antes != monta_call(sitio, LIMPA_ERA):
                raise SystemExit(f'{sitio:#x} nao e a chamada esperada para {LIMPA_ERA:#x}: '
                                 f'{antes.hex()}')
            app[o:o + 4] = monta_call(sitio, S['gancho_limpa'])
            print(f"  gancho da limpeza {sitio:#x} -> {S['gancho_limpa']:#x}: {oque}")

    # 4b2. a tela B: um goto na entrada de cada uma das treze rotinas, sobre os bytes de fabrica
    for entrada, antes, desvio, _, oque in TELA_DESVIOS:
        o = entrada - BASE_APP
        if bytes(app[o:o + len(antes)]) != antes:
            raise SystemExit(f'{entrada:#x} nao comeca com {antes.hex()}: {bytes(app[o:o + len(antes)]).hex()}')
        app[o:o + 4] = monta_goto(entrada, S[desvio])
        print(f'  tela B {entrada:#x} -> {desvio} ({S[desvio]:#x}): {oque}')

    # 4b3. o tela.o no lugar do G.729, e o G.729 fora do registro dos decodificadores. Antes, confere
    # que a faixa e o registro sao os de fabrica: o registro comeca com o tipo 0x40 e as quatro
    # primeiras rotinas dele, e a primeira e a entrada da faixa.
    o = G729_REGISTRO - BASE_APP
    registro = struct.unpack_from('<5I', app, o)
    if bytes(app[o:o + 4]) != G729_TIPO_ANTES or registro[1:] != G729_ENTRADAS or G729_ENTRADAS[0] != G729:
        raise SystemExit(f'{G729_REGISTRO:#x} nao e o registro do G.729 de fabrica: '
                         + ' '.join(f'{x:#x}' for x in registro))
    app[o:o + 4] = G729_TIPO_DEPOIS
    o = G729 - BASE_APP
    app[o:o + len(tela)] = tela
    print(f'  tela B: {len(tela)} B em {G729:#x} (cabem {G729_FIM - G729} no lugar do G.729); tipo do '
          f'G.729 no registro {G729_REGISTRO:#x}: {G729_TIPO_ANTES.hex()} -> {G729_TIPO_DEPOIS.hex()}')

    # 4c. diagnostico das faixas: so a cor do preenchimento, 2 bytes por sitio
    if REALCE_DIAG:
        for endereco, reg in REALCE_SITIOS:
            o = endereco - BASE_APP
            if bytes(app[o:o + 2]) != monta_imm8(reg, 0):
                raise SystemExit(f'{endereco:#x} nao e "r{reg} = 0": {bytes(app[o:o + 2]).hex()}')
            app[o:o + 2] = monta_imm8(reg, COR_REALCE)
        print(f'  DIAGNOSTICO: {len(REALCE_SITIOS)} faixas de largura total pintadas de '
              f'{COR_REALCE:#06x} (nao e a versao final)')

    # 5. revisao dos textos em ingles
    if TEXTOS_REVISADOS:
        for endereco, velho, texto in TEXTOS:
            o = endereco - BASE_APP
            if bytes(app[o:o + len(velho) + 1]) != velho.encode() + b'\0':
                raise SystemExit(f'{endereco:#x} nao contem "{velho}"')
            if len(texto) > len(velho):
                raise SystemExit(f'"{texto}" nao cabe no lugar de "{velho}"')
            app[o:o + len(velho) + 1] = texto.encode() + bytes(len(velho) + 1 - len(texto))
        print(f'  textos revisados: {len(TEXTOS)} cadeias, nenhum byte a mais')

    # 6. codigo novo e CRCs (a tabela ja esta dentro do app.bin)
    print(f'codigo novo: {len(blob)} B em {ENDERECO_CODIGO:#x} (ultimo byte '
          f'{ENDERECO_CODIGO + len(blob) - 1:#x})')
    print(f'regiao das vozes: {len(vozes)} B em {VOZES:#x} (cabem {VOZES_ACEITAS_FIM - VOZES} B ate o '
          'fim do C1, o que o carregador aceita)')
    jlfw.repack(origem, bytes(app), destino, blob=[(ENDERECO_CODIGO, blob), (VOZES, vozes)],
                overwrite=[(VOZES, VOZES_FIM)])
    print(f'gerado: {destino}')


if __name__ == '__main__':
    main()
