// Testes da descriptografia de uma ou mais cifras em sequência (César, Vigenère, Atbash).
// Os textos cifrados são gerados por tests/cifras.js, que não usa o motor: se o motor errar, o teste mostra.
// Rodar: npm test
'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { automatico, avaliar } = require('./motor');
const { PORTUGUES, INGLES, EXEMPLO_CLARO, EXEMPLO_CIFRADO, NOMES_PROPRIOS } = require('./corpus');
const { aplicarEtapas, cesar, vigenere, atbash, xor, substituicao, gerador, letrasAleatorias, chaveAleatoria, embaralhar, alfabetoEmbaralhado } = require('./cifras');

const TEXTOS = [...PORTUGUES.slice(0, 6), ...INGLES.slice(0, 4)];
const idiomaDe = (texto) => (INGLES.includes(texto) || texto === EXEMPLO_CLARO ? 'inglês' : 'português');

// Recupera o texto e confere tudo o que um resultado conclusivo precisa ter.
function recuperar(cifrado, esperado, opcoes = []) {
  const r = automatico(cifrado, opcoes);
  assert.equal(r.status, 'conclusivo', `esperava conclusivo, veio ${r.status} (${r.detalhe}); média ${r.media} limiar ${r.limiar}`);
  assert.equal(r.saida, esperado, 'o texto recuperado difere do original');
  assert.equal(r.verificado, true, 'a prova real (cifrar de volta e comparar) falhou');
  assert.ok(r.media >= r.limiar, 'conclusivo exige média acima do limiar');
  // verificação independente: as cifras e chaves REPORTADAS, aplicadas ao texto claro, têm de dar o texto cifrado
  assert.equal(aplicarEtapas(esperado, r.etapas), cifrado, `as etapas reportadas (${JSON.stringify(r.etapas)}) não reproduzem o texto cifrado`);
  if (r.equivalente) {   // a forma equivalente (Atbash do outro lado) também tem de reproduzir o texto cifrado
    assert.equal(r.verificadoEq, true, 'a forma equivalente falhou na prova real do motor');
    assert.equal(aplicarEtapas(esperado, r.equivalente), cifrado, `a forma equivalente (${JSON.stringify(r.equivalente)}) não reproduz o texto cifrado`);
  }
  return r;
}

// O resultado precisa ser claramente inconclusivo: sem texto, com explicação e com média abaixo do limiar.
function assertInconclusivo(cifrado, opcoes = [], descricao = '') {
  const r = automatico(cifrado, opcoes);
  assert.equal(r.status, 'inconclusivo', `${descricao}: não podia concluir, mas concluiu (média ${r.media}, limiar ${r.limiar}): ${r.saida.slice(0, 60)}`);
  assert.equal(r.saida, '', `${descricao}: texto inconclusivo não pode ser apresentado como resultado`);
  assert.match(r.stderr, /Nada foi apresentado como resultado/);
  assert.ok(r.detalhe.length > 10, 'precisa explicar o motivo');
  return r;
}

// ---------------------------------------------------------------------------------------------
test.describe('uma cifra só', () => {
  test('César, em português e em inglês, sem a chave', () => {
    const aleatorio = gerador(11);
    for (const claro of TEXTOS) {
      const deslocamento = 1 + Math.floor(aleatorio() * 25);
      const r = recuperar(cesar(claro, deslocamento), claro);
      assert.deepEqual(r.etapas, [{ cifra: 'César', chave: String(deslocamento) }]);
      assert.equal(r.idioma, idiomaDe(claro));
      assert.equal(r.decomposicao, '', 'uma cifra só não tem decomposição');
    }
  });

  test('Vigenère com chave desconhecida de 2 a 8 letras', () => {
    const aleatorio = gerador(12);
    for (let tamanho = 2; tamanho <= 8; tamanho++) {
      for (const claro of [PORTUGUES[tamanho % 6], INGLES[tamanho % 4]]) {
        const chave = chaveAleatoria(tamanho, aleatorio);
        const r = recuperar(vigenere(claro, chave), claro);
        assert.equal(r.etapas.length, 1);
        assert.equal(r.etapas[0].cifra, 'Vigenère');
        assert.equal(r.etapas[0].chave, chave, `chave de ${tamanho} letras`);
        assert.equal(r.verificado, true);
      }
    }
  });

  test('Atbash', () => {
    for (const claro of TEXTOS.slice(0, 4)) {
      const r = recuperar(atbash(claro), claro);
      assert.deepEqual(r.etapas, [{ cifra: 'Atbash', chave: 'sem chave' }]);
    }
  });

  test('texto já em claro: nenhuma cifra, sem inventar camadas', () => {
    for (const claro of [PORTUGUES[0], INGLES[0]]) {
      const r = recuperar(claro, claro);
      assert.deepEqual(r.etapas, []);
      assert.equal(r.passos.length, 1);
    }
  });
});

