# Lab7 - NIOS II Web Server com Ethernet (DE2)

Processador de strings via hardware customizado (User_HW) na placa Altera DE2.
O NIOS II roda um web server que recebe strings via HTTP, processa no User_HW (char+1, 0xF7->0x00)
e retorna o resultado.

## Requisitos

- Altera Quartus II 13.0sp1 (com NIOS II EDS)
- Placa Altera DE2 (Cyclone II) com cabo USB-Blaster
- Cabo Ethernet + roteador/modem com DHCP ativo
- PC na mesma rede LAN do roteador

## Estrutura do projeto

```
lab7/
  ethernet.qpf              # Projeto Quartus
  ethernet.qsf              # Pin assignments
  EthernetExample.vhd       # Top-level VHDL (NIOS + DM9000A + User_HW)
  EthernetExample.qsf       # Pin assignments (alternativo)
  nios_system.qsys          # QSYS: NIOS II + SDRAM + DM9000A + Timer + User_HW
  ip/
    DM9000A_IF/              # Driver Ethernet DM9000A (IP core)
    user_hw/                 # Hardware customizado (processa strings)
  software/
    lab7_sw/                 # Codigo C do NIOS II (web server)
      web_server.c           # Servidor HTTP com RX/TX threads
      web_server.h           # Configuracao de IP e prioridades
      network_utilities.c    # Config de rede (MAC, IP, DHCP)
      dm9000a.c/h            # Driver do chip Ethernet DM9000A
  index.html                 # Cliente HTML alternativo (abre no PC)
```

## Passo a passo para rodar

### 1. Gerar o sistema NIOS (QSYS)

1. Abrir Quartus II
2. **Tools > Qsys** (ou abrir `nios_system.qsys` direto)
3. Clicar **Generate HDL** (deixar VHDL selecionado)
4. Esperar gerar sem erros
5. Fechar Qsys

### 2. Compilar o projeto Quartus (gerar o .sof)

1. **File > Open Project** > selecionar `ethernet.qpf`
2. **Processing > Start Compilation** (Ctrl+L)
3. Esperar compilar (pode demorar uns minutos)
4. Verificar que nao ha erros (warnings sao normais)
5. O arquivo `output_files/ethernet.sof` sera gerado

### 3. Programar o FPGA

1. Conectar o cabo USB-Blaster na DE2 e ligar a placa
2. **Tools > Programmer**
3. Adicionar `output_files/ethernet.sof`
4. Clicar **Start**
5. Esperar "Successful"

### 4. Criar e compilar o BSP (primeira vez)

Abrir o **NIOS II Command Shell** e rodar:

```bash
cd <caminho-do-lab7>/software

# Criar o BSP (ucosii com NicheStack e DHCP)
nios2-bsp ucosii lab7_sw_bsp ../../nios_system.sopcinfo \
  --set hal.sys_clk_timer timer_0 \
  --set hal.enable_lightweight_device_driver_api false \
  --set hal.max_file_descriptors 32 \
  --default_sections_mapping sdram_0

# Compilar o BSP
cd lab7_sw_bsp
make
```

**Se o BSP ja existe** (clonado do repo), basta:
```bash
cd software/lab7_sw_bsp
make
```

### 5. Compilar o software do NIOS II

```bash
cd software/lab7_sw
make
```

### 6. Fazer download do software para a placa

```bash
nios2-download -g software/lab7_sw/lab7_sw.elf
```

### 7. Abrir o terminal para ver o IP

Em outro terminal NIOS II Command Shell:

```bash
nios2-terminal
```

Deve aparecer algo como:
```
========================================
  IP via DHCP: 192.168.1.XXX
  Open http://192.168.1.XXX/ in browser
========================================
Web Server started with RX + TX threads.
```

**Importante:** use dois terminais separados - um para o download, outro para o nios2-terminal.

### 8. Testar no navegador

Abrir `http://<IP_DA_PLACA>/` no navegador do PC (que deve estar na mesma rede).

A pagina permite digitar uma string, enviar para a placa, e ver o resultado processado pelo User_HW.

**Alternativa:** abrir o arquivo `index.html` localmente no navegador e colocar o IP da placa no campo de configuracao.

## Troubleshooting

- **DHCP nao responde / IP 0.0.0.0:** verificar se o cabo Ethernet esta conectado da DE2 ao roteador (nao direto no PC). Verificar LEDs na porta Ethernet da DE2.
- **Compilacao Quartus falha:** verificar se o QSYS foi gerado antes (passo 1).
- **nios2-download falha:** verificar se o .sof foi programado antes (passo 3). A DE2 perde o .sof toda vez que desliga.
- **Browser pede senha:** tentar em aba anonima ou outro navegador.
- **Apos desligar/ligar a DE2:** repetir passos 3, 6 e 7 (programar .sof, download .elf, abrir terminal).
