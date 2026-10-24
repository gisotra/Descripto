// Ponte segura entre a tela (renderer) e o processo principal.
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('cripto', {
  executar: (pedido) => ipcRenderer.invoke('cripto:executar', pedido),
  analisarArquivo: (caminho) => ipcRenderer.invoke('cripto:analisar-arquivo', caminho),
  listar: (caminho) => ipcRenderer.invoke('fs:listar', caminho),
  pastaDownloads: () => ipcRenderer.invoke('fs:downloads'),
  salvarDownloads: (nome, conteudo) => ipcRenderer.invoke('cripto:salvar-downloads', nome, conteudo),
  copiar: (conteudo) => ipcRenderer.invoke('cripto:copiar', conteudo),
  codigos: () => ipcRenderer.invoke('cripto:codigos'),
});
