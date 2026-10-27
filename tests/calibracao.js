// Calibração do limiar de "texto natural" do motor (limiar_natural em c/motor.c).
//
// Para cada tamanho de texto mede:
//   - NATURAL: a média de trigramas de textos reais (português e inglês) em claro;
//   - NULOS: o melhor valor que a MESMA busca do motor consegue arrancar de textos que NÃO devem ser aceitos
//     (letras aleatórias, texto cifrado por Vigenère com chave maior que a analisada, substituição
//     monoalfabética e texto embaralhado). O motor roda com --limiar 99, para nunca parar cedo.
// O limiar tem de ficar acima do máximo dos nulos e abaixo do mínimo dos naturais.
//
// Uso: node tests/calibracao.js [quantidade-por-categoria]
'use strict';

const { execFile } = require('child_process');
const { MOTOR, avaliar } = require('./motor');
const { PORTUGUES, INGLES } = require('./corpus');
const { vigenere, substituicao, gerador, letrasAleatorias, chaveAleatoria, embaralhar, alfabetoEmbaralhado } = require('./cifras');

const TAMANHOS = [20, 30, 40, 60, 80, 100, 130, 166, 200, 250];
const POR_CATEGORIA = Number(process.argv[2]) || 40;
const LONGO = [...PORTUGUES, ...INGLES].join('');

function melhorMedia(texto) {
  return new Promise((resolve) => {
    const processo = execFile(MOTOR, ['auto', 'decifrar', '--limiar', '99', '--tempo-ms', '20000'], { maxBuffer: 1 << 26 }, (erro, saida, stderr) => {
      const m = /@pontuacao (\S+) \|/.exec(stderr);
      resolve(m ? Number(m[1]) : NaN);
    });
    processo.stdin.end(texto);
  });
}

// Executa as tarefas com no máximo "limite" processos ao mesmo tempo.
async function emParalelo(tarefas, limite = 6) {
  const resultados = new Array(tarefas.length);
  let proxima = 0;
  await Promise.all(
    Array.from({ length: limite }, async () => {
      while (proxima < tarefas.length) {
        const i = proxima++;
        resultados[i] = await tarefas[i]();
      }
    }),
  );
  return resultados;
}

const resumo = (v) => {
  const o = v.filter(Number.isFinite).sort((a, b) => a - b);
  const media = o.reduce((soma, x) => soma + x, 0) / o.length;
  const dp = Math.sqrt(o.reduce((soma, x) => soma + (x - media) ** 2, 0) / o.length);
  return { min: o[0], p1: o[Math.floor(o.length * 0.01)], media, dp, max: o[o.length - 1] };
};

async function principal() {
  const aleatorio = gerador(20260101);
  const recorte = (n) => {
    const inicio = Math.floor(aleatorio() * (LONGO.length - n));
    return LONGO.slice(inicio, inicio + n);
  };

  console.log(`calibração: ${POR_CATEGORIA} textos por categoria e tamanho\n`);
  console.log('   n | naturais: mín   média  dp | nulos: máx   média  dp | folga (mín natural - máx nulo)');
  const sugestao = [];
  for (const n of TAMANHOS) {
    const naturais = Array.from({ length: POR_CATEGORIA * 3 }, () => avaliar(recorte(n)).media);
    const categorias = {
      aleatorio: Array.from({ length: POR_CATEGORIA }, () => letrasAleatorias(n, aleatorio)),
      vigenereLongo: Array.from({ length: POR_CATEGORIA }, (_, i) => vigenere(recorte(n), chaveAleatoria(i % 2 ? 20 : 12, aleatorio))),
      substituicao: Array.from({ length: POR_CATEGORIA }, () => substituicao(recorte(n), alfabetoEmbaralhado(aleatorio))),
      embaralhado: Array.from({ length: POR_CATEGORIA * 2 }, () => embaralhar(recorte(n), aleatorio)),
    };
    const todos = [];
    for (const textos of Object.values(categorias)) {
      todos.push(...(await emParalelo(textos.map((t) => () => melhorMedia(t)))));
    }
    const nulos = resumo(todos);
    const nat = resumo(naturais);
    sugestao.push({ n, maxNulos: nulos.max, minNatural: nat.min });
    const f = (x) => x.toFixed(2).padStart(6);
    console.log(`${String(n).padStart(4)} |           ${f(nat.min)} ${f(nat.media)} ${f(nat.dp)} |        ${f(nulos.max)} ${f(nulos.media)} ${f(nulos.dp)} | ${f(nat.min - nulos.max)}`);
  }
  console.log('\nmáximo dos nulos por tamanho (o limiar deve ficar acima disso, com folga):');
  console.log(sugestao.map((s) => `{ ${s.n}, ${s.maxNulos.toFixed(2)} }`).join(', '));
}

principal();
