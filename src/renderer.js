// Lógica da tela. Toda a criptografia é feita pelo motor em C (bin/motor).
const $ = (id) => document.getElementById(id);

const NOMES = { cesar: 'César', vigenere: 'Vigenère', atbash: 'Atbash', xor: 'XOR', auto: 'Automático' };

const estado = {
  entradaBruta: null, // texto exato vindo de arquivo/resultado (o textarea troca \r por \n)
  arquivoXor: null,
  resultado: '',
  codigos: null,
};

// ---------- navegação entre as telas ----------
function mostrarTela(nome) {
  document.querySelectorAll('.tela').forEach((tela) => {
    tela.hidden = tela.id !== `tela-${nome}`;
  });
  document.querySelectorAll('[data-tela]').forEach((botao) => {
    botao.classList.toggle('ativa', botao.dataset.tela === nome);
  });
  if (nome === 'codigos') {
    carregarCodigos();
  }
}

document.querySelectorAll('[data-tela]').forEach((botao) => {
  botao.addEventListener('click', () => mostrarTela(botao.dataset.tela));
});

// ---------- texto de entrada ----------
function textoEntrada() {
  return estado.entradaBruta ?? $('texto').value;
}

function definirTexto(texto) {
  $('texto').value = texto;
  estado.entradaBruta = texto;
  atualizarContador();
}

function atualizarContador() {
  const total = [...textoEntrada().replace(/\r?\n$/, '')].length;
  $('contador').textContent = `${total} caractere${total === 1 ? '' : 's'}`;
}

$('texto').addEventListener('input', () => {
  estado.entradaBruta = null;
  atualizarContador();
});

$('abrir-texto').addEventListener('click', async () => {
  const arquivo = await window.cripto.abrirTxt();
  if (!arquivo) {
    return;
  }
  if (arquivo.erro) {
    mostrarStatus(arquivo.erro, 'erro');
    return;
  }
  limparArquivoXor();
  definirTexto(arquivo.conteudo);
  mostrarStatus(`Texto carregado de ${arquivo.nome}.`, 'ok');
  analisarAutomaticamente();
});

document.querySelectorAll('input[name="modo"]').forEach((opcao) => {
  opcao.addEventListener('change', () => {
    $('rotulo-texto').textContent =
      modoAtual() === 'cifrar' ? 'Texto a ser criptografado' : 'Texto a ser descriptografado';
  });
});

function modoAtual() {
  return document.querySelector('input[name="modo"]:checked').value;
}

// O modo automático só descriptografa.
function atualizarModo() {
  const cifrar = modoAtual() === 'cifrar';
  $('automatico').disabled = cifrar;
  $('automatico').title = cifrar ? 'O automático só descriptografa' : '';
}

document.querySelectorAll('input[name="modo"]').forEach((opcao) => {
  opcao.addEventListener('change', atualizarModo);
});

// ---------- arquivo do XOR ----------
$('abrir-xor').addEventListener('click', async () => {
  const arquivo = await window.cripto.abrirTxt();
  if (!arquivo) {
    return;
  }
  if (arquivo.erro) {
    mostrarStatus(arquivo.erro, 'erro');
    return;
  }
  estado.arquivoXor = arquivo.caminho;
  $('nome-xor').textContent = arquivo.nome;
  $('limpar-xor').hidden = false;
  mostrarStatus(`Arquivo ${arquivo.nome} carregado.`, 'ok');
  analisarAutomaticamente();
});

function limparArquivoXor() {
  estado.arquivoXor = null;
  $('nome-xor').textContent = 'nenhum arquivo';
  $('limpar-xor').hidden = true;
}

$('limpar-xor').addEventListener('click', limparArquivoXor);

// ---------- execução dos algoritmos ----------
document.querySelectorAll('[data-algoritmo]').forEach((botao) => {
  botao.addEventListener('click', () => executar(botao.dataset.algoritmo));
});

// Ao enviar um .txt o usuário não precisa escolher nada: o motor testa todas as
// cifras (e as combinações de duas) e a tela mostra qual é a mais provável.
function analisarAutomaticamente() {
  document.querySelector('input[name="modo"][value="decifrar"]').checked = true;
  atualizarModo();
  return executar('auto');
}

async function executar(algoritmo) {
  const modo = modoAtual();
  const chave = $('chave').value.trim();
  const usarArquivo = (algoritmo === 'xor' || algoritmo === 'auto') && estado.arquivoXor !== null;
  const texto = textoEntrada();

  if (!usarArquivo && texto.length === 0) {
    mostrarStatus(
      algoritmo === 'xor' ? 'Adicione o arquivo .txt do XOR ou digite o texto.' : 'Digite ou carregue o texto.',
      'erro',
    );
    return;
  }

  definirOcupado(true);
  const inicio = performance.now();
  try {
    const resposta = await window.cripto.executar({
      algoritmo,
      modo,
      chave,
      texto: usarArquivo ? '' : texto,
      arquivo: usarArquivo ? estado.arquivoXor : null,
    });

    limparChave();
    if (!resposta.ok) {
      mostrarStatus(resposta.mensagem || 'Erro ao executar o motor em C.', 'erro');
      return;
    }

    const operacao = modo === 'cifrar' ? 'criptografado' : 'descriptografado';
    const origem = usarArquivo ? ` · arquivo ${$('nome-xor').textContent}` : '';
    const info = interpretarMensagem(resposta.mensagem);
    mostrarResultado(resposta.saida, `${NOMES[algoritmo]} · ${operacao}${origem}`);
    mostrarChave(info, Math.round(performance.now() - inicio));
    mostrarOpcoes(info.opcoes);
    mostrarStatus(info.avisos.join('\n') || 'Pronto.', info.avisos.length ? '' : 'ok');
  } finally {
    definirOcupado(false);
    atualizarModo();
  }
}

