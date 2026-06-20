# Asset Administration Shells (AAS) - Workstations

## 1. Objetivo

Este documento define a proposta de modelacao em Asset Administration Shell (AAS) para as quatro workstations do processo produtivo.

A abordagem proposta e criar uma AAS por workstation, reutilizando submodelos comuns para identificacao, estado operacional, workorder, eventos de producao, OEE e comunicacao Sparkplug B.

Submodelos especificos sao adicionados apenas onde fazem sentido:

- `Robot` apenas na workstation de armazenamento, porque corresponde ao robot LiDAR.

---

## 2. Assets AAS

| AAS ID | Asset representado | Device ID Sparkplug B | Funcao |
|---|---|---|---|
| `AAS_WS1_PreExpansao` | Workstation de pre-expansao | `WS1_PreExpansao` | Aquecimento |
| `AAS_WS2_Armazenamento` | Workstation de armazenamento / robot LiDAR | `WS2_Armazenamento` | Arrefecimento, secagem e transporte |
| `AAS_WS3_Moldagem` | Workstation de moldagem | `WS3_Moldagem` | Moldar e montar |
| `AAS_WS4_Qualidade` | Workstation de controlo de qualidade | `WS4_Qualidade` | Verificacao de defeitos |

---

## 3. Estrutura geral

Todas as workstations seguem a mesma estrutura base:

```text
AAS_WSx
|-- Identification
|-- WorkOrder
|-- KPIS
`-- Communication
```

A workstation de armazenamento inclui ainda:

```text
AAS_WS2_Armazenamento
`-- Robot
```

---

## 4. Submodelos comuns

Os submodelos seguintes existem em todas as AAS das workstations.

### 4.1 Identification

Identifica a workstation, a operacao associada e a sua localizacao logica/fisica.

```text
Identification
|-- WorkstationId
|-- WorkCenterName
|-- OperationName
|-- DeviceId
`-- Location
```

| Elemento | Tipo sugerido | Descricao |
|---|---|---|
| `WorkstationId` | String | Identificador interno da workstation |
| `WorkCenterName` | String | Nome do workcenter no Odoo/MRP |
| `OperationName` | String | Nome da operacao produtiva |
| `DeviceId` | String | Device ID usado em Sparkplug B |
| `Location` | String | Localizacao fisica ou logica da workstation |


### 4.2 WorkOrder

Representa a ordem de trabalho atualmente atribuida ou em execucao na workstation.

```text
WorkOrder
|-- ManufacturingOrderId
|-- WorkOrderId
|-- Status
|-- ProductName
|-- OperationName
|-- PauseReason
|-- PauseReasonType
|-- LastEventTimestamp
```

| Elemento | Tipo sugerido | Descricao |
|---|---|---|
| `ManufacturingOrderId` | String | Identificador da ordem de producao |
| `WorkOrderId` | String | Identificador da ordem de trabalho |
| `Status` | String | Estado da workorder, por exemplo `ASSIGNED`, `IN_PROGRESS`, `PAUSED`, `DONE` ou `CANCELLED` |
| `ProductName` | String | Nome do produto |
| `OperationName` | String | Operacao executada pela workorder |
| `PauseReason` | String | Motivo da pausa, se aplicavel |
| `PauseReasonType` | String | Tipo de pausa, por exemplo `PLANNED` ou `UNPLANNED` |
| `LastEventTimestamp` | DateTime | Timestamp do evento |


### 4.3 KPIS

Representa os indicadores de eficiencia da workstation.

```text
KPIS
|-- Availability
|-- Performance
|-- Quality
|-- OEE
```

| Elemento | Tipo sugerido | Descricao |
|---|---|---|
| `Availability` | Float | Disponibilidade da workstation |
| `Performance` | Float | Performance da workstation |
| `Quality` | Float | Qualidade da workstation |
| `OEE` | Float | Overall Equipment Effectiveness calculado |

### 4.4 Communication

Mapeia a AAS para a camada Sparkplug B/MQTT.

```text
Communication
|-- GroupId
|-- EdgeNodeId
|-- DeviceId
|-- DBIRTHTopic
|-- DDATATopic
|-- DCMDTopic
|-- DDEATHTopic
```

| Elemento | Tipo sugerido | Descricao |
|---|---|---|
| `GroupId` | String | Group ID Sparkplug B |
| `EdgeNodeId` | String | Edge Node ID Sparkplug B |
| `DeviceId` | String | Device ID da workstation |
| `DBIRTHTopic` | String | Topico de nascimento do device |
| `DDATATopic` | String | Topico de dados/eventos |
| `DCMDTopic` | String | Topico de comandos para a workstation |
| `DDEATHTopic` | String | Topico de morte/desligamento do device |

---

## 5. AAS_WS1_PreExpansao

```text
AAS_WS1_PreExpansao
|-- Identification
|   |-- WorkstationId = WS1
|   |-- WorkCenterName = Pre-expansao
|   |-- OperationName = Aquecimento
|   |-- DeviceId = WS1_PreExpansao
|   |-- Location = Linha EPS / Pre-expansao
|-- WorkOrder
|-- KPIS
|-- Communication
    |-- GroupId = ESTG_Fabrica
    |-- EdgeNodeId = node_red_edge
    |-- DeviceId = WS1_PreExpansao
    |-- DBIRTHTopic = spBv1.0/ESTG_Fabrica/DBIRTH/node_red_edge/WS1_PreExpansao
    |-- DDATATopic = spBv1.0/ESTG_Fabrica/DDATA/node_red_edge/WS1_PreExpansao
    |-- DCMDTopic = spBv1.0/ESTG_Fabrica/DCMD/node_red_edge/WS1_PreExpansao
    |-- DDEATHTopic = spBv1.0/ESTG_Fabrica/DDEATH/node_red_edge/WS1_PreExpansao