// ---------------------------------------------------------------------------------------------
test.describe('duas cifras em sequência', () => {
  test('exemplo do enunciado: César(3) e depois Vigenère("abc")', () => {
    const r = recuperar(EXEMPLO_CIFRADO, EXEMPLO_CLARO);

    assert.equal(r.idioma, 'inglês', 'o texto do exemplo está em inglês');
    // camada efetiva provada pelo texto cifrado: um Vigenère de chave composta "def"
    assert.deepEqual(r.etapas, [{ cifra: 'Vigenère', chave: 'def' }]);
    // decomposição compatível, na convenção do enunciado (Vigenère externa, César interna)
    assert.match(r.decomposicao, /César \(deslocamento 3, camada interna, aplicada primeiro\)/);
    assert.match(r.decomposicao, /Vigenère \(chave abc, camada externa, aplicada depois\)/);
    // a ordem inversa da cifragem: primeiro Vigenère (externa), depois César (interna)
    assert.equal(r.passos.length, 3);
    assert.match(r.passos[1].acao, /Vigenère \(chave abc\), camada externa/);
    assert.match(r.passos[2].acao, /César \(deslocamento 3\), camada interna/);
    // o resultado intermediário é avaliado: ainda parece cifrado; só o último é natural
    assert.equal(r.passos[0].natural, false);
    assert.equal(r.passos[1].natural, false, 'depois de desfazer só a Vigenère o texto ainda está cifrado');
    assert.ok(r.passos[1].media < r.limiar);
    assert.equal(r.passos[2].natural, true);
    assert.ok(r.passos[2].media > r.limiar);
    // honestidade sobre o que o texto cifrado prova
    assert.match(r.nota, /26 divisões equivalentes/);
    assert.match(r.nota, /idêntico/);
  });

  test('a ordem entre César e Vigenère não existe no texto cifrado (somam deslocamentos)', () => {
    const aleatorio = gerador(21);
    for (let i = 0; i < 60; i++) {
      const claro = TEXTOS[i % TEXTOS.length];
      const deslocamento = 1 + Math.floor(aleatorio() * 25);
      const chave = chaveAleatoria(2 + (i % 7), aleatorio);
      const composta = [...chave].map((c) => String.fromCharCode(97 + ((c.charCodeAt(0) - 97 + deslocamento) % 26))).join('');
      const a = vigenere(cesar(claro, deslocamento), chave);   // César e depois Vigenère
      const b = cesar(vigenere(claro, chave), deslocamento);   // Vigenère e depois César
      assert.equal(a, b, 'as duas ordens dão o mesmo texto cifrado');
      assert.equal(a, vigenere(claro, composta), 'e é igual a um único Vigenère de chave composta');
    }
  });

  test('César depois Vigenère e Vigenère depois César dão o mesmo resultado no motor', () => {
    const aleatorio = gerador(22);
    for (let i = 0; i < 8; i++) {
      const claro = TEXTOS[i];
      const deslocamento = 1 + Math.floor(aleatorio() * 25);
      const chave = chaveAleatoria(3 + (i % 5), aleatorio);
      const r1 = recuperar(vigenere(cesar(claro, deslocamento), chave), claro);
      const r2 = recuperar(cesar(vigenere(claro, chave), deslocamento), claro);
      assert.deepEqual(r1.etapas, r2.etapas);
      assert.equal(r1.etapas[0].cifra, 'Vigenère');
      assert.equal(r1.etapas[0].chave.length, chave.length);
      // a chave efetiva é a soma dos deslocamentos
      const composta = [...chave].map((c) => String.fromCharCode(97 + ((c.charCodeAt(0) - 97 + deslocamento) % 26))).join('');
      assert.equal(r1.etapas[0].chave, composta);
    }
  });

  test('Atbash combinado com César ou Vigenère, nas duas ordens', () => {
    const aleatorio = gerador(23);
    for (let i = 0; i < 6; i++) {
      const claro = TEXTOS[i];
      const deslocamento = 1 + Math.floor(aleatorio() * 25);
      const chave = chaveAleatoria(3 + i, aleatorio);
      recuperar(cesar(atbash(claro), deslocamento), claro);
      recuperar(atbash(cesar(claro, deslocamento)), claro);
      const r = recuperar(vigenere(atbash(claro), chave), claro);
      recuperar(atbash(vigenere(claro, chave)), claro);
      // com Atbash e chave há duas formas equivalentes, as duas conferidas
      assert.ok(r.equivalente, 'deve mostrar a forma equivalente');
      assert.equal(r.verificadoEq, true);
      assert.notDeepEqual(r.equivalente, r.etapas);
      assert.match(r.nota, /não diz de que lado o Atbash foi aplicado/);
    }
  });

  test('Atbash e Vigenère("abc"): a chave "azy" é a mesma cifra com o Atbash do outro lado (caso relatado)', () => {
    const negada = (chave) => [...chave].map((c) => String.fromCharCode(97 + ((26 - (c.charCodeAt(0) - 97)) % 26))).join('');
    assert.equal(negada('abc'), 'azy');
    for (const claro of [PORTUGUES[0], PORTUGUES[4], INGLES[1]]) {
      const atbashDepoisVigenere = vigenere(atbash(claro), 'abc');
      const vigenereDepoisAtbash = atbash(vigenere(claro, 'azy'));
      assert.equal(atbashDepoisVigenere, vigenereDepoisAtbash, 'as duas formas dão o mesmo texto cifrado');

      const r = recuperar(atbashDepoisVigenere, claro);
      // forma principal: a que o autor usou (chave "abc", letras mais perto de "a")
      assert.deepEqual(r.etapas, [{ cifra: 'Atbash', chave: 'sem chave' }, { cifra: 'Vigenère', chave: 'abc' }]);
      // forma equivalente: Vigenère com a chave negada e depois Atbash
      assert.deepEqual(r.equivalente, [{ cifra: 'Vigenère', chave: 'azy' }, { cifra: 'Atbash', chave: 'sem chave' }]);
      assert.match(r.nota, /"Vigenère\(azy\) e depois Atbash" e "Atbash e depois Vigenère\(abc\)"/);
      // os passos seguem a forma principal: desfaz o Vigenère (camada externa) e depois o Atbash
      assert.match(r.passos[1].acao, /desfazer Vigenère \(chave abc\)/);
      assert.match(r.passos[2].acao, /desfazer Atbash/);
      assert.equal(r.passos[1].natural, false);
      assert.equal(r.passos[2].natural, true);
    }
  });

  test('com chaves que são palavras, a forma principal é a que usa a palavra, seja qual for a ordem usada', () => {
    const negada = (chave) => [...chave].map((c) => String.fromCharCode(97 + ((26 - (c.charCodeAt(0) - 97)) % 26))).join('');
    for (const palavra of ['segredo', 'chave', 'torneio', 'senha', 'cifra']) {
      const claro = PORTUGUES[2];
      const a = recuperar(vigenere(atbash(claro), palavra), claro);          // Atbash e depois a palavra
      const b = recuperar(atbash(vigenere(claro, palavra)), claro);          // a palavra e depois Atbash
      assert.deepEqual(a.etapas, [{ cifra: 'Atbash', chave: 'sem chave' }, { cifra: 'Vigenère', chave: palavra }], palavra);
      assert.deepEqual(b.etapas, [{ cifra: 'Vigenère', chave: palavra }, { cifra: 'Atbash', chave: 'sem chave' }], palavra);
      assert.equal(a.equivalente[0].chave, negada(palavra));
      assert.equal(b.equivalente[1].cifra, 'Vigenère');
    }
  });

  test('três camadas (César, Atbash e Vigenère) com o limite padrão de 3', () => {
    const claro = PORTUGUES[1];
    recuperar(vigenere(atbash(cesar(claro, 7)), 'segredo'), claro);
  });

  test('a identificação não depende de uma ordem fixa: todas as combinações de 2 cifras', () => {
    const aleatorio = gerador(24);
    const claro = INGLES[2];
    const k = chaveAleatoria(5, aleatorio);
    const combinacoes = {
      'César → Vigenère': (t) => vigenere(cesar(t, 11), k),
      'Vigenère → César': (t) => cesar(vigenere(t, k), 11),
      'Atbash → César': (t) => cesar(atbash(t), 11),
      'César → Atbash': (t) => atbash(cesar(t, 11)),
      'Atbash → Vigenère': (t) => vigenere(atbash(t), k),
      'Vigenère → Atbash': (t) => atbash(vigenere(t, k)),
    };
    for (const [nome, cifrar] of Object.entries(combinacoes)) {
      const r = automatico(cifrar(claro));
      assert.equal(r.saida, claro, nome);
      assert.equal(r.verificado, true, nome);
      assert.equal(aplicarEtapas(claro, r.etapas), cifrar(claro), `${nome}: etapas reportadas`);
    }
  });
});