// O motor em C escreve no stderr linhas que começam com @ (cifras, chave,
// valores e outras possibilidades); o resto são avisos para o usuário.
function interpretarMensagem(mensagem) {
  const info = { cifras: '', chave: '', valores: [], opcoes: [], avisos: [] };

  (mensagem || '').split('\n').forEach((linha) => {
    if (linha.startsWith('@cifras ')) {
      info.cifras = linha.slice(8);
    } else if (linha.startsWith('@chave ')) {
      info.chave = linha.slice(7);
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

// Algoritmo ideal, chave e uma linha por byte/letra em decimal, hexadecimal e binário.
function mostrarChave(info, milissegundos) {
  const corpo = $('corpo-chave');
  corpo.replaceChildren();
  $('chave-encontrada').hidden = info.chave === '';
  if (info.chave === '') {
    return;
  }
  $('ideal-texto').textContent = info.cifras;
  $('ideal-tempo').textContent = milissegundos === undefined ? '' : `(analisado em ${milissegundos} ms)`;
  $('chave-texto').textContent = info.chave;
  $('cifras-texto').textContent = info.cifras;
  $('tabela-chave').hidden = info.valores.length === 0;
  info.valores.forEach((valor) => {
    const linha = document.createElement('tr');
    [valor.nome, valor.decimal, valor.hexa, valor.binario].forEach((texto) => linha.appendChild(celula(texto)));
    corpo.appendChild(linha);
  });
}

function mostrarOpcoes(opcoes) {
  const lista = $('lista-opcoes');
  lista.replaceChildren();
  $('detalhes-opcoes').hidden = opcoes.length === 0;
  opcoes.forEach((opcao) => {
    const item = document.createElement('li');
    const previa = document.createElement('span');
    item.appendChild(document.createTextNode(opcao.descricao));
    previa.className = 'previa';
    previa.textContent = opcao.previa;
    item.appendChild(previa);
    lista.appendChild(item);
  });
}

function limparChave() {
  mostrarChave({ chave: '', cifras: '', valores: [] });
  mostrarOpcoes([]);
}

function definirOcupado(ocupado) {
  document.querySelectorAll('[data-algoritmo]').forEach((botao) => {
    botao.disabled = ocupado;
  });
}

// Caracteres de controle (comuns no XOR) ficam visíveis só na tela;
// copiar e salvar usam o resultado original.
function textoVisivel(texto) {
  return texto.replace(/[\x00-\x09\x0B-\x1F\x7F-\x9F]/g, (c) => {
    const codigo = c.charCodeAt(0);
    if (codigo < 0x20) {
      return String.fromCharCode(0x2400 + codigo);
    }
    return codigo === 0x7f ? '␡' : '·';
  });
}

function mostrarResultado(texto, titulo) {
  estado.resultado = texto;
  $('resultado').textContent = textoVisivel(texto);
  $('titulo-resultado').textContent = titulo;
  ['usar', 'copiar', 'salvar'].forEach((id) => {
    $(id).disabled = texto.length === 0;
  });
}

function mostrarStatus(mensagem, tipo) {
  const status = $('status');
  status.textContent = mensagem;
  status.className = `status ${tipo}`.trim();
}

$('usar').addEventListener('click', () => {
  definirTexto(estado.resultado);
  limparArquivoXor();
  mostrarStatus('Resultado copiado para o campo de texto. Escolha o próximo algoritmo.', 'ok');
});

$('copiar').addEventListener('click', async () => {
  await window.cripto.copiar(estado.resultado);
  mostrarStatus('Resultado copiado para a área de transferência.', 'ok');
});

$('salvar').addEventListener('click', async () => {
  const resposta = await window.cripto.salvar(estado.resultado);
  if (resposta.erro) {
    mostrarStatus(resposta.erro, 'erro');
  } else if (resposta.salvo) {
    mostrarStatus(`Salvo em ${resposta.caminho}`, 'ok');
  }
});

// ---------- tela de códigos ----------
async function carregarCodigos() {
  if (estado.codigos === null) {
    estado.codigos = await window.cripto.codigos();
    const abas = $('abas-codigo');
    estado.codigos.forEach((item) => {
      const botao = document.createElement('button');
      botao.type = 'button';
      botao.className = 'aba';
      botao.dataset.codigo = item.id;
      botao.textContent = item.nome;
      botao.addEventListener('click', () => mostrarCodigo(item.id));
      abas.appendChild(botao);
    });
    mostrarCodigo(estado.codigos[0].id);
  }
}

function mostrarCodigo(id) {
  const item = estado.codigos.find((codigo) => codigo.id === id);
  document.querySelectorAll('[data-codigo]').forEach((botao) => {
    botao.classList.toggle('ativa', botao.dataset.codigo === id);
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

atualizarContador();
atualizarModo();