```

---

## 6. AAS_WS2_Armazenamento

```text
AAS_WS2_Armazenamento
|-- Identification
|   |-- WorkstationId = WS2
|   |-- WorkCenterName = Armazenamento intermedio / Estabilizacao
|   |-- OperationName = Arrefecimento, secagem e transporte
|   |-- DeviceId = WS2_Armazenamento
|   |-- Location = Linha EPS / Armazenamento
|-- WorkOrder
|-- KPIS
|-- Communication
|   |-- GroupId = ESTG_Fabrica
|   |-- EdgeNodeId = node_red_edge
|   |-- DeviceId = WS2_Armazenamento
|   |-- DBIRTHTopic = spBv1.0/ESTG_Fabrica/DBIRTH/node_red_edge/WS2_Armazenamento
|   |-- DDATATopic = spBv1.0/ESTG_Fabrica/DDATA/node_red_edge/WS2_Armazenamento
|   |-- DCMDTopic = spBv1.0/ESTG_Fabrica/DCMD/node_red_edge/WS2_Armazenamento
|   |-- DDEATHTopic = spBv1.0/ESTG_Fabrica/DDEATH/node_red_edge/WS2_Armazenamento
|-- Robot
    |-- ObstacleDetected
    |-- RightWheel
    |   |-- Speed
    |   |-- Rotation
    |   |-- Duration
    |-- LeftWheel
        |-- Speed
        |-- Rotation
        |-- Duration
```

### 6.1 Robot

VAI SER PARA MANTER DURATION??

Submodelo especifico do robot LiDAR.

| Elemento | Tipo sugerido | Metrica Sparkplug B sugerida | Descricao |
|---|---|---|---|
| `ObstacleDetected` | Boolean | `Robot/ObstacleDetected` | Indica se foi detetado obstaculo |
| `RightWheel.Speed` | Int32 ou Float | `Robot/RightWheel/Speed` | Velocidade da roda direita |
| `RightWheel.Rotation` | Int32 ou Float | `Robot/RightWheel/Rotation` | Rotacao da roda direita |
| `RightWheel.Duration` | Int32 ou Float | `Robot/RightWheel/Duration` | Duracao/tempo de movimento da roda direita |
| `LeftWheel.Speed` | Int32 ou Float | `Robot/LeftWheel/Speed` | Velocidade da roda esquerda |
| `LeftWheel.Rotation` | Int32 ou Float | `Robot/LeftWheel/Rotation` | Rotacao da roda esquerda |
| `LeftWheel.Duration` | Int32 ou Float | `Robot/LeftWheel/Duration` | Duracao/tempo de movimento da roda esquerda |


---

## 7. AAS_WS3_Moldagem

```text
AAS_WS3_Moldagem
|-- Identification
|   |-- WorkstationId = WS3
|   |-- WorkCenterName = Modelagem
|   |-- OperationName = Moldar e montar
|   |-- DeviceId = WS3_Moldagem
|   `-- Location = Linha EPS / Modelagem
|-- WorkOrder
|-- KPIS
`-- Communication
    |-- GroupId = ESTG_Fabrica
    |-- EdgeNodeId = node_red_edge
    |-- DeviceId = WS3_Moldagem
    |-- DBIRTHTopic = spBv1.0/ESTG_Fabrica/DBIRTH/node_red_edge/WS3_Moldagem
    |-- DDATATopic = spBv1.0/ESTG_Fabrica/DDATA/node_red_edge/WS3_Moldagem
    |-- DCMDTopic = spBv1.0/ESTG_Fabrica/DCMD/node_red_edge/WS3_Moldagem
    `-- DDEATHTopic = spBv1.0/ESTG_Fabrica/DDEATH/node_red_edge/WS3_Moldagem
```

---

## 8. AAS_WS4_Qualidade

```text
AAS_WS4_Qualidade
|-- Identification
|   |-- WorkstationId = WS4
|   |-- WorkCenterName = Controlo de qualidade
|   |-- OperationName = Verificacao de defeitos
|   |-- DeviceId = WS4_Qualidade
|   |-- Location = Linha EPS / Qualidade
|   `-- DefectCode
|        
|-- WorkOrder
|-- KPIS
|-- Communication
|   |-- GroupId = ESTG_Fabrica
|   |-- EdgeNodeId = node_red_edge
|   |-- DeviceId = WS4_Qualidade
|   |-- DBIRTHTopic = spBv1.0/ESTG_Fabrica/DBIRTH/node_red_edge/WS4_Qualidade
|   |-- DDATATopic = spBv1.0/ESTG_Fabrica/DDATA/node_red_edge/WS4_Qualidade
|   |-- DCMDTopic = spBv1.0/ESTG_Fabrica/DCMD/node_red_edge/WS4_Qualidade
|   `-- DDEATHTopic = spBv1.0/ESTG_Fabrica/DDEATH/node_red_edge/WS4_Qualidade
```

## 10. Resumo

A modelacao proposta fica assim:

```text
AAS_WS1_PreExpansao
|-- Identification
|-- WorkOrder
|-- KPIS
`-- Communication

AAS_WS2_Armazenamento
|-- Identification
|-- WorkOrder
|-- KPIS
|-- Communication
`-- RobotTelemetry

AAS_WS3_Moldagem
|-- Identification
|-- WorkOrder
|-- KPIS
`-- Communication

AAS_WS4_Qualidade
|-- Identification
|-- WorkOrder
|-- KPIS
`-- Communication

```

