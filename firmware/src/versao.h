/* A versao do firmware, num lugar so: o item VERSION a mostra (o montar.py a grava no app.bin, no lugar do V0.12, com
 * no maximo 5 letras) e, desde a v170, o cabo e o Bluetooth a respondem (gancho_cabo, no bancos.c), para o Tempra Link
 * e o CHIRP saberem com qual firmware falam. Cada um deles so trabalha com as versoes que conhece: mudou o que o
 * firmware guarda ou responde, muda a versao, e eles tambem (docs/engenharia-reversa.md, secao 43). */
#ifndef VERSAO_H
#define VERSAO_H

#define VERSAO "V4.2T"

#endif
