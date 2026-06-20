# Arquitetura Sparkplug B — Projeto IIoT EPS

## 1. Objetivo

Este documento define uma proposta simples e normalizada para a comunicação MQTT/Sparkplug B entre o Node-RED e os elementos do chão de fábrica do projeto IIoT.

O objetivo é usar Sparkplug B para:

- Enviar ordens de trabalho do Odoo/MRP para as workstations.
- Registar início, pausa e fim das ordens de trabalho.
- Registar defeitos e resultados de controlo de qualidade.
- Registar o estado dos equipamentos.
- Enviar dados para CrateDB e Grafana.
- Permitir integração OT-IT através do Node-RED.

---

## 2. Componentes principais

| Componente | Função |
|---|---|
| Odoo / MRP | Gestão de produtos, BOM, workcenters, operações e ordens de trabalho |
| Node-RED | Edge Gateway, Integration Gateway e orquestrador dos fluxos |
| Broker MQTT | Comunicação publish/subscribe usando Sparkplug B |
| CrateDB | Base de dados de séries temporais para eventos do chão de fábrica |
| Grafana | Dashboard para visualizar produção, estados e OEE |
| Arduinos das workstations | Controlam o HMI e comunicam com Node-RED por MQTT/Sparkplug B |
| Robot LiDAR | Workstation de armazenamento intermédio/estabilização |

---

## 3. Workstations consideradas

De acordo com o enunciado, o processo produtivo tem quatro WorkCenters principais:

| WorkCenter | Operação | Device ID Sparkplug B proposto |
|---|---|---|
| Pré-expansão | Aquecimento | `WS1_PreExpansao` |
| Armazenamento intermédio / Estabilização | Arrefecimento e secagem, transporte para armazém e regresso | `WS2_Armazenamento` |
| Modelagem | Moldar e montar | `WS3_Moldagem` |
| Controlo de qualidade | Verificação de defeitos | `WS4_Qualidade` |

Nota: a câmara de qualidade e os sensores associados são considerados parte da respetiva workstation. Assim, a câmara Nicla Vision não é modelada como um device Sparkplug B separado. O mesmo raciocínio é aplicado ao robot LiDAR: ele representa a workstation de armazenamento intermédio/estabilização.

---

## 4. Identificadores Sparkplug B

```text
Group ID: ESTG_Fabrica
Edge Node ID: node_red_edge
```

Formato geral dos tópicos:

```text
spBv1.0/{GroupID}/{MessageType}/{EdgeNodeID}/{DeviceID}
```

Exemplo:

```text
spBv1.0/ESTG_Fabrica/DDATA/node_red_edge/WS1_PreExpansao
```

---

## 5. Tipos de mensagem usados

| Mensagem | Direção | Utilização |
|---|---|---|
| `DBIRTH` | Workstation → Node-RED | A workstation iniciou e declara métricas |
| `DDEATH` | Workstation → Node-RED | A workstation ficou indisponível |
| `DDATA` | Workstation → Node-RED | Eventos, estados, defeitos, produção, OEE |
| `DCMD` | Node-RED → Workstation | Envio de workorders e comandos |

---

## 6. Tópicos por workstation

### 6.1 Pré-expansão

Device ID:

```text
WS1_PreExpansao
```

Tópicos:

```text
spBv1.0/ESTG_Fabrica/DBIRTH/node_red_edge/WS1_PreExpansao
spBv1.0/ESTG_Fabrica/DDATA/node_red_edge/WS1_PreExpansao
spBv1.0/ESTG_Fabrica/DCMD/node_red_edge/WS1_PreExpansao
spBv1.0/ESTG_Fabrica/DDEATH/node_red_edge/WS1_PreExpansao
```

Função:

- Receber workorders de aquecimento.
- Mostrar a ordem no HMI.
- Permitir iniciar, pausar e terminar a operação.
- Enviar eventos para o Node-RED.

---

### 6.2 Armazenamento intermédio / Estabilização

Device ID:

```text
WS2_Armazenamento
```

Esta workstation corresponde ao robot LiDAR. Ou seja, o robot é tratado como a workstation de armazenamento/estabilização, em vez de ser um device separado.

Tópicos:

```text
spBv1.0/ESTG_Fabrica/DBIRTH/node_red_edge/WS2_Armazenamento
spBv1.0/ESTG_Fabrica/DDATA/node_red_edge/WS2_Armazenamento
spBv1.0/ESTG_Fabrica/DCMD/node_red_edge/WS2_Armazenamento
spBv1.0/ESTG_Fabrica/DDEATH/node_red_edge/WS2_Armazenamento
```