// ---------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------
// Aplica uma cadeia descrita por letras: A = Atbash, C = César, V = Vigenère, X = XOR (sempre por último).
function cifrarCadeia(claro, cadeia, aleatorio, letrasVigenere = () => 2 + Math.floor(aleatorio() * 5)) {
  return cadeia.reduce((texto, c) => {
    if (c === 'A') return atbash(texto);
    if (c === 'C') return cesar(texto, 1 + Math.floor(aleatorio() * 25));
    if (c === 'V') return vigenere(texto, chaveAleatoria(letrasVigenere(), aleatorio));
    return xor(texto, Array.from({ length: 1 + Math.floor(aleatorio() * 2) }, () => 1 + Math.floor(aleatorio() * 255)));
  }, claro);
}

// Resultado aceito pela estrutura da cifra: conclusivo, texto certo, prova real, aviso, mas média abaixo do limiar.
function recuperarPelaEstrutura(cifrado, esperado) {
  const r = automatico(cifrado);
  assert.equal(r.status, 'conclusivo', `esperava conclusivo, veio ${r.status} (${r.detalhe}); média ${r.media} limiar ${r.limiar}`);
  assert.equal(r.saida.replace(/\n$/, ''), esperado, `texto errado (${JSON.stringify(r.etapas)})`);
  assert.equal(r.verificado, true, 'a prova real falhou');
  assert.equal(aplicarEtapas(esperado, r.etapas), cifrado, `as etapas reportadas (${JSON.stringify(r.etapas)}) não reproduzem o texto cifrado`);
  if (r.media < r.limiar) {
    assert.match(r.detalhe, /estrutura da cifra/, 'abaixo do limiar, só pode ser aceito pela estrutura da cifra');
    assert.match(r.stderr, /@estrutura .*Leia o texto para confirmar/, 'tem de avisar que boa parte do texto não parece português');
  }
  return r;
}

