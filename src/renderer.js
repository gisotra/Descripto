// Lógica da tela. Toda a criptografia é feita pelo motor em C (bin/motor).
const $ = (id) => document.getElementById(id);

const NOMES = { cesar: 'César', vigenere: 'Vigenère', atbash: 'Atbash', xor: 'XOR', xor2: 'XOR de 2 bits', auto: 'Automático' };

// ======================================================================
// Telas e atalhos
// ======================================================================
const TELAS = ['descriptografar', 'criptografar', 'codigos'];
const DICAS = {
  descriptografar:
    '↑ ↓ escolher · → ou Enter abrir pasta · ← voltar · Espaço carregar o .txt e descriptografar · ' +
    'Ctrl+D Downloads · Ctrl+L lugares · Ctrl+C copiar a resposta · Esc volta para a lista',
  criptografar:
    'Tab / Shift+Tab trocam de campo · ↑ ↓ escolhem nas listas · Enter criptografa · ' +
    'depois da cifra: T trocar · S segunda criptografia · R começar de novo',
  codigos: '← → trocam de algoritmo · Tab vai para o código e ↑ ↓ rolam',
};
let telaAtual = 'descriptografar';

function mostrarTela(nome) {
  telaAtual = nome;
  TELAS.forEach((tela) => {
    $(`tela-${tela}`).hidden = tela !== nome;
  });
  document.querySelectorAll('.navegacao [data-tela]').forEach((botao) => {
    const ativo = botao.dataset.tela === nome;
    botao.classList.toggle('ativa', ativo);
    botao.setAttribute('aria-selected', String(ativo));
  });
  $('dicas').textContent = DICAS[nome];
  if (nome === 'descriptografar') {
    $('lista').focus({ preventScroll: true });
  } else if (nome === 'criptografar') {
    focarCriptografar();
  } else {
    carregarCodigos().then(() => {
      const ativa = document.querySelector('#abas-codigo .aba.ativa');
      if (ativa) {
        ativa.focus();
      }
    });
  }
}

document.querySelectorAll('.navegacao [data-tela]').forEach((botao) => {
  botao.addEventListener('click', () => mostrarTela(botao.dataset.tela));
});

// ← → entre as abas do topo
$('tela-descriptografar').parentElement.querySelector('.navegacao').addEventListener('keydown', (e) => {
  if (e.key !== 'ArrowRight' && e.key !== 'ArrowLeft') {
    return;
  }
  const passo = e.key === 'ArrowRight' ? 1 : -1;
  const proxima = TELAS[(TELAS.indexOf(telaAtual) + passo + TELAS.length) % TELAS.length];
  mostrarTela(proxima);
  document.querySelector(`.navegacao [data-tela="${proxima}"]`).focus();
  e.preventDefault();
});

function emCampoDeTexto() {
  const ativo = document.activeElement;
  return ativo && ['INPUT', 'TEXTAREA', 'SELECT'].includes(ativo.tagName);
}

document.addEventListener('keydown', (e) => {
  if (e.ctrlKey && !e.altKey && !e.shiftKey && !e.metaKey) {
    const indice = ['1', '2', '3'].indexOf(e.key);
    if (indice >= 0) {
      mostrarTela(TELAS[indice]);
      e.preventDefault();
      return;
    }
    if (telaAtual === 'descriptografar') {
      const tecla = e.key.toLowerCase();
      if (tecla === 'd') {
        abrirPasta(pastaDownloads);
        $('lista').focus();
        e.preventDefault();
      } else if (tecla === 'l') {
        abrirPasta('', nav.caminho);
        $('lista').focus();
        e.preventDefault();
      } else if (tecla === 'c' && !emCampoDeTexto() && window.getSelection().toString() === '') {
        copiarResposta();
        e.preventDefault();
      }
    } else if (telaAtual === 'criptografar' && e.key === 'Enter' && !$('passo').hidden) {
      aplicarCifra();
      e.preventDefault();
    }
    return;
  }

  if (e.key === 'Escape') {
    if (telaAtual === 'descriptografar') {
      $('lista').focus();
    } else if (telaAtual === 'criptografar') {
      if (!$('btn-cancelar-segunda').hidden) {
        cancelarSegunda();
      } else {
        focarCriptografar();
      }
    }
    return;
  }

  // T / S / R na tela de criptografia, quando nenhum campo está sendo digitado
  if (telaAtual === 'criptografar' && !$('decisao').hidden && !emCampoDeTexto() && !e.ctrlKey && !e.altKey && !e.metaKey) {
    const tecla = e.key.toLowerCase();
    if (tecla === 't') {
      trocarCifra();
    } else if (tecla === 's' && !$('btn-segunda').disabled) {
      pedirSegunda();
    } else if (tecla === 'r') {
      reiniciarCifra();
    } else {
      return;
    }
    e.preventDefault();
  }
});

async function copiar(texto, onde) {
  await window.cripto.copiar(texto);
  onde('Copiado para a área de transferência.', 'ok');
}

function definirStatus(elemento, mensagem, tipo) {
  elemento.textContent = mensagem;
  elemento.className = `status ${tipo || ''}`.trim();
}

// Caracteres de controle (comuns no XOR) ficam visíveis só na tela;
// copiar e salvar usam o texto original.
function textoVisivel(texto) {
  return texto.replace(/[\x00-\x09\x0B-\x1F\x7F-\x9F]/g, (c) => {
    const codigo = c.charCodeAt(0);
    if (codigo < 0x20) {
      return String.fromCharCode(0x2400 + codigo);
    }
    return codigo === 0x7f ? '␡' : '·';
  });
}