Função:

- Receber workorders de arrefecimento e secagem.
<!-- - Mapear o ambiente. -->
<!-- - Navegar do ponto A, fábrica, para o ponto B, armazém. -->
<!-- - Regressar ao ponto inicial. -->
 - Informar início, pausa, fim<!--, obstáculos e estado da missão. -->

---

### 6.3 Modelagem

Device ID:

```text
WS3_Modelagem
```

Tópicos:

```text
spBv1.0/ESTG_Fabrica/DBIRTH/node_red_edge/WS3_Modelagem
spBv1.0/ESTG_Fabrica/DDATA/node_red_edge/WS3_Modelagem
spBv1.0/ESTG_Fabrica/DCMD/node_red_edge/WS3_Modelagem
spBv1.0/ESTG_Fabrica/DDEATH/node_red_edge/WS3_Modelagem
```

Função:

- Receber workorders de moldagem/montagem.
- Mostrar a ordem no HMI.
- Permitir iniciar, pausar e terminar a operação.
- Enviar quantidades produzidas e rejeitadas, se aplicável.

---

### 6.4 Controlo de qualidade

Device ID:

```text
WS4_Qualidade
```

A câmara é considerada parte desta workstation. A workstation recebe a ordem, faz a verificação com a câmara e envia o resultado da classificação.

Tópicos:

```text
spBv1.0/ESTG_Fabrica/DBIRTH/node_red_edge/WS4_Qualidade
spBv1.0/ESTG_Fabrica/DDATA/node_red_edge/WS4_Qualidade
spBv1.0/ESTG_Fabrica/DCMD/node_red_edge/WS4_Qualidade
spBv1.0/ESTG_Fabrica/DDEATH/node_red_edge/WS4_Qualidade
```

Função:

- Receber workorders de verificação de defeitos.
- Avaliar o bloco com a câmara.
- Mostrar o resultado no HMI.
- Permitir confirmação pelo operador.
- Enviar resultado da inspeção e defeitos para o Node-RED.

---

## 7. Métricas comuns a todas as workstations

Estas métricas podem existir em todas as workstations.

| Métrica | Tipo | Descrição |
|---|---|---|
| `Event/Type` | String | `Start_WorkOrder`, `Pause_WorkOrder`,`Stop_WorkOrder` |
| `Event/Timestamp` | DateTime | Timestamp do evento |
| `ManufacturingOrder/Id` | String | Ordem de produção |
| `WorkOrder/Id` | String | Identificador da workorder |
| `Operation/Name`|String||
| `WorkCenter/Name`|String||
| `Product/Name` | String | `Bloco x (AZUL)` |
| `Product/Id` | Int32 | 1 |
| `KPI/Availability` | Float | Disponibilidade da workstation no momento do evento |
| `KPI/Performance` | Float | Performance da workstation no momento do evento |
| `KPI/Quality` | Float | Qualidade da workstation no momento do evento |
| `KPI/OEE` | Float | OEE da workstation no momento do evento |


---

## 8. Métricas específicas por workstation

### 8.2 Armazenamento intermédio / Estabilização

Como esta workstation é o robot LiDAR, tem métricas de produção e métricas de navegação.

| Métrica | Tipo | Descrição |
|---|---|---|
| `Robot/RightWheel/Speed` | String | |
| `Robot/RightWheel/Rotation` | String |  |
| `Robot/LeftWheel/Speed` | String |  |
| `Robot/LeftWheel/Rotation` | String |  |

---

## 10. DBIRTH das workstations

Cada workstation, ao iniciar, deve publicar um `DBIRTH` com as métricas que suporta.

Exemplo para a workstation de Pré-expansão:

Tópico:

```text
spBv1.0/ESTG_Fabrica/DBIRTH/node_red_edge/WS1_PreExpansao
```

Payload lógico:

```text
Event/Type = ""                                String
Event/Timestamp = null                         DateTime
ManufacturingOrder/Id = ""                     String
WorkOrder/Id = ""                              String
WorkOrder/PauseReason = ""                     String
WorkOrder/PauseReasonType = ""                 String
Command/Type = ""                              String
Operation/Name = ""                            String
WorkCenter/Name = ""                           String
KPI/Availability = 0.0                         Float
KPI/Performance = 0.0                          Float
KPI/Quality = 0.0                              Float
KPI/OEE = 0.0                                  Float
Product/Name = ""                              String 
Product/Id = 0                                 Int32 
```