test.describe('três e quatro cifras em sequência', () => {
  test('cadeias de 3 e 4 cifras (Atbash, César, Vigenère e XOR por último), chave de Vigenère de 2 a 6 letras', () => {
    const aleatorio = gerador(61);
    const cadeias = ['ACV', 'CVA', 'VAC', 'AVC', 'CAV', 'ACVX', 'CVAX', 'VCAX', 'AVCX', 'CAVX', 'ACVA', 'VACA'];
    for (let i = 0; i < cadeias.length * 2; i++) {
      const claro = TEXTOS[i % TEXTOS.length];
      const cadeia = [...cadeias[i % cadeias.length]];
      let cifrado = cifrarCadeia(claro, cadeia, aleatorio);
      while (cifrado.endsWith('\n')) {    // último byte cifrado = quebra de linha: o regulamento manda ignorá-lo
        cifrado = cifrarCadeia(claro, cadeia, aleatorio);
      }
      recuperar(cifrado, claro);
    }
  });

  test('duas Vigenère em sequência: a chave efetiva tem o mmc das duas e é decomposta nas duas chaves', () => {
    const casos = [['abc', 'fghij', 4], ['segredo', 'lyr', 0], ['abcd', 'xyz', 11], ['chave', 'abcd', 0]];
    for (const [k1, k2, deslocamento] of casos) {
      const claro = PORTUGUES[(k1.length + k2.length) % PORTUGUES.length];
      const cifrado = cesar(vigenere(vigenere(claro, k1), k2), deslocamento || 26);
      const r = recuperar(cifrado, claro);
      const [p1, p2] = [k1.length, k2.length].sort((a, b) => a - b);
      assert.match(r.decomposicao, new RegExp(`Vigenère \\(chave [a-z]{${p1}}, ${p1} letras.*Vigenère \\(chave [a-z]{${p2}}, ${p2} letras`),
        `${k1}+${k2}: decomposição em duas chaves`);
    }
  });

  test('texto com muitos nomes próprios (seleção georgiana): decifrado e aceito pela estrutura da cifra, com aviso', () => {
    const claro = NOMES_PROPRIOS[0];
    assert.equal(avaliar(claro).natural, false, 'o próprio texto claro fica abaixo do limiar de texto natural');
    const aleatorio = gerador(62);
    for (const cadeia of ['ACV', 'CVA', 'VAC', 'ACVX', 'CVAX', 'CAVX']) {
      const r = recuperarPelaEstrutura(cifrarCadeia(claro, [...cadeia], aleatorio, () => 5), claro);
      assert.match(r.detalhe, /estrutura da cifra/, cadeia);
    }
  });

  test('textos com nomes próprios e duas Vigenère: a chave longa "de ajuste" nunca vence a chave certa', () => {
    const aleatorio = gerador(63);
    for (const claro of NOMES_PROPRIOS) {
      for (const [k1, k2] of [['abc', 'fghij'], ['xk', 'pqr']]) {
        recuperarPelaEstrutura(cesar(vigenere(vigenere(claro, k1), k2), 1 + Math.floor(aleatorio() * 25)), claro);
      }
    }
  });

  test('a simplificação da chave não troca a chave certa por uma menor com colunas erradas', () => {
    // "uxut" lembra "ut" repetida; a chave de 14 letras de A → V(7) → V(2) → A tem vizinhas que enganam a busca letra a letra
    for (const claro of TEXTOS) {
      for (const chave of ['uxut', 'abab', 'mamae', 'rarara']) {
        recuperar(vigenere(claro, chave), claro);
      }
    }
    const claro = [...PORTUGUES, ...INGLES].join('').slice(2297, 2297 + 240);
    recuperar(atbash(vigenere(vigenere(atbash(claro), 'oojccrj'), 'no')), claro);
  });

  test('cifras sem solução continuam inconclusivas com a busca de até 4 camadas e chave efetiva de até 24 letras', () => {
    const aleatorio = gerador(64);
    for (let i = 0; i < 8; i++) {
      const claro = TEXTOS[i % TEXTOS.length];
      assertInconclusivo(vigenere(embaralhar(claro, aleatorio), chaveAleatoria(31, aleatorio)), [], 'Vigenère de 31 letras sobre texto embaralhado');
      assertInconclusivo(vigenere(substituicao(claro, alfabetoEmbaralhado(aleatorio)), chaveAleatoria(5, aleatorio)), [], 'substituição e depois Vigenère');
      assertInconclusivo(xor(substituicao(claro, alfabetoEmbaralhado(aleatorio)), [17, 200]), [], 'substituição e depois XOR');
    }
  });
});

