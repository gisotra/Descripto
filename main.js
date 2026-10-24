// Processo principal do Electron: abre a janela, navega nas pastas e executa o motor em C.
const { app, BrowserWindow, Menu, ipcMain, clipboard } = require('electron');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const MOTOR = path.join(__dirname, 'bin', process.platform === 'win32' ? 'motor.exe' : 'motor');
const ALGORITMOS = ['cesar', 'vigenere', 'atbash', 'xor', 'auto'];
const MODOS = ['cifrar', 'decifrar'];
const TEMPO_LIMITE_MS = 15000;
const TAMANHO_MAXIMO = 5 * 1024 * 1024;

const CODIGOS = [
  { id: 'cesar', nome: 'César', arquivo: 'algoritmos/caesar.c', original: true },
  { id: 'vigenere', nome: 'Vigenère', arquivo: 'algoritmos/vigenere.c', original: true },
  { id: 'atbash', nome: 'Atbash', arquivo: 'algoritmos/atbash.c', original: true },
  { id: 'xor', nome: 'XOR', arquivo: 'algoritmos/xor.c', original: true },
  { id: 'motor', nome: 'motor.c (usado pela interface)', arquivo: 'c/motor.c', original: false },
];

let janela = null;

function criarJanela() {
  janela = new BrowserWindow({
    width: 1100,
    height: 860,
    minWidth: 760,
    minHeight: 600,
    title: 'Torneio de Criptografia',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });

  janela.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
  janela.webContents.on('will-navigate', (evento) => evento.preventDefault());
  janela.loadFile(path.join(__dirname, 'src', 'index.html'));
}

function executarMotor(args, entrada) {
  return new Promise((resolve) => {
    const saida = [];
    const erros = [];
    const processo = spawn(MOTOR, args, { timeout: TEMPO_LIMITE_MS, windowsHide: true });

    processo.stdout.on('data', (dados) => saida.push(dados));
    processo.stderr.on('data', (dados) => erros.push(dados));
    processo.stdin.on('error', () => {}); // o motor pode terminar antes de ler tudo
    processo.on('error', (erro) => {
      resolve({ ok: false, saida: '', mensagem: `Não foi possível executar o motor em C: ${erro.message}` });
    });
    processo.on('close', (codigo, sinal) => {
      const mensagem = Buffer.concat(erros).toString('utf8').replace(/\r/g, '').trim();
      if (sinal) {
        resolve({ ok: false, saida: '', mensagem: 'O motor em C demorou demais e foi interrompido.' });
        return;
      }
      resolve({ ok: codigo === 0, saida: Buffer.concat(saida).toString('utf8'), mensagem });
    });

    processo.stdin.end(entrada);
  });
}

// Pasta Downloads do usuário (no Mint pode se chamar "Transferências"). Em sistemas sem as pastas
// do usuário configuradas o Electron devolve a pasta pessoal; nesse caso usa ~/Downloads se existir.
function pastaDownloads() {
  const casa = path.resolve(app.getPath('home'));
  const informada = app.getPath('downloads');
  if (path.resolve(informada) !== casa) {
    return informada;
  }
  const padrao = path.join(casa, 'Downloads');
  return fs.existsSync(padrao) ? padrao : informada;
}

function motorDisponivel() {
  return fs.existsSync(MOTOR);
}

// Texto digitado ou colado (modo manual e etapas de criptografia).
ipcMain.handle('cripto:executar', async (_evento, pedido) => {
  const { algoritmo, modo, chave = '', texto = '' } = pedido || {};

  if (!ALGORITMOS.includes(algoritmo) || !MODOS.includes(modo)) {
    return { ok: false, saida: '', mensagem: 'Pedido inválido.' };
  }
  if (!motorDisponivel()) {
    return { ok: false, saida: '', mensagem: 'O motor em C não foi compilado. Rode: npm run build' };
  }
  return executarMotor([algoritmo, modo, String(chave)], Buffer.from(String(texto), 'utf8'));
});

// Arquivo .txt escolhido no navegador: lê os bytes inteiros e roda o modo automático.
ipcMain.handle('cripto:analisar-arquivo', async (_evento, caminho) => {
  const inicio = process.hrtime.bigint();

  if (typeof caminho !== 'string' || path.extname(caminho).toLowerCase() !== '.txt') {
    return { ok: false, saida: '', mensagem: 'Escolha um arquivo .txt.' };
  }
  if (!motorDisponivel()) {
    return { ok: false, saida: '', mensagem: 'O motor em C não foi compilado. Rode: npm run build' };
  }

  let bytes;
  try {
    const info = await fs.promises.stat(caminho);
    if (!info.isFile()) {
      return { ok: false, saida: '', mensagem: 'Isso não é um arquivo.' };
    }
    if (info.size > TAMANHO_MAXIMO) {
      return { ok: false, saida: '', mensagem: 'O arquivo é grande demais (máximo 5 MB).' };
    }
    bytes = await fs.promises.readFile(caminho); // bytes crus: o arquivo é lido por inteiro
  } catch (erro) {
    return { ok: false, saida: '', mensagem: `Não foi possível ler o arquivo: ${erro.message}` };
  }

  const resposta = await executarMotor(['auto', 'decifrar', ''], bytes);
  const ms = Number((process.hrtime.bigint() - inicio) / 1000000n);
  return { ...resposta, nome: path.basename(caminho), tamanho: bytes.length, ms };
});