// ======================================================================
// Navegador de arquivos (só pelo teclado, se quiser)
// ======================================================================
let pastaDownloads = '';
let nav = { caminho: '', pai: null, itens: [], indice: 0 };
let seqAnalise = 0;

function formatarTamanho(bytes) {
  return bytes < 1024 ? `${bytes} B` : `${(bytes / 1024).toFixed(1).replace('.', ',')} KB`;
}

function formatarData(ms) {
  return new Date(ms).toLocaleString('pt-BR', { dateStyle: 'short', timeStyle: 'short' });
}

async function abrirPasta(caminho, selecionar) {
  const resposta = await window.cripto.listar(caminho);
  if (resposta.erro) {
    $('aviso-lista').textContent = resposta.erro;
    $('aviso-lista').className = 'dica status erro';
    return false;
  }
  $('aviso-lista').textContent = '';
  $('aviso-lista').className = 'dica';

  const itens = [];
  if (resposta.pai !== null) {
    itens.push({
      tipo: 'voltar',
      nome: resposta.pai === '' ? '.. (voltar para os Lugares)' : '.. (pasta anterior)',
      caminho: resposta.pai,
    });
  }
  resposta.entradas.forEach((entrada) => itens.push(entrada));

  // Seleção inicial: a pasta de onde viemos; senão o .txt mais recente (Espaço já carrega ele).
  let indice = selecionar ? itens.findIndex((item) => item.caminho === selecionar) : -1;
  if (indice < 0) {
    indice = itens.findIndex((item) => item.tipo === 'arquivo');
  }
  if (indice < 0) {
    indice = itens.findIndex((item) => item.tipo !== 'voltar');
  }
  nav = { caminho: resposta.caminho, pai: resposta.pai, itens, indice: Math.max(indice, 0) };
  $('caminho').textContent = resposta.titulo;
  desenharLista();
  return true;
}

function desenharLista() {
  const lista = $('lista');
  lista.replaceChildren();

  if (nav.itens.filter((item) => item.tipo !== 'voltar').length === 0) {
    const vazio = document.createElement('li');
    vazio.className = 'vazio';
    vazio.textContent = 'Nenhuma pasta ou arquivo .txt aqui.';
    lista.appendChild(vazio);
  }

  nav.itens.forEach((item, i) => {
    const linha = document.createElement('li');
    const marca = document.createElement('span');
    const nome = document.createElement('span');
    const detalhe = document.createElement('span');

    linha.id = `item-${i}`;
    linha.setAttribute('role', 'option');
    linha.className = item.tipo === 'arquivo' ? 'arquivo' : 'pasta';
    marca.className = 'marca';
    marca.textContent = item.tipo === 'arquivo' ? '▪' : item.tipo === 'voltar' ? '↰' : '▸';
    nome.className = 'nome';
    nome.textContent = item.nome;
    detalhe.className = 'detalhe';
    detalhe.textContent = item.tipo === 'arquivo' ? `${formatarTamanho(item.tamanho)} · ${formatarData(item.modificado)}` : '';
    linha.append(marca, nome, detalhe);
    linha.addEventListener('click', () => {
      selecionarIndice(i);
      $('lista').focus();
    });
    linha.addEventListener('dblclick', () => ativar());
    lista.appendChild(linha);
  });
  selecionarIndice(nav.indice);
}

function selecionarIndice(indice) {
  if (nav.itens.length === 0) {
    return;
  }
  nav.indice = Math.min(Math.max(indice, 0), nav.itens.length - 1);
  nav.itens.forEach((_, i) => {
    const linha = $(`item-${i}`);
    const selecionado = i === nav.indice;
    linha.classList.toggle('selecionado', selecionado);
    linha.setAttribute('aria-selected', String(selecionado));
  });
  const atual = $(`item-${nav.indice}`);
  $('lista').setAttribute('aria-activedescendant', atual.id);
  atual.scrollIntoView({ block: 'nearest' });
}

function voltarPasta() {
  if (nav.pai !== null) {
    abrirPasta(nav.pai, nav.caminho);
  }
}

// Espaço / Enter: abre a pasta ou, em um .txt, carrega e descriptografa na hora.
function ativar() {
  const item = nav.itens[nav.indice];
  if (!item) {
    return;
  }
  if (item.tipo === 'voltar') {
    voltarPasta();
  } else if (item.tipo === 'pasta') {
    abrirPasta(item.caminho);
  } else {
    analisarArquivo(item);
  }
}

$('lista').addEventListener('keydown', (e) => {
  if (e.ctrlKey || e.altKey || e.metaKey) {
    return;
  }
  const ultimo = nav.itens.length - 1;
  switch (e.key) {
    case 'ArrowDown':
      selecionarIndice(nav.indice + 1);
      break;
    case 'ArrowUp':
      selecionarIndice(nav.indice - 1);
      break;
    case 'PageDown':
      selecionarIndice(nav.indice + 8);
      break;
    case 'PageUp':
      selecionarIndice(nav.indice - 8);
      break;
    case 'Home':
      selecionarIndice(0);
      break;
    case 'End':
      selecionarIndice(ultimo);
      break;
    case 'ArrowRight': {
      const item = nav.itens[nav.indice];
      if (item && item.tipo === 'pasta') {
        abrirPasta(item.caminho);
      }
      break;
    }
    case 'ArrowLeft':
    case 'Backspace':
      voltarPasta();
      break;
    case 'Enter':
    case ' ':
      ativar();
      break;
    default:
      return;
  }
  e.preventDefault();
});