---

## 11. DCMD — envio de workorder para uma workstation

O Node-RED envia a workorder para a workstation através de `DCMD`.

Exemplo para Pré-expansão:

Tópico:

```text
spBv1.0/ESTG_Fabrica/DCMD/node_red_edge/WS1_PreExpansao
```

Payload lógico:

```text
Command/Type = ASSIGN_WORKORDER                String
ManufacturingOrder/Id = MO001                  String
WorkOrder/Id = WO001                           String
Operation/Name = Aquecimento                   String
Command/Type = ""                              String
Operation/Name = ""                            String
WorkCenter/Name = ""                           String
KPI/Availability = 0.0                         Float
KPI/Performance = 0.0                          Float
KPI/Quality = 0.0                              Float
KPI/OEE = 0.0                                  Float
Product/Name = ""                              String 
Product/Id = 0                                 Int32 
```

---

### 11.1 DCMD — confirmação de processamento para a workstation

Depois de receber um evento por `DDATA` e executar a ação correspondente no Odoo, o Node-RED envia uma confirmação para a workstation através de `DCMD`.

Esta confirmação permite ao Arduino/HMI saber se o evento foi processado com sucesso ou se deve manter/mostrar erro ao operador.

Tópico exemplo para Controlo de qualidade:

```text
spBv1.0/ESTG_Fabrica/DCMD/node_red_edge/WS4_Qualidade
```

Payload lógico em caso de sucesso:

```text
Machine/Ack = 200                              String
Machine/AckMessage = OK                        String
```

Payload lógico em caso de erro:

```text
Machine/Ack = 400                              String
Machine/AckMessage = Erro ao executar comando no Odoo String
```

Valores previstos para `Machine/Ack`:

```text
200 = evento recebido e registado com sucesso
400 = evento recebido, mas falhou o processamento no Node-RED/Odoo
```

---

## 12. DDATA — eventos enviados pelas workstations

### 12.1 Início de workorder

Tópico:

```text
spBv1.0/ESTG_Fabrica/DDATA/node_red_edge/WS1_PreExpansao
```

Payload lógico:

```text
Event/Type = Start_WorkOrder                    String
Event/Timestamp = 2026-06-01T11:00:00Z          DateTime
ManufacturingOrder/Id = MO001                   String
WorkOrder/Id = WO001                            String
```

---

### 12.2 Pausa de workorder

Payload lógico:

```text
Event/Type = Pause_WorkOrder                    String
Event/Timestamp = 2026-06-01T11:00:00Z          DateTime
ManufacturingOrder/Id = MO001                   String
WorkOrder/Id = WO001                            String
WorkOrder/PauseReason = SETUP                   String
WorkOrder/PauseReasonType = PLANNED             String
```

Exemplos de `WorkOrder/PauseReason`:

```text
SETUP
MATERIAL_MISSING
MAINTENANCE
OPERATOR_BREAK
ROBOT_BLOCKED
QUALITY_CHECK
UNKNOWN
```

Exemplos de `WorkOrder/PauseReasonType`:

```text
PLANNED
UNPLANNED
```

---

### 12.3 Fim de workorder

Payload lógico:

```text
Event/Type = Stop_WorkOrder                     String
Event/Timestamp = 2026-06-01T11:00:00Z          DateTime
WorkOrder/Id = WO001                            String
ManufacturingOrder/Id = MO001                   String
```

---

### 12.4 Registo de defeito

Payload lógico:

```text
Event/Type = Defect_Detected                    String
Event/Timestamp = 2026-06-01T11:00:00Z          DateTime
ManufacturingOrder/Id = MO001                   String
WorkOrder/Id = WO001                            String
Product/Name = Blocox                           String
Product/Id = 1                                  String
Production/DefectCode = COLOR_ERROR             String
Production/DefectQuantity = 1                   Int32
```

Exemplos de `Quality/DefectCode`:

```text
COLOR_ERROR
SHAPE_DEFECT
DIMENSION_ERROR
BROKEN_BLOCK
SURFACE_DEFECT
CAMERA_UNCERTAIN
```

---

## 13. Workstation de armazenamento / robot LiDAR

A workstation de armazenamento é representada pelo device:

```text
WS2_Armazenamento
```


