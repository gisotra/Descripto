// Garante que o que já existia continua funcionando depois da descriptografia em camadas:
// XOR, combinações com XOR, os botões individuais (chave informada ou vazia) e a criptografia.
'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { executar, automatico } = require('./motor');
const { PORTUGUES, INGLES, limpar } = require('./corpus');
const { aplicarEtapas, cesar, vigenere, atbash, xor, gerador, chaveAleatoria } = require('./cifras');

const TEXTOS = [...PORTUGUES.slice(0, 5), ...INGLES.slice(0, 3)];
const bytes = (aleatorio, n) => Array.from({ length: n }, () => 128 + Math.floor(aleatorio() * 127));

test.describe('XOR (arquivo UTF-8 do regulamento) sozinho e combinado', () => {
  test('XOR de 1 a 5 bytes, chave desconhecida', () => {
    const aleatorio = gerador(51);
    for (let tamanho = 1; tamanho <= 5; tamanho++) {
      const claro = TEXTOS[tamanho];
      const chave = bytes(aleatorio, tamanho);
      const r = automatico(xor(claro, chave));
      assert.equal(r.status, 'conclusivo');
      assert.equal(r.saida, claro);
      assert.equal(r.verificado, true);
      assert.deepEqual(r.etapas, [{ cifra: 'XOR', chave: chave.join(', ') }]);
      assert.equal(aplicarEtapas(claro, r.etapas), xor(claro, chave));
      assert.equal(r.passos[r.passos.length - 1].natural, true, 'o último passo da análise tem de ser o texto natural');
    }
  });

  test('uma cifra de letras e depois XOR (XOR sempre por último)', () => {
    const aleatorio = gerador(52);
    const casos = [
      ['César → XOR', (t) => cesar(t, 9), ['César']],
      ['Atbash → XOR', (t) => atbash(t), ['Atbash']],
      ['Vigenère → XOR', (t) => vigenere(t, 'segredo'), ['Vigenère']],
    ];
    for (const [nome, preparar, esperadas] of casos) {
      const claro = TEXTOS[2];
      const chave = bytes(aleatorio, 3);
      const r = automatico(xor(preparar(claro), chave));
      assert.equal(r.saida, claro, nome);
      assert.equal(r.verificado, true, nome);
      assert.deepEqual(r.etapas.map((e) => e.cifra), [...esperadas, 'XOR'], nome);
      assert.equal(r.etapas[r.etapas.length - 1].chave, chave.join(', '));
      assert.equal(aplicarEtapas(claro, r.etapas), xor(preparar(claro), chave), `${nome}: etapas reportadas`);
      assert.equal(r.passos[r.passos.length - 1].natural, true, `${nome}: o último passo da análise tem de ser o texto natural`);
      assert.equal(r.passos[0].natural, false, `${nome}: o passo 0 (depois do XOR) ainda tem outra camada`);
    }
  });

  test('arquivo com quebra de linha no fim é aceito (a quebra final é ignorada)', () => {
    const claro = TEXTOS[0];
    const r = automatico(`${xor(claro, [200, 17, 51])}\n`);
    assert.equal(r.saida, claro);
    assert.equal(r.verificado, true);
  });

  test('quebra de linha final cifrada junto com o texto não faz a prova real falhar ("não confere")', () => {
    const aleatorio = gerador(53);
    for (const chave of [[221], bytes(aleatorio, 2), bytes(aleatorio, 3)]) {
      const claro = TEXTOS[1];
      const r = automatico(xor(`${claro}\n`, chave));
      assert.equal(r.status, 'conclusivo', chave.join(','));
      assert.equal(r.saida, claro, chave.join(','));
      assert.equal(r.verificado, true, `${chave.join(',')}: a prova real tem de conferir`);
      assert.deepEqual(r.etapas, [{ cifra: 'XOR', chave: chave.join(', ') }]);
    }
  });

  test('quebra de linha CRLF crua no fim do arquivo é aceita', () => {
    const claro = TEXTOS[0];
    const r = automatico(`${xor(claro, [200, 17, 51])}\r\n`);
    assert.equal(r.saida, claro);
    assert.equal(r.verificado, true);
  });

  test('texto que não é letra nem XOR de letras gera erro claro, não resultado', () => {
    const r = automatico('Olá, isto não é um texto cifrado!');
    assert.notEqual(r.status, 'conclusivo');
    assert.equal(r.saida, '');
  });
});

test.describe('botões individuais e criptografia (sem mudanças de comportamento)', () => {
  test('César e Vigenère com a chave informada', () => {
    const claro = TEXTOS[0];
    assert.equal(executar(['cesar', 'decifrar', '7'], cesar(claro, 7)).saida, claro);
    assert.equal(executar(['vigenere', 'decifrar', 'segredo'], vigenere(claro, 'segredo')).saida, claro);
    assert.equal(executar(['atbash', 'decifrar'], atbash(claro)).saida, claro);
  });

  test('César e Vigenère com a chave vazia: o botão descobre a chave', () => {
    const claro = TEXTOS[1];
    const c = executar(['cesar', 'decifrar', ''], cesar(claro, 12));
    assert.equal(c.saida, claro);
    assert.match(c.stderr, /@chave deslocamento 12/);
    const v = executar(['vigenere', 'decifrar', ''], vigenere(claro, 'chave'));
    assert.equal(v.saida, claro);
    assert.match(v.stderr, /@chave chave\n/);
  });

  test('criptografar dá o mesmo texto das implementações de referência', () => {
    const claro = TEXTOS[3];
    assert.equal(executar(['cesar', 'cifrar', '5'], claro).saida, cesar(claro, 5));
    assert.equal(executar(['vigenere', 'cifrar', 'abcdef'], claro).saida, vigenere(claro, 'abcdef'));
    assert.equal(executar(['atbash', 'cifrar'], claro).saida, atbash(claro));
    const chave = [200, 17, 51];
    assert.equal(Buffer.from(executar(['xor', 'cifrar', chave.join(',')], claro).saida, 'utf8').toString('utf8'), xor(claro, chave));
  });

  test('chaves inválidas são recusadas', () => {
    assert.notEqual(executar(['cesar', 'cifrar', '30'], 'abc').codigo, 0);
    assert.notEqual(executar(['vigenere', 'cifrar', 'ab1'], 'abc').codigo, 0);
    assert.notEqual(executar(['xor', 'cifrar', '300'], 'abc').codigo, 0);
    assert.notEqual(executar(['auto', 'cifrar'], 'abc').codigo, 0, 'o automático só descriptografa');
    assert.notEqual(executar(['auto', 'decifrar', '--max-camadas', '9'], 'abc').codigo, 0, 'opção fora do intervalo');
  });

  test('textos com palavras estrangeiras e letras raras (waffle, yakisoba, kiwi) continuam recuperados', () => {
    const claro = limpar(
      'Na caixa de xadrez o zelador chamou o coordenador do show de jazz que esperava no táxi e juntos fizeram um waffle com yakisoba e um festival para a zebra do zoológico, o público aplaudiu',
    );
    const aleatorio = gerador(53);
    for (let tamanho = 4; tamanho <= 8; tamanho++) {
      const chave = chaveAleatoria(tamanho, aleatorio);
      const r = automatico(vigenere(claro, chave));
      assert.equal(r.saida, claro, `chave ${chave}`);
    }
  });
});
