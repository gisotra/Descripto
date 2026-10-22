// Roda antes do "npm run electron": instala as dependencias se ainda nao foram instaladas
// (por exemplo, logo depois de clonar o repositorio).
const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const raiz = path.join(__dirname, '..');

if (!fs.existsSync(path.join(raiz, 'node_modules', 'electron', 'path.txt'))) {
  console.log('Instalando as dependências (só na primeira vez)...');
  execSync('npm install', { cwd: raiz, stdio: 'inherit' });
}
