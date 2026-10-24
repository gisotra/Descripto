// Prepara o motor em C. Chamado pelo "npm run electron".
//
// 1. Tenta compilar c/motor.c com o gcc (bin/motor, ou bin/motor.exe no Windows).
// 2. No Windows, sem gcc, usa o motor.exe pré-compilado de prebuilt/ -- mas só se ele
//    foi gerado a partir do código atual (confere o hash do código-fonte).
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const { hashDoFonte, arquivoHash } = require('./hash-fonte');

const raiz = path.join(__dirname, '..');
const pastaBin = path.join(raiz, 'bin');
const fonte = path.join(raiz, 'c', 'motor.c');
const windows = process.platform === 'win32';
const saida = path.join(pastaBin, windows ? 'motor.exe' : 'motor');
const precompilado = path.join(raiz, 'prebuilt', 'motor-windows-x64.exe');

function usarPrecompilado() {
  if (!fs.existsSync(precompilado) || !fs.existsSync(arquivoHash)) {
    return false;
  }
  const esperado = fs.readFileSync(arquivoHash, 'utf8').trim();
  if (esperado !== hashDoFonte()) {
    console.error(
      'O motor.exe pré-compilado é de uma versão antiga do c/motor.c. ' +
        'Peça a versão atualizada (git pull) ou instale o gcc.',
    );
    return false;
  }
  fs.copyFileSync(precompilado, saida);
  console.log('gcc não encontrado: usando o motor pré-compilado (prebuilt/motor-windows-x64.exe).');
  return true;
}

fs.mkdirSync(pastaBin, { recursive: true });

try {
  execFileSync('gcc', ['-O2', '-Wall', '-Wextra', '-std=gnu11', '-o', saida, fonte, '-lm'], {
    stdio: 'inherit',
  });
  console.log(`Motor em C compilado: ${path.relative(raiz, saida)}`);
} catch (erro) {
  if (erro.code === 'ENOENT') {
    if (windows && usarPrecompilado()) {
      process.exit(0);
    }
    console.error(
      windows
        ? 'gcc não encontrado. Instale o MinGW-w64 (ex.: pelo MSYS2) e coloque o gcc no PATH.'
        : 'gcc não encontrado. Instale com: sudo apt install build-essential',
    );
  } else {
    console.error('Falha ao compilar c/motor.c (veja as mensagens do gcc acima).');
  }
  process.exit(1);
}