// ======================================================================
// Resultado da descriptografia
// ======================================================================
const estadoResultado = { claro: '', resposta: '' };

// O motor escreve no stderr linhas que começam com @ (cifras, etapas, chave,
// valores, verificação e outras possibilidades); o resto são avisos.
function interpretarMensagem(mensagem) {
  const info = {
    cifras: '', chave: '', etapas: [], valores: [], opcoes: [], avisos: [], verificado: '',
    status: '', detalheStatus: '', idioma: '', pontuacao: null, evidencias: [], passos: [], decomposicao: '', nota: '',
    equivalente: [], verificadoEq: '', estrutura: '',
  };

  (mensagem || '').split('\n').forEach((linha) => {
    if (linha.startsWith('@cifras ')) {
      info.cifras = linha.slice(8);
    } else if (linha.startsWith('@chave ')) {
      info.chave = linha.slice(7);
    } else if (linha.startsWith('@etapa ')) {
      const [cifra, chave] = linha.slice(7).split(' | ');
      info.etapas.push({ cifra, chave });
    } else if (linha.startsWith('@verificado ')) {
      info.verificado = linha.slice(12).trim();
    } else if (linha.startsWith('@etapaeq ')) {
      const [cifra, chave] = linha.slice(9).split(' | ');
      info.equivalente.push({ cifra, chave });
    } else if (linha.startsWith('@verificadoeq ')) {
      info.verificadoEq = linha.slice(14).trim();
    } else if (linha.startsWith('@status ')) {
      const [estado, detalhe = ''] = linha.slice(8).split(' | ');
      info.status = estado.trim();
      info.detalheStatus = detalhe.trim();
    } else if (linha.startsWith('@idioma ')) {
      info.idioma = linha.slice(8).trim();
    } else if (linha.startsWith('@pontuacao ')) {
      const [media, limiar, letras] = linha.slice(11).split(' | ').map(Number);
      info.pontuacao = { media, limiar, letras };
    } else if (linha.startsWith('@evidencia ')) {
      info.evidencias.push(linha.slice(11));
    } else if (linha.startsWith('@passo ')) {
      const [numero, acao, ic, media, idioma, veredito] = linha.slice(7).split(' | ');
      info.passos.push({ numero, acao, ic, media, idioma, veredito });
    } else if (linha.startsWith('@decomposicao ')) {
      info.decomposicao = linha.slice(14);
    } else if (linha.startsWith('@nota ')) {
      info.nota = linha.slice(6);
    } else if (linha.startsWith('@estrutura ')) {
      info.estrutura = linha.slice(11);
    } else if (linha.startsWith('@valor ')) {
      const [nome, decimal, hexa, binario] = linha.slice(7).split(' | ');
      info.valores.push({ nome, decimal, hexa, binario });
    } else if (linha.startsWith('@opcao ')) {
      const [descricao, ...resto] = linha.slice(7).split(' :: ');
      info.opcoes.push({ descricao, previa: resto.join(' :: ') });
    } else if (linha.trim() !== '') {
      info.avisos.push(linha);
    }
  });
  return info;
}

function celula(texto) {
  const td = document.createElement('td');
  td.textContent = texto;
  return td;
}

function preencherTabela(corpo, linhas) {
  corpo.replaceChildren();
  linhas.forEach((colunas) => {
    const linha = document.createElement('tr');
    colunas.forEach((texto) => linha.appendChild(celula(texto)));
    corpo.appendChild(linha);
  });
}

// Formato pedido pelo regulamento: texto claro, cifras na ordem de aplicação e chaves.
function montarResposta(claro, etapas, equivalente = []) {
  if (etapas.length === 0) {
    return `Texto claro: ${claro}\nCifras: nenhuma (o texto já está em claro)`;
  }
  const linhas = [
    `Texto claro: ${claro}`,
    `Cifras, na ordem em que foram aplicadas: ${etapas.map((e, i) => `${i + 1}) ${e.cifra}`).join('  ')}`,
    `Chaves: ${etapas.map((e, i) => `${i + 1}) ${e.cifra}: ${e.chave}`).join('  ')}`,
  ];
  if (equivalente.length) {   // com Atbash, a outra forma (Atbash do outro lado, chave negada) dá o mesmo texto cifrado
    linhas.push(`Forma equivalente, que também reproduz o texto cifrado: ${equivalente.map((e, i) => `${i + 1}) ${e.cifra}: ${e.chave}`).join('  ')}`);
  }
  return linhas.join('\n');
}

function preencherEvidencias(lista, evidencias) {
  lista.replaceChildren();
  evidencias.forEach((texto) => {
    const item = document.createElement('li');
    item.textContent = texto;
    lista.appendChild(item);
  });
}