test.describe('resultados inconclusivos (nada de texto aleatório passando por sucesso)', () => {
  test('texto curto demais para uma conclusão confiável', () => {
    const aleatorio = gerador(31);
    for (const tamanho of [20, 30, 54]) {
      const claro = PORTUGUES[tamanho % 6].slice(0, tamanho);
      const r = assertInconclusivo(vigenere(claro, chaveAleatoria(5, aleatorio)), [], `${tamanho} letras`);
      assert.match(r.detalhe, /só \d+ letras/);
    }
  });

  test('letras aleatórias nunca viram "descriptografia bem-sucedida"', () => {
    const aleatorio = gerador(32);
    for (let i = 0; i < 40; i++) {
      const r = assertInconclusivo(letrasAleatorias(60 + Math.floor(aleatorio() * 200), aleatorio), [], 'aleatório');
      assert.ok(r.media < r.limiar);
    }
  });

  test('cifras que o motor não resolve: Vigenère com chave de 31 e 37 letras, substituição, texto embaralhado', () => {
    const aleatorio = gerador(33);
    for (let i = 0; i < 12; i++) {
      const claro = TEXTOS[i % TEXTOS.length];
      assertInconclusivo(vigenere(claro, chaveAleatoria(31, aleatorio)), [], 'Vigenère de 31 letras');
      assertInconclusivo(vigenere(claro, chaveAleatoria(37, aleatorio)), [], 'Vigenère de 37 letras');
      assertInconclusivo(substituicao(claro, alfabetoEmbaralhado(aleatorio)), [], 'substituição monoalfabética');
      assertInconclusivo(embaralhar(claro, aleatorio), [], 'texto embaralhado');
    }
  });

  test('o limite de camadas é respeitado e configurável', () => {
    const claro = PORTUGUES[2];
    const cifrado = cesar(atbash(claro), 9);
    assertInconclusivo(cifrado, ['--max-camadas', '1'], 'limite de 1 camada');
    recuperar(cifrado, claro, ['--max-camadas', '2']);
    recuperar(cifrado, claro);
  });

  test('o limite do período da chave é respeitado e configurável', () => {
    const claro = (PORTUGUES[0] + PORTUGUES[3]).slice(0, 250);
    const cifrado = vigenere(claro, 'qwhtpzmkdbfs');   // 12 letras
    assertInconclusivo(cifrado, ['--max-periodo', '8'], 'chave de 12 letras com o limite de 8');
    recuperar(cifrado, claro, ['--max-periodo', '16']);
    recuperar(cifrado, claro);                          // o padrão é 24 (duas Vigenère em sequência somam chaves)
  });

  test('o limite de tempo é respeitado e informado', () => {
    const aleatorio = gerador(34);
    const r = assertInconclusivo(letrasAleatorias(30000, aleatorio), ['--tempo-ms', '100'], 'tempo');
    assert.match(r.detalhe, /tempo máximo/);
  });

  test('a mesma busca recupera o texto quando há evidência suficiente', () => {
    // 20 textos reais cifrados com chaves de 2 a 8 letras: o motor não pode ficar inconclusivo demais
    const aleatorio = gerador(35);
    let conclusivos = 0;
    for (let i = 0; i < 20; i++) {
      const claro = TEXTOS[i % TEXTOS.length];
      const r = automatico(vigenere(claro, chaveAleatoria(2 + (i % 7), aleatorio)));
      if (r.status === 'conclusivo' && r.saida === claro) {
        conclusivos++;
      }
    }
    assert.ok(conclusivos >= 19, `só ${conclusivos} de 20 recuperados`);
  });
});