// ---------- navegador de pastas (setas do teclado) ----------
async function existe(caminho) {
  try {
    await fs.promises.access(caminho);
    return true;
  } catch {
    return false;
  }
}

async function listarLugares() {
  const candidatos = [
    ['Downloads', pastaDownloads()],
    ['Área de trabalho', app.getPath('desktop')],
    ['Documentos', app.getPath('documents')],
    ['Pasta pessoal', app.getPath('home')],
  ];
  if (process.platform === 'win32') {
    for (let codigo = 67; codigo <= 90; codigo++) {   // de C: a Z: (A: e B: são disquetes)
      const letra = String.fromCharCode(codigo);
      candidatos.push([`Disco ${letra}:`, `${letra}:\\`]);
    }
  } else {
    candidatos.push(['Sistema de arquivos (/)', '/']);
  }

  const existentes = await Promise.all(candidatos.map(([, caminho]) => existe(caminho)));
  return candidatos
    .filter((_, i) => existentes[i])
    .map(([nome, caminho]) => ({ nome, caminho, tipo: 'pasta' }));
}

const comparar = new Intl.Collator('pt-BR', { sensitivity: 'base', numeric: true }).compare;

async function listarPasta(pedido) {
  if (!pedido) {
    return { caminho: '', titulo: 'Lugares', pai: null, entradas: await listarLugares() };
  }

  const dir = path.resolve(String(pedido));
  let itens;
  try {
    itens = await fs.promises.readdir(dir, { withFileTypes: true });
  } catch (erro) {
    return { erro: `Não foi possível abrir a pasta: ${erro.message}` };
  }

  const entradas = [];
  await Promise.all(
    itens.map(async (item) => {
      if (item.name.startsWith('.')) {
        return;
      }
      const completo = path.join(dir, item.name);
      try {
        let info = null;
        if (item.isSymbolicLink()) {
          info = await fs.promises.stat(completo);
        }
        if (item.isDirectory() || (info && info.isDirectory())) {
          entradas.push({ nome: item.name, caminho: completo, tipo: 'pasta' });
        } else if ((item.isFile() || (info && info.isFile())) && /\.txt$/i.test(item.name)) {
          info = info || (await fs.promises.stat(completo));
          entradas.push({
            nome: item.name,
            caminho: completo,
            tipo: 'arquivo',
            tamanho: info.size,
            modificado: info.mtimeMs,
          });
        }
      } catch {
        // sem permissão ou link quebrado: ignora o item
      }
    }),
  );

  // Pastas em ordem alfabética; arquivos .txt do mais recente para o mais antigo.
  entradas.sort((a, b) => {
    if (a.tipo !== b.tipo) {
      return a.tipo === 'pasta' ? -1 : 1;
    }
    return a.tipo === 'pasta' ? comparar(a.nome, b.nome) : b.modificado - a.modificado;
  });

  const raiz = path.parse(dir).root;
  return { caminho: dir, titulo: dir, pai: dir === raiz ? '' : path.dirname(dir), entradas };
}

ipcMain.handle('fs:listar', (_evento, caminho) => listarPasta(caminho));

ipcMain.handle('fs:downloads', () => pastaDownloads());

// Salva direto na pasta Downloads (sem janela de diálogo), sem sobrescrever nada.
ipcMain.handle('cripto:salvar-downloads', async (_evento, nome, conteudo) => {
  const limpo = path.basename(String(nome)).replace(/[^\w.\-]+/g, '_') || 'resultado.txt';
  const { name, ext } = path.parse(limpo);

  for (let tentativa = 1; tentativa < 1000; tentativa++) {
    const arquivo = path.join(pastaDownloads(), tentativa === 1 ? limpo : `${name}-${tentativa}${ext}`);
    try {
      await fs.promises.writeFile(arquivo, String(conteudo), { encoding: 'utf8', flag: 'wx' });
      return { salvo: true, caminho: arquivo };
    } catch (erro) {
      if (erro.code !== 'EEXIST') {
        return { salvo: false, erro: `Não foi possível salvar: ${erro.message}` };
      }
    }
  }
  return { salvo: false, erro: 'Já existem arquivos demais com esse nome.' };
});

ipcMain.handle('cripto:copiar', (_evento, conteudo) => {
  clipboard.writeText(String(conteudo));
  return true;
});

ipcMain.handle('cripto:codigos', () =>
  Promise.all(
    CODIGOS.map(async (item) => {
      try {
        const codigo = await fs.promises.readFile(path.join(__dirname, item.arquivo), 'utf8');
        return { ...item, codigo };
      } catch (erro) {
        return { ...item, codigo: `// Não foi possível ler ${item.arquivo}: ${erro.message}` };
      }
    }),
  ),
);

app.whenReady().then(() => {
  Menu.setApplicationMenu(null); // sem barra de menus: a tecla Alt não interfere nos atalhos
  criarJanela();
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      criarJanela();
    }
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