// Nenhuma sequência de cifras produziu texto natural: não mostra nenhum texto como se fosse o resultado.
function exibirInconclusivo(info, meta, resposta) {
  estadoResultado.claro = '';
  estadoResultado.resposta = '';
  $('resultado-vazio').hidden = true;
  $('resultado-conteudo').hidden = true;
  $('resultado-inconclusivo').hidden = false;
  $('inconclusivo-titulo').textContent = `${meta.titulo}: resultado inconclusivo (${resposta.ms} ms)`;
  $('inconclusivo-motivo').textContent = info.detalheStatus;
  preencherEvidencias($('inconclusivo-evidencias'), info.evidencias);
  const p = info.pontuacao;
  $('inconclusivo-pontuacao').textContent = p
    ? `Melhor média de trigramas encontrada: ${p.media.toFixed(2)}; para aceitar um texto de ${p.letras} letras seria preciso pelo menos ${p.limiar.toFixed(2)}.`
    : '';
  const tentativa = info.opcoes.find((opcao) => opcao.descricao.startsWith('melhor tentativa'));
  $('inconclusivo-tentativa').hidden = !tentativa;
  $('inconclusivo-texto').textContent = tentativa ? tentativa.previa : '';
}

function mostrarMensagemVazia(texto, tipo) {
  $('resultado-inconclusivo').hidden = true;
  $('resultado-vazio').hidden = false;
  $('resultado-vazio').textContent = texto;
  $('resultado-vazio').className = tipo === 'erro' ? 'status erro' : 'discreto';
  $('resultado-conteudo').hidden = true;
}

function exibirResultado(resposta, meta) {
  if (!resposta.ok) {
    mostrarMensagemVazia(resposta.mensagem || 'Erro ao executar o motor em C.', 'erro');
    return;
  }

  const info = interpretarMensagem(resposta.mensagem);
  if (info.status === 'inconclusivo') {
    exibirInconclusivo(info, meta, resposta);
    return;
  }
  const claro = resposta.saida.replace(/\n$/, '');
  let etapas = info.etapas;
  if (etapas.length === 0 && meta.algoritmo && meta.algoritmo !== 'auto') {
    // botões individuais: uma única cifra
    const chave = meta.algoritmo === 'atbash' ? 'sem chave' : info.chave || meta.chave || '';
    etapas = [{ cifra: NOMES[meta.algoritmo], chave }];
  }

  estadoResultado.claro = claro;
  estadoResultado.resposta = montarResposta(claro, etapas, info.equivalente);

  $('resultado-vazio').hidden = true;
  $('resultado-inconclusivo').hidden = true;
  $('resultado-conteudo').hidden = false;
  $('resultado-titulo').textContent = meta.titulo;
  $('ideal-texto').textContent = etapas.length ? etapas.map((e) => e.cifra).join(' → ') : info.cifras || 'nenhuma cifra';
  $('ideal-tempo').textContent = `(analisado em ${resposta.ms} ms)`;
  // César, Vigenère e Atbash em sequência se fundem (ver o README): a tabela mostra a forma mais simples que
  // reproduz o arquivo, com até 3 etapas. O original pode ter usado mais cifras (ex.: César → Vigenère somam).
  $('dupla').hidden = etapas.length < 2;
  $('dupla').textContent = etapas.length >= 2
    ? `Criptografia múltipla identificada: ${etapas.length} cifras em sequência na forma mais simples que reproduz o arquivo`
    : '';
  $('dupla').title = 'César e Vigenère somam deslocamentos e o Atbash pode trocar de lado: 3 ou 4 cifras de letras podem ' +
    'equivaler a menos etapas. A tabela mostra uma forma conferida que reproduz exatamente o arquivo.';
  $('aviso-estrutura').hidden = info.estrutura === '';
  $('aviso-estrutura').textContent = info.estrutura ? `⚠ ${info.estrutura}` : '';

  const verificacao = $('verificacao');
  if (info.verificado === 'sim') {
    verificacao.textContent = '✔ Conferido: cifrando o texto claro com essas cifras e chaves, o resultado é exatamente o arquivo.';
    verificacao.className = 'verificacao ok';
  } else if (info.verificado === 'nao') {
    verificacao.textContent = '✖ Não confere: cifrar o texto claro com essas cifras e chaves não reproduz o arquivo. Veja as outras possibilidades.';
    verificacao.className = 'verificacao falhou';
  } else {
    verificacao.textContent = '';
    verificacao.className = 'verificacao';
  }

  const confianca = $('confianca');
  if (info.status === 'conclusivo') {
    const p = info.pontuacao;
    confianca.textContent = `Conclusivo · ${info.detalheStatus}${info.idioma ? ` · idioma: ${info.idioma}` : ''}` +
      (p ? ` · média de trigramas ${p.media.toFixed(2)} (limiar ${p.limiar.toFixed(2)} para ${p.letras} letras)` : '');
  } else {
    confianca.textContent = '';
  }
  const textoEquivalente = info.equivalente.map((e) => `${e.cifra} (${e.chave})`).join(' → ');
  $('equivalente').hidden = info.equivalente.length === 0;
  $('equivalente').textContent = info.equivalente.length
    ? `Forma equivalente ${info.verificadoEq === 'sim' ? '(também conferida: reproduz o arquivo)' : '(NÃO conferiu)'}: ${textoEquivalente}. ` +
      'O texto cifrado é o mesmo com o Atbash de qualquer lado; só a chave troca de sinal.'
    : '';
  $('decomposicao').hidden = info.decomposicao === '';
  $('decomposicao').textContent = info.decomposicao ? `Decomposição compatível em duas camadas: ${info.decomposicao}` : '';
  $('nota-camadas').hidden = info.nota === '';
  $('nota-camadas').textContent = info.nota;
  $('tabela-passos').hidden = info.passos.length === 0;
  preencherTabela($('corpo-passos'), info.passos.map((p) => [p.numero, p.acao, p.ic, p.media, p.idioma, p.veredito]));
  preencherEvidencias($('lista-evidencias'), info.evidencias);

  $('tabela-etapas').hidden = etapas.length === 0;
  preencherTabela($('corpo-etapas'), etapas.map((e, i) => [String(i + 1), e.cifra, e.chave]));
  $('tabela-chave').hidden = info.valores.length === 0;
  preencherTabela($('corpo-chave'), info.valores.map((v) => [v.nome, v.decimal, v.hexa, v.binario]));

  $('resposta').textContent = estadoResultado.resposta;
  definirStatus($('status'), info.avisos.join('\n'), info.avisos.length ? 'aviso' : '');

  const lista = $('lista-opcoes');
  lista.replaceChildren();
  $('detalhes-opcoes').hidden = info.opcoes.length === 0;
  info.opcoes.forEach((opcao) => {
    const item = document.createElement('li');
    const previa = document.createElement('span');
    item.appendChild(document.createTextNode(opcao.descricao));
    previa.className = 'previa';
    previa.textContent = opcao.previa;
    item.appendChild(previa);
    lista.appendChild(item);
  });
}

