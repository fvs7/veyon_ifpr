# Guia de testes em bancada – Veyon IFPR (REA)

Este guia descreve como validar, numa máquina Windows de bancada, o instalador gerado pelo workflow
**Build Windows installer (IFPR)** (`.github/workflows/build-windows.yml`).

O Veyon IFPR é uma distribuição institucional do [Veyon](https://veyon.io), mantida pelo IFPR – Campus
União da Vitória como Recurso Educacional Aberto (REA), sob a licença **GNU GPL v2** (ver `COPYING`).

---

## 1. O que está sendo testado

| Item | O que mudou |
|---|---|
| Sobre (Master e Configurador) | Linha “Edição institucional” com a versão REA/IFPR |
| Plugin **InternetControl** | Botões **Bloquear Internet** / **Liberar Internet** no Veyon Master |
| Plugin **ExamMode** | Botões **Iniciar Modo Prova** / **Finalizar Prova** no Veyon Master |
| Padrões de autenticação | Método padrão = **arquivos de chave**; chaves públicas buscadas em `\\servidor-ifpr\veyon-keys` |

> **Instalador:** o Veyon gera instaladores **NSIS (`.exe`)**, não `.msi`. O `.exe` aceita instalação
> silenciosa (`/S`) e importação de configuração (`/ApplyConfig=arquivo.json`), o que cobre a
> implantação em massa nos laboratórios.

---

## 2. Pré-requisitos da bancada

- **PC-ALUNO**: Windows 10 ou 11 (x64), preferencialmente uma VM com snapshot, com acesso à Internet.
- **PC-PROFESSOR**: outro Windows (ou o mesmo PC para testes rápidos, ver observação na seção 6),
  na mesma rede local privada (10.x, 172.16–31.x ou 192.168.x).
- Firewall do Windows (**Windows Defender Firewall**) **ativo** no PC-ALUNO.
- Um pendrive para o teste de mídias removíveis.
- Acesso ao compartilhamento `\\servidor-ifpr\veyon-keys` (ou um caminho alternativo, ver 3.3).

---

## 3. Instalação

### 3.1 Baixar o instalador

1. No GitHub, abra **Actions → Build Windows installer (IFPR)** e escolha a execução mais recente com ✅.
2. Em **Artifacts**, baixe `veyon-ifpr-win64-installer` e descompacte.
3. Confira o hash: `Get-FileHash .\veyon-*-win64-setup.exe -Algorithm SHA256` deve bater com
   `veyon-installer.sha256`.

### 3.2 Instalar

- **Interativo:** execute `veyon-<versão>-win64-setup.exe` como administrador.
  - PC-PROFESSOR: marque o componente **Veyon Master**.
  - PC-ALUNO: o Master é opcional (pode ficar desmarcado).
- **Silencioso (implantação em massa):**

  ```bat
  veyon-<versão>-win64-setup.exe /S
  veyon-<versão>-win64-setup.exe /S /ApplyConfig=C:\caminho\veyon-ifpr.json
  ```

### 3.3 Chaves de autenticação

O padrão desta edição é **autenticação por arquivo de chave**. O serviço do aluno lê a chave pública em:

```
\\servidor-ifpr\veyon-keys\<nome-da-chave>\key
```

1. No PC-PROFESSOR (prompt como administrador):

   ```bat
   "C:\Program Files\Veyon\veyon-cli.exe" authkeys create professor
   "C:\Program Files\Veyon\veyon-cli.exe" authkeys export professor/public C:\temp\professor_public_key.pem
   ```

2. No servidor, copie o arquivo para `\\servidor-ifpr\veyon-keys\professor\key`
   (o nome do arquivo deve ser exatamente `key`, sem extensão).
3. **Permissões do compartilhamento:** o Serviço Veyon roda como **SYSTEM**, então acessa a rede com a
   **conta do computador** (`DOMINIO\PC$`). Conceda **leitura** ao grupo **Computadores do domínio**.
4. **Máquina fora do domínio / sem acesso ao servidor:** aponte para uma pasta local:

   ```bat
   "C:\Program Files\Veyon\veyon-cli.exe" config set Authentication/PublicKeyBaseDir "C:\ProgramData\Veyon\keys\public"
   "C:\Program Files\Veyon\veyon-cli.exe" authkeys import professor/public C:\temp\professor_public_key.pem
   ```

> ⚠️ Se o compartilhamento ficar indisponível, o professor **não consegue se conectar** aos alunos.
> Para produção, avalie copiar as chaves localmente via GPO e manter o compartilhamento só como origem.

### 3.4 Conferências pós-instalação

- [ ] `services.msc` → **Veyon Service** em execução.
- [ ] Veyon Configurador → **Autenticação**: método = *Autenticação por arquivo de chave*;
      diretório de chaves públicas = `\\servidor-ifpr\veyon-keys`.
- [ ] `veyon-cli.exe authkeys list` no PC-ALUNO mostra a chave `professor/public`.
- [ ] Do PC-PROFESSOR, o Veyon Master mostra a tela do PC-ALUNO (adicione o computador em
      Configurador → Locais e computadores).

---

## 4. Teste 1 – Identidade REA (menu Sobre)

1. Veyon Master → menu/botão **Sobre**.
2. Veyon Configurador → **Ajuda → Sobre**.

**Esperado:** em ambos aparece a linha **“Edição institucional: IFPR REA - Campus Uniao da Vitoria 1.0.0”**
e o texto sobre a distribuição como REA sob GNU GPL v2.

---

## 5. Teste 2 – Controle de Internet

### 5.1 Bloquear

1. No PC-ALUNO, abra o navegador em `https://www.ifpr.edu.br` (deve carregar).
2. No Master, selecione o PC-ALUNO e clique em **Bloquear Internet**.

**Esperado no PC-ALUNO:**
- [ ] Sites HTTP e HTTPS **não carregam** (inclusive em abas novas e em outro navegador).
- [ ] A conexão do Veyon **continua** (a miniatura no Master segue atualizando; acesso remoto funciona).
- [ ] Recursos da rede local continuam acessíveis: `ping <gateway>`, pastas compartilhadas,
      sites internos em 10.x/172.16–31.x/192.168.x.
- [ ] Regras criadas (PowerShell como administrador):

  ```powershell
  Get-NetFirewallRule -Group "Veyon IFPR" | Format-Table DisplayName, Direction, Action, Enabled
  Get-NetFirewallRule -Group "Veyon IFPR" | Get-NetFirewallPortFilter
  Get-NetFirewallRule -Group "Veyon IFPR" | Get-NetFirewallAddressFilter
  ```

  Duas regras de **saída/bloqueio**: TCP 80,443 e UDP 443 (QUIC), somente para endereços públicos.
  Nenhuma regra menciona a porta 11100.

### 5.2 Liberar

1. No Master, clique em **Liberar Internet**.

**Esperado:**
- [ ] Navegação volta a funcionar (pode levar alguns segundos; reabra a aba se necessário).
- [ ] `Get-NetFirewallRule -Group "Veyon IFPR"` não retorna nada.

### 5.3 Persistência e casos de borda

- [ ] Bloqueie, **reinicie** o PC-ALUNO: após o boot a Internet continua bloqueada e o Master consegue
      reconectar e **Liberar Internet**.
- [ ] Clicar **Bloquear Internet** duas vezes não duplica regras.
- [ ] O PC do professor **nunca** é bloqueado, mesmo se estiver na lista da sala.

---

## 6. Teste 3 – Modo Prova

### 6.1 Iniciar

1. Conecte um pendrive no PC-ALUNO e confirme que ele abre. **Remova-o.**
2. No Master, selecione o PC-ALUNO e clique em **Iniciar Modo Prova**.

**Esperado no PC-ALUNO:**
- [ ] Faixa laranja discreta no topo de cada monitor: **“MODO EXAME / PROVA • Internet e pendrives
      bloqueados pelo professor”**. Ela não captura cliques nem foco.
- [ ] Internet bloqueada (mesmas verificações da seção 5.1).
- [ ] Pendrive conectado **agora** não monta / não aparece no Explorador.
- [ ] Registro:

  ```bat
  reg query HKLM\SYSTEM\CurrentControlSet\Services\USBSTOR /v Start
  reg query "HKLM\SOFTWARE\Veyon IFPR\ExamMode" /v UsbStorOriginalStart
  ```

  `Start` = `0x4`; `UsbStorOriginalStart` = valor anterior (normalmente `0x3`).

### 6.2 Finalizar

1. No Master, clique em **Finalizar Prova**.

**Esperado:**
- [ ] A faixa some.
- [ ] Internet liberada (regras do grupo “Veyon IFPR” removidas).
- [ ] `USBSTOR\Start` volta ao valor original (normalmente `0x3`) e `UsbStorOriginalStart` é apagado.
- [ ] Pendrive conectado depois disso volta a funcionar (remova e reconecte).

### 6.3 Robustez

- [ ] Inicie o Modo Prova, **reinicie** o PC-ALUNO e faça login: restrições continuam ativas e a faixa
      **reaparece sozinha** em até ~15 s após o Master se conectar.
- [ ] Faça logoff/login durante a prova: a faixa reaparece.
- [ ] Com dois monitores, a faixa aparece nos dois.
- [ ] Clicar **Iniciar Modo Prova** duas vezes e depois **Finalizar Prova** restaura o valor original
      (não fica preso em 4).
- [ ] Em um PC onde o USB já estava desativado pelo administrador (`Start = 4`), Iniciar + Finalizar
      **mantém 4**.

> Observação: o PC onde o Master está rodando é sempre excluído do bloqueio e do Modo Prova.
> Para testar com uma única máquina, use duas VMs.

---

## 7. Limitações conhecidas (para registrar nos testes)

- **Pendrive já montado** antes de iniciar a prova continua acessível até ser removido
  (a chave `USBSTOR` só impede o carregamento do driver em novas conexões).
- O bloqueio é por **porta de destino (80/443)**. Não bloqueia VPNs ou proxies em outras portas.
  Se o campus usar **proxy HTTP na rede local** (ex.: 10.x.x.x:3128), a navegação via proxy
  **continua funcionando**, pois a rede local nunca é bloqueada.
- Se uma GPO desativar o Windows Defender Firewall ou impedir a mesclagem de regras locais
  (“Aplicar regras de firewall local = Não”), o bloqueio **não terá efeito**. O log do Veyon registra
  um aviso quando o firewall está desativado no perfil ativo.
- Bloqueio de Internet, USB e faixa da prova estão implementados **somente no Windows**.

---

## 8. Reversão manual (emergência)

Se algo falhar e o professor perder o controle, num prompt de administrador no PC-ALUNO:

```bat
powershell -Command "Remove-NetFirewallRule -Group 'Veyon IFPR'"
reg add HKLM\SYSTEM\CurrentControlSet\Services\USBSTOR /v Start /t REG_DWORD /d 3 /f
reg delete "HKLM\SOFTWARE\Veyon IFPR\ExamMode" /v UsbStorOriginalStart /f
taskkill /F /IM veyon-worker.exe
```

---

## 9. Logs e coleta de evidências

- Ative o nível de log **Depuração** em Configurador → Geral → Registro (log) e reinicie o serviço.
- Os logs do serviço/servidor ficam em `C:\Windows\Temp\` (`VeyonService.log`, `VeyonServer-<sessão>.log`,
  `VeyonWorker-<sessão>.log`), pois rodam como SYSTEM.
- Mensagens relevantes: `Internet access blocked`, `USB mass storage disabled`,
  `finishing exam mode`, `Windows Defender Firewall is DISABLED`.

Registre para cada teste: versão do instalador (hash), versão do Windows, resultado (OK/Falha) e prints.
