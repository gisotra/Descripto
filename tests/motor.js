// Executa o motor em C (bin/motor) e interpreta as linhas "@..." que ele escreve no stderr.
'use strict';

const { spawnSync } = require('child_process');
const path = require('path');

const MOTOR = path.join(__dirname, '..', 'bin', process.platform === 'win32' ? 'motor.exe' : 'motor');

// entrada: string (cada caractere vira UTF-8) ou Buffer. Retorna a saída, o código de saída e os campos lidos.
function executar(args, entrada = '') {
  const r = spawnSync(MOTOR, args, { input: Buffer.isBuffer(entrada) ? entrada : Buffer.from(entrada, 'utf8'), maxBuffer: 64 * 1024 * 1024 });
  const stderr = r.stderr.toString('utf8').replace(/\r/g, '');
  const info = {
    codigo: r.status,
    saida: r.stdout.toString('utf8'),
    stderr,
    status: null,        // 'conclusivo' | 'inconclusivo'
    detalhe: '',         // confiança ou motivo
    cifras: '',
    chave: '',
    etapas: [],          // [{ cifra, chave }] na ordem de cifragem
    passos: [],          // evolução da descriptografia, com a avaliação de cada texto intermediário
    verificado: null,
    equivalente: null,   // outra forma, com o Atbash do outro lado, que reproduz o mesmo texto cifrado
    verificadoEq: null,
    idioma: '',
    decomposicao: '',
    nota: '',
    media: NaN,
    limiar: NaN,
    avisos: [],
  };
  for (const linha of stderr.split('\n')) {
    if (linha.startsWith('@status ')) {
      const [estado, detalhe = ''] = linha.slice(8).split(' | ');
      info.status = estado.trim();
      info.detalhe = detalhe.trim();
    } else if (linha.startsWith('@cifras ')) {
      info.cifras = linha.slice(8);
    } else if (linha.startsWith('@chave ')) {
      info.chave = linha.slice(7);
    } else if (linha.startsWith('@etapa ')) {
      const [cifra, chave] = linha.slice(7).split(' | ');
      info.etapas.push({ cifra, chave });
    } else if (linha.startsWith('@etapaeq ')) {
      const [cifra, chave] = linha.slice(9).split(' | ');
      (info.equivalente = info.equivalente || []).push({ cifra, chave });
    } else if (linha.startsWith('@verificadoeq ')) {
      info.verificadoEq = linha.slice(14).trim() === 'sim';
    } else if (linha.startsWith('@passo ')) {
      const [numero, acao, ic, media, idioma, veredito] = linha.slice(7).split(' | ');
      info.passos.push({ numero: Number(numero), acao, ic: Number(ic), media: Number(media), idioma, natural: veredito.startsWith('texto natural') });
    } else if (linha.startsWith('@verificado ')) {
      info.verificado = linha.slice(12).trim() === 'sim';
    } else if (linha.startsWith('@idioma ')) {
      info.idioma = linha.slice(8);
    } else if (linha.startsWith('@decomposicao ')) {
      info.decomposicao = linha.slice(14);
    } else if (linha.startsWith('@nota ')) {
      info.nota = linha.slice(6);
    } else if (linha.startsWith('@pontuacao ')) {
      const [media, limiar] = linha.slice(11).split(' | ');
      info.media = Number(media);
      info.limiar = Number(limiar);
    } else if (linha.trim() !== '' && !linha.startsWith('@')) {
      info.avisos.push(linha);
    }
  }
  return info;
}

// Descriptografia automática de um texto cifrado (só letras ou arquivo de XOR).
function automatico(cifrado, opcoes = []) {
  return executar(['auto', 'decifrar', ...opcoes], cifrado);
}

// Mede um texto sem procurar cifras: { media, ic, idioma, limiar, natural, letras }.
function avaliar(texto) {
  const r = executar(['avaliar', 'decifrar'], texto);
  const m = /media=(\S+) ic=(\S+) idioma=(\S+) limiar=(\S+) natural=(\S+) letras=(\d+)/.exec(r.saida);
  return { media: Number(m[1]), ic: Number(m[2]), idioma: m[3], limiar: Number(m[4]), natural: m[5] === 'sim', letras: Number(m[6]) };
}

module.exports = { executar, automatico, avaliar, MOTOR };
