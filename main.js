// Processo principal do Electron: abre a janela e executa o motor em C.
const { app, BrowserWindow, ipcMain, dialog, clipboard } = require('electron');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const MOTOR = path.join(__dirname, 'bin', 'motor');
const ALGORITMOS = ['cesar', 'vigenere', 'atbash', 'xor', 'auto'];
const MODOS = ['cifrar', 'decifrar'];
const TEMPO_LIMITE_MS = 15000;

const CODIGOS = [
  { id: 'cesar', nome: 'César', arquivo: 'algoritmos/caesar.c', original: true },
  { id: 'vigenere', nome: 'Vigenère', arquivo: 'algoritmos/vigenere.c', original: true },
  { id: 'atbash', nome: 'Atbash', arquivo: 'algoritmos/atbash.c', original: true },
  { id: 'xor', nome: 'XOR', arquivo: 'algoritmos/xor.c', original: true },
  { id: 'motor', nome: 'motor.c (usado pela interface)', arquivo: 'c/motor.c', original: false },
];

// Só arquivos escolhidos pelo usuário na janela de seleção podem ser lidos.
const arquivosPermitidos = new Set();
let janela = null;

function criarJanela() {
  janela = new BrowserWindow({
    width: 1000,
    height: 800,
    minWidth: 720,
    minHeight: 560,
    title: 'Torneio de Criptografia',
    autoHideMenuBar: true,
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
    const processo = spawn(MOTOR, args, { timeout: TEMPO_LIMITE_MS });

    processo.stdout.on('data', (dados) => saida.push(dados));
    processo.stderr.on('data', (dados) => erros.push(dados));
    processo.stdin.on('error', () => {}); // o motor pode terminar antes de ler tudo
    processo.on('error', (erro) => {
      resolve({ ok: false, saida: '', mensagem: `Não foi possível executar o motor em C: ${erro.message}` });
    });
    processo.on('close', (codigo, sinal) => {
      const mensagem = Buffer.concat(erros).toString('utf8').trim();
      if (sinal) {
        resolve({ ok: false, saida: '', mensagem: 'O motor em C demorou demais e foi interrompido.' });
        return;
      }
      resolve({ ok: codigo === 0, saida: Buffer.concat(saida).toString('utf8'), mensagem });
    });

    processo.stdin.end(entrada);
  });
}

ipcMain.handle('cripto:executar', async (_evento, pedido) => {
  const { algoritmo, modo, chave = '', texto = '', arquivo = null } = pedido || {};

  if (!ALGORITMOS.includes(algoritmo) || !MODOS.includes(modo)) {
    return { ok: false, saida: '', mensagem: 'Pedido inválido.' };
  }
  if (!fs.existsSync(MOTOR)) {
    return { ok: false, saida: '', mensagem: 'O motor em C não foi compilado. Rode: npm run build' };
  }

  let entrada;
  if (arquivo) {
    if (!arquivosPermitidos.has(arquivo)) {
      return { ok: false, saida: '', mensagem: 'Selecione o arquivo novamente.' };
    }
    try {
      entrada = await fs.promises.readFile(arquivo); // bytes crus, arquivo inteiro
    } catch (erro) {
      return { ok: false, saida: '', mensagem: `Não foi possível ler o arquivo: ${erro.message}` };
    }
  } else {
    entrada = Buffer.from(String(texto), 'utf8');
  }

  return executarMotor([algoritmo, modo, String(chave)], entrada);
});

ipcMain.handle('cripto:abrir-txt', async () => {
  const escolha = await dialog.showOpenDialog(janela, {
    title: 'Adicionar arquivo .txt',
    properties: ['openFile'],
    filters: [
      { name: 'Arquivos de texto', extensions: ['txt'] },
      { name: 'Todos os arquivos', extensions: ['*'] },
    ],
  });
  if (escolha.canceled || escolha.filePaths.length === 0) {
    return null;
  }

  const caminho = escolha.filePaths[0];
  try {
    const conteudo = await fs.promises.readFile(caminho, 'utf8');
    arquivosPermitidos.add(caminho);
    return { caminho, nome: path.basename(caminho), conteudo };
  } catch (erro) {
    return { erro: `Não foi possível ler o arquivo: ${erro.message}` };
  }
});

ipcMain.handle('cripto:salvar', async (_evento, conteudo) => {
  const escolha = await dialog.showSaveDialog(janela, {
    title: 'Salvar resultado',
    defaultPath: 'resultado.txt',
    filters: [{ name: 'Arquivos de texto', extensions: ['txt'] }],
  });
  if (escolha.canceled || !escolha.filePath) {
    return { salvo: false };
  }

  try {
    await fs.promises.writeFile(escolha.filePath, String(conteudo), 'utf8');
    return { salvo: true, caminho: escolha.filePath };
  } catch (erro) {
    return { salvo: false, erro: `Não foi possível salvar: ${erro.message}` };
  }
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