### 13.1 DDATA de paragem

```text
Event/Type = Pause_WorkOrder                   String
Event/Timestamp = 2026-06-01T11:00:00Z         DateTime
ManufacturingOrder/Id = whx                    String
WorkOrder/Id = WO002                           String
WorkOrder/PauseReason = ROBOT_BLOCKED          String
WorkOrder/PauseReasonType = UNPLANNED          String
```

### 13.2 DDATA de Informacao das rodas

```text
Event/Type = Robot_Metrics                     String
Event/Timestamp = 2026-06-01T11:00:00Z         DateTime
Robot/RightWheel/Speed = 1                         Int32
Robot/RightWheel/Rotation = 1                      Int32
Robot/LeftWheel/Speed = 1                         Int32
Robot/LeftWheel/Rotation = 1                      Int32
```




<!--

Esta workstation recebe comandos do Node-RED para workorders

 ### 13.1 DCMD para enviar missão ao robot

Tópico:

```text
spBv1.0/ESTG_Fabrica/DCMD/node_red_edge/WS2_Armazenamento
```

Payload lógico:

```text
Command/Type = MOVE_TO                         String
WorkOrder/Id = WO002                           String
Robot/MissionId = MISSION_001                  String
Robot/Origin = FABRICA                         String
Robot/Destination = ARMAZEM                    String
```

### 13.2 DDATA de estado da missão

Tópico:

```text
spBv1.0/ESTG_Fabrica/DDATA/node_red_edge/WS2_Armazenamento
```

Payload lógico:

```text
WorkOrder/Event/Type = START                    String
WorkOrder/Id = WO002                           String
Robot/MissionId = MISSION_001                  String
Robot/MissionStatus = MOVING                   String
Robot/Origin = FABRICA                         String
Robot/Destination = ARMAZEM                    String
Robot/Position/X = 2.4                         Float
Robot/Position/Y = 5.7                         Float
Robot/Position/Theta = 90.0                    Float
Robot/BatteryLevel = 78.5                      Float
Robot/ObstacleDetected = false                 Boolean
```


### 13.4 DDATA de fim de missão

```text
WorkOrder/Event/Type = STOP                     String
WorkOrder/Id = WO002                           String
Robot/MissionId = MISSION_001                  String
Robot/MissionStatus = FINISHED                 String
```

### 13.5 DDATA de mapeamento

Para simplificar, os pontos do mapa podem ser enviados como JSON numa métrica `String`.

```text
Robot/MissionStatus = MAPPING                  String
Robot/MapData = [{"x":1.2,"y":3.4},{"x":1.3,"y":3.5}] String
```

--- -->


## 15. Fluxo geral de funcionamento

1. O operador cria ou gere ordens de trabalho no Odoo/MRP.
2. O Node-RED consulta o Odoo e identifica as workorders prontas.
3. O Node-RED envia a workorder para a workstation correta usando `DCMD`.
4. A workstation mostra a ordem no HMI.
5. O operador inicia, pausa ou termina a operação.
6. A workstation envia eventos por `DDATA`.
7. O Node-RED regista os eventos no Odoo e no CrateDB.
8. O Grafana mostra estados, eventos e indicadores como OEE.
9. Quando a operação termina, o Node-RED envia a workorder para o workcenter seguinte.

Fluxo simplificado:

```text
Odoo/MRP
   ↓
Node-RED
   ↓ DCMD
Workstation
   ↓ DDATA
Node-RED
   ↓
Odoo + CrateDB + Grafana
```

---

## 16. Sequência produtiva proposta

```text
1. WS1_PreExpansao
   Operação: Aquecimento

2. WS2_Armazenamento
   Operação: Arrefecimento, secagem e transporte com robot LiDAR

3. WS3_Moldagem
   Operação: Moldar e montar

4. WS4_Qualidade
   Operação: Verificação de defeitos com câmara integrada
```

---

## 17. Resumo dos Device IDs

| Device ID | Nome real | Função |
|---|---|---|
| `WS1_PreExpansao` | Workstation de Pré-expansão | Aquecimento |
| `WS2_Armazenamento` | Workstation de Armazenamento / Robot LiDAR | Arrefecimento, secagem, transporte e mapeamento |
| `WS3_Moldagem` | Workstation de Modelagem | Moldar e montar |
| `WS4_Qualidade` | Workstation de Controlo de Qualidade | Verificação de defeitos |

---