// ---------------------------------------------------------------------------------------------
test.describe('avaliação linguística e evidências', () => {
  test('o avaliador separa português, inglês e letras aleatórias', () => {
    const pt = avaliar(PORTUGUES[0]);
    const en = avaliar(INGLES[0]);
    const aleatorio = avaliar(letrasAleatorias(200, gerador(41)));
    assert.equal(pt.idioma, 'português');
    assert.equal(en.idioma, 'inglês');
    assert.ok(pt.natural && en.natural);
    assert.equal(aleatorio.natural, false);
    assert.ok(pt.media > aleatorio.media + 3, 'português deve ficar bem acima do aleatório');
  });

  test('índice de coincidência: ~0,038 em letras aleatórias, bem mais alto em texto real', () => {
    const aleatorio = avaliar(letrasAleatorias(3000, gerador(42)));
    assert.ok(aleatorio.ic > 0.034 && aleatorio.ic < 0.043, `IC aleatório ${aleatorio.ic}`);
    assert.ok(avaliar(PORTUGUES[0]).ic > 0.06);
    assert.ok(avaliar(INGLES[0]).ic > 0.058);
  });

  test('a evidência de IC e do período aparece no resultado', () => {
    const r = automatico(vigenere(PORTUGUES[1], 'abcde'));
    assert.match(r.stderr, /@evidencia IC do texto cifrado = 0\.0[3-5]\d+ .* polialfabética/);
    assert.match(r.stderr, /@evidencia período estimado pelo IC das colunas = 5 /);
  });

  test('o limiar de texto natural cresce em rigor com textos mais curtos', () => {
    const limiares = [60, 80, 100, 166, 250].map((n) => avaliar(PORTUGUES[0].slice(0, n)).limiar);
    for (let i = 1; i < limiares.length; i++) {
      assert.ok(limiares[i] < limiares[i - 1], 'textos mais curtos exigem média mais alta');
    }
    assert.equal(avaliar(PORTUGUES[0].slice(0, 30)).natural, false, 'abaixo de 55 letras nunca é natural');
  });
});