// Depois de carregar um arquivo (ou usar o modo manual), o resultado passa a ficar ACIMA da lista de arquivos
// e a tela volta para o topo: a primeira coisa vista é a descriptografia. A lista continua logo abaixo.
function resultadoParaOTopo() {
  $('tela-descriptografar').classList.add('com-resultado');
  window.scrollTo(0, 0);
}

async function analisarArquivo(item) {
  const minha = ++seqAnalise;
  resultadoParaOTopo();
  mostrarMensagemVazia(`Analisando ${item.nome}…`);
  const resposta = await window.cripto.analisarArquivo(item.caminho);
  if (minha !== seqAnalise) {
    return;
  }
  exibirResultado(resposta, { titulo: `${item.nome} · ${resposta.tamanho ?? item.tamanho} bytes` });
  resultadoParaOTopo();
}

function copiarResposta() {
  if (estadoResultado.resposta !== '') {
    copiar(estadoResultado.resposta, (m, t) => definirStatus($('status'), m, t));
  }
}

$('copiar-resposta').addEventListener('click', copiarResposta);
$('copiar-claro').addEventListener('click', () => {
  copiar(estadoResultado.claro, (m, t) => definirStatus($('status'), m, t));
});

// ---------- modo manual ----------
document.querySelectorAll('#manual [data-algoritmo]').forEach((botao) => {
  botao.addEventListener('click', async () => {
    const algoritmo = botao.dataset.algoritmo;
    const texto = $('manual-texto').value;
    const chave = $('manual-chave').value.trim();

    if (texto === '') {
      mostrarMensagemVazia('Cole o texto cifrado no campo do modo manual.', 'erro');
      return;
    }
    const inicio = performance.now();
    const resposta = await window.cripto.executar({
      algoritmo,
      modo: 'decifrar',
      chave,
      texto,
      maxCamadas: Number($('manual-camadas').value) || undefined,
      maxPeriodo: Number($('manual-periodo').value) || undefined,
    });
    resposta.ms = Math.round(performance.now() - inicio);
    exibirResultado(resposta, { titulo: `Texto colado · ${NOMES[algoritmo]}`, algoritmo, chave });
    resultadoParaOTopo();
  });
});

// ======================================================================
// Criptografar: uma cifra por vez, sempre por escolha do usuário
// ======================================================================
// Orçamento em décimos de bit (4,7 bits por letra ou deslocamento = 47).
const FASES = {
  1: { orcamento: 160, max: 1 },
  2: { orcamento: 240, max: 1 },
  3: { orcamento: 320, max: 2 },
  4: { orcamento: 400, max: 2 },
};
const DECIMOS_POR_LETRA = 47;
const estadoCifra = { etapas: [], segunda: false };

const baseDe = (algoritmo) => (algoritmo === 'xor2' ? 'xor' : algoritmo);
const formatarBits = (decimos) => (decimos / 10).toFixed(1).replace('.', ',');
const faseAtual = () => Number($('fase').value);
const custoUsado = () => estadoCifra.etapas.reduce((soma, e) => soma + e.custo, 0);
const textoClaro = () => $('claro').value;

function normalizarTexto(texto) {
  return texto.normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase().replace(/[^a-z]/g, '');
}

