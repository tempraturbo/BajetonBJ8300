unit ModeloRadio;

{ O que é do BJ-8300 no Tempra Link do celular: o nome, a frente do rádio (as teclas e os códigos delas, os mesmos
  do controle remoto do Windows) e o tamanho da tela. As telas do app só conhecem isto e as units do rádio
  (Remoto.pas, BJ8300.pas, CaboBJ.pas); para o UV-17L, troca-se este arquivo e elas, e o transporte (o cabo pelo USB
  no lugar do Bluetooth). }

interface

uses
  Remoto;

type
  TTipoTecla = (ttAlgarismo, ttFuncao);

  TTeclaModelo = record
    Texto, Letras: string;
    Codigo: Byte;
    Tipo: TTipoTecla;
  end;

const
  RADIO_MARCA = 'Baojie';
  RADIO_NOME = 'BJ-8300';
  RADIO_TELA_LARGURA = TELA_LARGURA;   // 160 x 128, a tela do rádio
  RADIO_TELA_ALTURA = TELA_ALTURA;

  // a frente do rádio: a linha de cima, os algarismos em 4 linhas de 3 e as teclas do lado
  TECLAS_CIMA: array[0..3] of TTeclaModelo = (
    (Texto: 'MENU'; Letras: 'OK'; Codigo: TECLA_OK; Tipo: ttFuncao),
    (Texto: '▲'; Codigo: TECLA_CIMA; Tipo: ttFuncao),
    (Texto: '▼'; Codigo: TECLA_BAIXO; Tipo: ttFuncao),
    (Texto: 'EXIT'; Codigo: TECLA_EXIT; Tipo: ttFuncao));
  TECLAS_NUMEROS: array[0..11] of TTeclaModelo = (
    (Texto: '1'; Codigo: 6), (Texto: '2'; Letras: 'ABC'; Codigo: 7), (Texto: '3'; Letras: 'DEF'; Codigo: 8),
    (Texto: '4'; Letras: 'GHI'; Codigo: 10), (Texto: '5'; Letras: 'JKL'; Codigo: 11),
    (Texto: '6'; Letras: 'MNO'; Codigo: 12), (Texto: '7'; Letras: 'PQRS'; Codigo: 14),
    (Texto: '8'; Letras: 'TUV'; Codigo: 15), (Texto: '9'; Letras: 'WXYZ'; Codigo: 16),
    (Texto: '*'; Codigo: TECLA_ESTRELA; Tipo: ttFuncao), (Texto: '0'; Codigo: TECLA_ZERO),
    (Texto: '#'; Codigo: TECLA_CERQUILHA; Tipo: ttFuncao));
  TECLAS_LADO: array[0..1] of TTeclaModelo = (
    (Texto: 'PF2'; Codigo: TECLA_PF2; Tipo: ttFuncao),
    (Texto: 'PF3'; Codigo: TECLA_PF3; Tipo: ttFuncao));

implementation

end.
