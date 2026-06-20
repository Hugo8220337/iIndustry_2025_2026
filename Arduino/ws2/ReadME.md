# WS2 Armazenamento — Robot de Navegacao Autonoma

## Visao Geral

Este sistema implementa um robot movel autonomo responsavel pela **Workstation 2 (Armazenamento)**. O robot navega entre workstations seguindo uma linha no chao, deteta e contorna obstaculos com um sensor LiDAR, e comunica o seu estado em tempo real via MQTT para o sistema central.

O movimento e ativado pela Workstation 1 quando termina todas as suas work orders, enviando um sinal via MQTT para o Arduino, que por sua vez envia o comando `CMD:START` ao EV3.

---

## Arquitetura do Sistema

```
WS1 termina work orders
        |
        v
   Node-RED / MQTT
        |
        v
  Arduino Giga R1
  (subscreve MQTT, envia CMD:START via Serial)
  - RPLIDAR A1M8: deteta obstaculos
        |
        v  Serial UART (in3)
      EV3 (ev3dev)
      - Color Sensor (in2): segue linha azul
        |
        v  Serial UART (in3)
  Arduino Giga R1
  (recebe metricas, publica MQTT)
        |
        v
   Node-RED / MQTT
        |
        v
     CrateDB
  (armazena metricas)
```

---

## Hardware

| Componente | Porta / Ligacao | Funcao |
|---|---|---|
| Motor direito | OUTPUT_B | Tracao direita |
| Motor esquerdo | OUTPUT_A | Tracao esquerda |
| Color Sensor | INPUT_2 | Seguimento de linha e deteção de workstations |
| RPLIDAR A1M8 | INPUT_3 (UART) | Deteção de obstaculos |
| Arduino Giga R1 | Serial (in3) | Bridge MQTT ↔ EV3 |

---

## Marcadores no Chao

| Cor | Significado |
|---|---|
| Azul | Linha de navegacao principal |
| Amarelo | WS1 Pre-Expansao (ponto de partida / retorno) |
| Vermelho | WS2 Armazenamento (destino final) |

---

## Funcionamento

### 1. Arranque

O robot aguarda em standby ate receber um dos seguintes sinais:

- `<CMD:START>` via Serial (enviado pelo Arduino quando WS1 termina work orders)
- Reflexo >= 50 no color sensor (arranque fisico manual para testes)

Quando recebe o sinal, avanca 0.5 segundos para sair da marca de arranque e inicia o modo de navegacao.

### 2. Seguimento de Linha (PID)

O color sensor le a reflexao do chao em modo `COL-REFLECT`. O robot mantem-se na fronteira entre a linha azul e o chao usando um controlador PID:

```
Valor azul:    ~15 (reflexao baixa)
Valor chao:    ~35 (reflexao alta)
Target (PID):  25  (fronteira entre os dois)
Limite desvio: 30  (acima disto = linha perdida)
```

O controlador calcula a correcao a aplicar aos dois motores para manter o sensor no valor target, permitindo seguir curvas e mudancas de direcao na linha.

### 3. Busca Ativa (Linha Perdida)

Quando o sensor perde a linha (reflexao > 30), o robot inicia uma busca agressiva:

- **Fase 1 (0-0.8s):** Roda para o lado onde perdeu a linha
- **Fase 2 (0.8-2.5s):** Roda para o lado oposto
- **Fase 3 (>2.5s):** Avanca em frente
- **Reset (>3.0s):** Reinicia o ciclo de busca

### 4. Deteção de Obstaculos (RPLIDAR)

Os pontos sao filtrados por:

- Distancia valida: 50mm a 4000mm (elimina ruido do sensor)
- Zona frontal: angulos 330° a 30° (60° de abertura a frente)
- Distancia de perigo: < 280mm

Se 3 ou mais pontos de perigo forem detetados na zona frontal, o protocolo de contornacao e ativado.


### 5. Chegada a Workstations

| Cor detetada | Acao |
|---|---|
| Vermelho (reflexao >= 43) | Para 2 segundos, envia metrica WS2_CHEGADA, inverte 180° e retorna |
| Amarelo (reflexao >= 50, apos inversao) | Para, envia metrica END, estaciona e aguarda proximo START |

---

## Metricas Enviadas

O robot envia metricas continuamente para o Arduino no formato:

```
<MET,estado,vel_esq,vel_dir,obstaculo,ws>
```

### Tabela de Estados

| Codigo | Estado | Significado |
|---|---|---|
| 0 | PARADO | Robot parado, aguarda CMD:START |
| 1 | START | A seguir linha, em movimento |
| 2 | DETETADO | Obstaculo detetado, a virar |
| 3 | CONTORNAR | A desviar do obstaculo |
| 4 | NOLINE | Linha perdida, a procurar |
| 5 | FOUNDLINE | Linha encontrada apos contorno |
| 6 | WS_CHEGADA | Chegou a WS2_Armazenamento |
| 7 | END | Percurso completo, estacionado |

### Exemplo de Mensagens

```
<MET,0,0,0,0,nenhuma>              -> Standby
<MET,1,18,12,0,nenhuma>            -> A seguir linha em curva
<MET,2,0,0,1,nenhuma>              -> Obstaculo detetado
<MET,3,15,15,1,nenhuma>            -> A contornar
<MET,6,0,0,0,2(WS2_Armazenamento)>   -> Chegou a WS2
<MET,7,0,0,0,1(WS1_Pre-expansao)>    -> Percurso completo
```

O Arduino recebe estas metricas e publica no topico MQTT `robot/metrics`. O Node-RED subscreve este topico e armazena os dados no CrateDB.

---

## Estrutura de Ficheiros

```
WS2_Lidar/
└── WS2Nav.py          Script principal do robot EV3
```

---

## Como Executar

```bash
# Via SSH no EV3
ssh robot@ev3dev.local

# Executar o script
python3 /home/robot/WS2_Lidar/WS2Nav.py
```

O robot aguarda `CMD:START` ou reflexo >= 50 para iniciar.

---

## Dependencias

- `ev3dev2` — biblioteca de controlo do EV3
- `pyserial` — comunicacao UART com o Arduino
- Python 3 (ev3dev)