// Interpreta a chave digitada e calcula o custo em bits da cifra.
function analisarChave(algoritmo, bruto) {
  const texto = bruto.trim();

  if (algoritmo === 'atbash') {
    return { custo: 0, chave: '', rotulo: 'sem chave' };
  }
  if (algoritmo === 'cesar') {
    if (!/^\d+$/.test(texto) || Number(texto) < 1 || Number(texto) > 25) {
      return { erro: 'A chave do César é um número de 1 a 25.' };
    }
    return { custo: DECIMOS_POR_LETRA, chave: String(Number(texto)), rotulo: String(Number(texto)) };
  }
  if (algoritmo === 'vigenere') {
    if (!/^[a-zA-Z]+$/.test(texto)) {
      return { erro: 'A senha do Vigenère usa só letras de a a z (sem espaços, acentos ou números).' };
    }
    if (texto.length > 8) {
      return { erro: 'A senha do Vigenère tem no máximo 8 letras.' };
    }
    const senha = texto.toLowerCase();
    return { custo: DECIMOS_POR_LETRA * senha.length, chave: senha, rotulo: senha };
  }

  // xor e xor2: bytes inteiros, em decimal ou 0x..
  const partes = texto.split(/[\s,;]+/).filter(Boolean);
  if (partes.length === 0 || !partes.every((p) => /^(0x[0-9a-f]{1,2}|\d{1,3})$/i.test(p))) {
    return { erro: 'Digite bytes de 0 a 255 separados por vírgula, ex.: 200, 17, 0x33.' };
  }
  const valores = partes.map((p) => Number(p));
  if (valores.some((v) => v > 255)) {
    return { erro: 'Cada byte do XOR vai de 0 a 255.' };
  }
  if (algoritmo === 'xor2') {
    if (valores.length !== 1 || valores[0] > 3) {
      return { erro: 'O XOR de 2 bits usa um único valor de 0 a 3.' };
    }
    return { custo: 20, chave: String(valores[0]), rotulo: String(valores[0]) };
  }
  return { custo: 80 * valores.length, chave: valores.join(','), rotulo: valores.join(', ') };
}

// Regras do regulamento para a cifra que o usuário quer aplicar agora.
function validarEtapa(algoritmo, analise) {
  const fase = FASES[faseAtual()];
  const usadas = estadoCifra.etapas.map((e) => baseDe(e.algoritmo));

  if (estadoCifra.etapas.length >= fase.max) {
    return fase.max === 1 ? 'Esta fase permite uma única cifra.' : 'Esta fase permite no máximo duas cifras.';
  }
  if (algoritmo === 'xor2' && faseAtual() !== 1) {
    return 'O XOR de 2 bits só vale na fase 1.';
  }
  if (usadas.includes(baseDe(algoritmo))) {
    return 'As duas cifras precisam ser diferentes.';
  }
  if (usadas.includes('xor')) {
    return 'O XOR deve ser sempre a última cifra.';
  }
  const total = custoUsado() + analise.custo;
  if (total > fase.orcamento) {
    return `Passa do orçamento: seriam ${formatarBits(total)} bits e a fase tem ${formatarBits(fase.orcamento)}.`;
  }
  return null;
}

function regrasDoTexto() {
  const texto = textoClaro();
  const letras = texto.length;
  return [
    { ok: /^[a-z]+$/.test(texto), texto: 'Só letras minúsculas de a a z (sem espaços, acentos, números ou símbolos)' },
    { ok: letras >= 200 && letras <= 250, texto: `Entre 200 e 250 letras (agora: ${letras})` },
  ];
}

function atualizarRegrasDoTexto() {
  const regras = regrasDoTexto();
  const lista = $('regras-texto');
  lista.replaceChildren();
  regras.forEach((regra) => {
    const item = document.createElement('li');
    item.className = regra.ok ? 'ok' : 'falha';
    item.textContent = regra.texto;
    lista.appendChild(item);
  });
  $('contador').textContent = `${textoClaro().length} caracteres`;
}

function atualizarOrcamento() {
  const fase = FASES[faseAtual()];
  const usado = custoUsado();
  $('orcamento').textContent = `${formatarBits(usado)} de ${formatarBits(fase.orcamento)} bits usados`;
  $('orcamento').classList.toggle('estourou', usado > fase.orcamento);
}

const DICAS_CHAVE = {
  cesar: 'Deslocamento de 1 a 25. Custa 4,7 bits.',
  vigenere: 'Senha de 1 a 8 letras (a-z). Custa 4,7 bits por letra.',
  atbash: 'Atbash não tem chave e custa 0 bit.',
  xor: 'Bytes de 0 a 255 separados por vírgula (ex.: 200, 17, 0x33). Custa 8 bits por byte.',
  xor2: 'Um valor de 0 a 3, aplicado aos 2 bits menos significativos. Custa 2 bits (só na fase 1).',
};

function montarOpcoesDeCifra() {
  const usadas = estadoCifra.etapas.map((e) => baseDe(e.algoritmo));
  const opcoes = ['cesar', 'vigenere', 'atbash', 'xor'];
  if (faseAtual() === 1) {
    opcoes.push('xor2');
  }
  const anterior = $('cifra').value;
  $('cifra').replaceChildren();
  opcoes
    .filter((algoritmo) => !usadas.includes(baseDe(algoritmo)))
    .forEach((algoritmo) => {
      const opcao = document.createElement('option');
      opcao.value = algoritmo;
      opcao.textContent = algoritmo === 'xor' ? 'XOR (bytes inteiros)' : NOMES[algoritmo];
      $('cifra').appendChild(opcao);
    });
  if ([...$('cifra').options].some((o) => o.value === anterior)) {
    $('cifra').value = anterior;
  }
}

function atualizarPasso() {
  const algoritmo = $('cifra').value;
  const campo = $('chave-cifrar');
  const digitado = campo.value.trim() !== '';

  campo.disabled = algoritmo === 'atbash';
  campo.placeholder = algoritmo === 'atbash' ? 'sem chave' : '';
  $('dica-chave').textContent = DICAS_CHAVE[algoritmo] || '';

  $('erro-passo').textContent = '';
  if (algoritmo === 'atbash' || digitado) {
    const analise = analisarChave(algoritmo, campo.value);
    const erro = analise.erro || validarEtapa(algoritmo, analise);
    if (erro) {
      $('erro-passo').textContent = erro;
    } else {
      $('dica-chave').textContent += ` Esta cifra: ${formatarBits(analise.custo)} bits · total ${formatarBits(custoUsado() + analise.custo)} de ${formatarBits(FASES[faseAtual()].orcamento)}.`;
    }
  }
}

