// Implementações de referência das cifras, INDEPENDENTES do motor em C. Servem para gerar os textos
// cifrados dos testes: se o motor e estas funções discordarem, o teste revela.
'use strict';

const A = 'a'.charCodeAt(0);
const letra = (codigo) => String.fromCharCode(A + (((codigo % 26) + 26) % 26));

// César: desloca cada letra. senha de 1 a 25.
function cesar(texto, senha) {
  return [...texto].map((c) => letra(c.charCodeAt(0) - A + senha)).join('');
}

// Vigenère: a letra i recebe o deslocamento da letra i % tamanho da chave (a = 0, b = 1, ...).
function vigenere(texto, chave) {
  return [...texto].map((c, i) => letra(c.charCodeAt(0) - A + chave.charCodeAt(i % chave.length) - A)).join('');
}

// Atbash: a <-> z, b <-> y, ...
function atbash(texto) {
  return [...texto].map((c) => letra(25 - (c.charCodeAt(0) - A))).join('');
}

// XOR com uma chave de bytes repetida. O resultado é uma string cujos "caracteres" têm código de 0 a 255
// (no arquivo do torneio, cada um vira UTF-8: acima de 127 ocupam 2 bytes).
function xor(texto, bytes) {
  return [...texto].map((c, i) => String.fromCharCode(c.charCodeAt(0) ^ bytes[i % bytes.length])).join('');
}

// Substituição monoalfabética com um alfabeto embaralhado (cifra clássica que o motor NÃO implementa).
function substituicao(texto, alfabeto) {
  return [...texto].map((c) => alfabeto[c.charCodeAt(0) - A]).join('');
}

// Gerador pseudoaleatório determinístico (mulberry32): os testes dão sempre o mesmo resultado.
function gerador(semente) {
  let s = semente >>> 0;
  return () => {
    s = (s + 0x6d2b79f5) >>> 0;
    let t = s;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function letrasAleatorias(n, aleatorio) {
  return Array.from({ length: n }, () => letra(Math.floor(aleatorio() * 26))).join('');
}

function chaveAleatoria(tamanho, aleatorio) {
  return letrasAleatorias(tamanho, aleatorio);
}

function embaralhar(texto, aleatorio) {
  const v = [...texto];
  for (let i = v.length - 1; i > 0; i--) {
    const j = Math.floor(aleatorio() * (i + 1));
    [v[i], v[j]] = [v[j], v[i]];
  }
  return v.join('');
}

function alfabetoEmbaralhado(aleatorio) {
  return embaralhar('abcdefghijklmnopqrstuvwxyz', aleatorio);
}

// Aplica as etapas reportadas pelo motor ({ cifra, chave }, na ordem de cifragem) ao texto claro, com as
// implementações de referência acima. Serve de verificação INDEPENDENTE do que o motor afirma.
function aplicarEtapas(claro, etapas) {
  return etapas.reduce((texto, { cifra, chave }) => {
    if (cifra === 'César') {
      return cesar(texto, Number(chave));
    }
    if (cifra === 'Vigenère') {
      return vigenere(texto, chave);
    }
    if (cifra === 'Atbash') {
      return atbash(texto);
    }
    if (cifra === 'XOR') {
      return xor(texto, chave.split(/[,\s]+/).filter(Boolean).map(Number));
    }
    throw new Error(`cifra desconhecida: ${cifra}`);
  }, claro);
}

module.exports = { aplicarEtapas, cesar, vigenere, atbash, xor, substituicao, gerador, letrasAleatorias, chaveAleatoria, embaralhar, alfabetoEmbaralhado };