function descreverCadeia() {
  return ['Texto claro', ...estadoCifra.etapas.map((e) => `${NOMES[e.algoritmo]} (${e.rotulo})`)].join(' → ');
}

function montarFicha() {
  const etapas = estadoCifra.etapas;
  const problemas = regrasDoTexto().filter((r) => !r.ok).map((r) => r.texto);
  const ultima = etapas[etapas.length - 1];
  const linhas = [];

  if (problemas.length) {
    linhas.push(`ATENÇÃO: o texto claro não cumpre estas regras: ${problemas.join('; ')}.`);
  }
  linhas.push(
    `FICHA DE ENTREGA · Fase ${faseAtual()}`,
    `Texto claro: ${textoClaro()}`,
    `Cifras, na ordem em que foram aplicadas: ${etapas.map((e, i) => `${i + 1}) ${NOMES[e.algoritmo]}`).join('  ')}`,
    `Chaves: ${etapas.map((e, i) => `${i + 1}) ${NOMES[e.algoritmo]}: ${e.rotulo}`).join('  ')}`,
    `Custo da chave: ${formatarBits(custoUsado())} de ${formatarBits(FASES[faseAtual()].orcamento)} bits`,
    baseDe(ultima.algoritmo) === 'xor'
      ? 'Texto cifrado: arquivo .txt em UTF-8 (salvo pelo botão acima)'
      : `Texto cifrado: ${ultima.saida}`,
  );
  return linhas.join('\n');
}

function renderCriptografar() {
  const { etapas, segunda } = estadoCifra;
  const fase = FASES[faseAtual()];
  const iniciou = etapas.length > 0;

  $('fase').disabled = iniciou;
  $('claro').readOnly = iniciou;
  $('normalizar').disabled = iniciou;
  $('passo').hidden = iniciou && !segunda;
  $('titulo-passo').textContent = iniciou ? 'Cifra 2: aplicada sobre o resultado da cifra 1' : 'Cifra 1';
  $('btn-cancelar-segunda').hidden = !(iniciou && segunda);
  $('decisao').hidden = !(iniciou && !segunda);
  $('saida-cifra').hidden = !iniciou;

  montarOpcoesDeCifra();
  atualizarOrcamento();
  atualizarRegrasDoTexto();
  atualizarPasso();

  if (iniciou) {
    const ultima = etapas[etapas.length - 1];
    $('cadeia').textContent = `${descreverCadeia()}  ·  ${formatarBits(custoUsado())} de ${formatarBits(fase.orcamento)} bits`;
    $('cifrado').textContent = textoVisivel(ultima.saida);
    $('ficha').textContent = montarFicha();

    const completo = etapas.length >= fase.max;
    $('texto-decisao').textContent = completo
      ? `Texto criptografado com ${etapas.map((e) => NOMES[e.algoritmo]).join(' e depois ')}. Esta fase não permite outra cifra.`
      : `Texto criptografado com ${NOMES[ultima.algoritmo]}. O que você quer fazer agora?`;
    $('btn-segunda').hidden = completo;
    $('btn-trocar').textContent = completo ? 'Trocar a última criptografia ' : 'Trocar de criptografia ';
    $('btn-trocar').insertAdjacentHTML('beforeend', '<kbd>T</kbd>');

    let motivo = '';
    if (!completo && baseDe(ultima.algoritmo) === 'xor') {
      motivo = 'O XOR deve ser sempre a última cifra, então não dá para aplicar outra depois dele.';
    }
    $('btn-segunda').disabled = motivo !== '';
    $('motivo-segunda').textContent =
      motivo || (completo ? '' : '“Trocar” desfaz esta cifra e volta para escolher outra. “Segunda” mantém esta e cifra o resultado de novo.');
  }
}

function focarCriptografar() {
  if (!$('decisao').hidden) {
    (!$('btn-segunda').hidden && !$('btn-segunda').disabled ? $('btn-segunda') : $('btn-trocar')).focus();
  } else if (!$('passo').hidden && estadoCifra.etapas.length > 0) {
    $('cifra').focus();
  } else {
    $('claro').focus();
  }
}

async function aplicarCifra() {
  const algoritmo = $('cifra').value;
  const primeira = estadoCifra.etapas.length === 0;

  if (primeira && !/^[a-z]+$/.test(textoClaro())) {
    $('erro-passo').textContent = 'Ajuste o texto claro (só letras a-z, veja as regras acima) ou use o botão “Limpar texto”.';
    return;
  }
  const analise = analisarChave(algoritmo, $('chave-cifrar').value);
  const erro = analise.erro || validarEtapa(algoritmo, analise);
  if (erro) {
    $('erro-passo').textContent = erro;
    return;
  }

  // Cada clique aplica UMA cifra: sobre o texto claro (cifra 1) ou sobre o resultado da cifra 1 (cifra 2).
  const entrada = primeira ? textoClaro() : estadoCifra.etapas[estadoCifra.etapas.length - 1].saida;
  const resposta = await window.cripto.executar({
    algoritmo: baseDe(algoritmo),
    modo: 'cifrar',
    chave: analise.chave,
    texto: entrada,
  });
  if (!resposta.ok) {
    $('erro-passo').textContent = resposta.mensagem || 'Erro ao executar o motor em C.';
    return;
  }

  estadoCifra.etapas.push({ algoritmo, chave: analise.chave, rotulo: analise.rotulo, custo: analise.custo, saida: resposta.saida });
  estadoCifra.segunda = false;
  $('chave-cifrar').value = '';
  definirStatus($('status-cifra'), '', '');
  renderCriptografar();
  focarCriptografar();
}

function trocarCifra() {
  estadoCifra.etapas.pop();
  estadoCifra.segunda = false;
  renderCriptografar();
  focarCriptografar();
}

function pedirSegunda() {
  estadoCifra.segunda = true;
  $('chave-cifrar').value = '';
  renderCriptografar();
  $('cifra').focus();
}

function cancelarSegunda() {
  estadoCifra.segunda = false;
  renderCriptografar();
  focarCriptografar();
}

function reiniciarCifra() {
  estadoCifra.etapas = [];
  estadoCifra.segunda = false;
  $('chave-cifrar').value = '';
  renderCriptografar();
  $('claro').focus();
}

$('btn-cifrar').addEventListener('click', aplicarCifra);
$('btn-trocar').addEventListener('click', trocarCifra);
$('btn-segunda').addEventListener('click', pedirSegunda);
$('btn-reiniciar').addEventListener('click', reiniciarCifra);
$('btn-cancelar-segunda').addEventListener('click', cancelarSegunda);
$('fase').addEventListener('change', renderCriptografar);
$('cifra').addEventListener('change', () => {
  $('chave-cifrar').value = '';
  atualizarPasso();
});
$('chave-cifrar').addEventListener('input', atualizarPasso);
$('chave-cifrar').addEventListener('keydown', (e) => {
  if (e.key === 'Enter') {
    aplicarCifra();
    e.preventDefault();
  }
});
$('claro').addEventListener('input', atualizarRegrasDoTexto);
$('normalizar').addEventListener('click', () => {
  $('claro').value = normalizarTexto(textoClaro());
  atualizarRegrasDoTexto();
  $('claro').focus();
});

$('copiar-cifrado').addEventListener('click', () => {
  copiar(estadoCifra.etapas[estadoCifra.etapas.length - 1].saida, (m, t) => definirStatus($('status-cifra'), m, t));
});
$('copiar-ficha').addEventListener('click', () => {
  copiar(montarFicha(), (m, t) => definirStatus($('status-cifra'), m, t));
});
$('salvar-cifrado').addEventListener('click', async () => {
  const ultima = estadoCifra.etapas[estadoCifra.etapas.length - 1];
  const resposta = await window.cripto.salvarDownloads(`desafio-fase${faseAtual()}.txt`, ultima.saida);
  if (resposta.salvo) {
    definirStatus($('status-cifra'), `Salvo em ${resposta.caminho}`, 'ok');
  } else {
    definirStatus($('status-cifra'), resposta.erro, 'erro');
  }
});

// ======================================================================
// Códigos dos algoritmos
// ======================================================================
const estadoCodigos = { itens: null };

async function carregarCodigos() {
  if (estadoCodigos.itens !== null) {
    return;
  }
  estadoCodigos.itens = await window.cripto.codigos();
  const abas = $('abas-codigo');
  estadoCodigos.itens.forEach((item) => {
    const botao = document.createElement('button');
    botao.type = 'button';
    botao.className = 'aba';
    botao.dataset.codigo = item.id;
    botao.setAttribute('role', 'tab');
    botao.textContent = item.nome;
    botao.addEventListener('click', () => mostrarCodigo(item.id));
    abas.appendChild(botao);
  });
  mostrarCodigo(estadoCodigos.itens[0].id);
}

$('abas-codigo').addEventListener('keydown', (e) => {
  if (e.key !== 'ArrowRight' && e.key !== 'ArrowLeft') {
    return;
  }
  const botoes = [...document.querySelectorAll('#abas-codigo .aba')];
  const atual = botoes.indexOf(document.activeElement);
  const proximo = botoes[(atual + (e.key === 'ArrowRight' ? 1 : -1) + botoes.length) % botoes.length];
  proximo.focus();
  mostrarCodigo(proximo.dataset.codigo);
  e.preventDefault();
});

function mostrarCodigo(id) {
  const item = estadoCodigos.itens.find((codigo) => codigo.id === id);
  document.querySelectorAll('#abas-codigo .aba').forEach((botao) => {
    const ativo = botao.dataset.codigo === id;
    botao.classList.toggle('ativa', ativo);
    botao.setAttribute('aria-selected', String(ativo));
  });
  $('info-codigo').textContent = item.original
    ? `${item.arquivo} — código original, sem modificações.`
    : `${item.arquivo} — programa em C que a interface executa (mesmas fórmulas dos originais).`;

  const destino = $('codigo');
  destino.replaceChildren();
  item.codigo.replace(/\n$/, '').split('\n').forEach((linha) => {
    const span = document.createElement('span');
    span.className = 'linha';
    span.textContent = linha;
    destino.appendChild(span);
  });
}

// ======================================================================
// Início
// ======================================================================
(async function iniciar() {
  renderCriptografar();
  $('dicas').textContent = DICAS.descriptografar;
  pastaDownloads = await window.cripto.pastaDownloads();
  if (!(await abrirPasta(pastaDownloads))) {
    await abrirPasta('');   // Downloads não existe ou não abre: começa pelos Lugares
  }
  $('lista').focus();
})();
