# Dashboards Grafana - Projeto IIoT EPS

## 1. Objetivo

Este documento define uma proposta de dashboards Grafana para visualizar a informacao recolhida das workstations atraves de Sparkplug B, Node-RED e CrateDB.

---

## 2. Workstations consideradas

| Workstation | Device ID | Operacao |
|---|---|---|
| Pre-expansao | `WS1_PreExpansao` | Aquecimento |
| Armazenamento / Robot LiDAR | `WS2_Armazenamento` | Arrefecimento, secagem e transporte |
| Moldagem | `WS3_Moldagem` | Moldar e montar |
| Controlo de qualidade | `WS4_Qualidade` | Verificacao de defeitos |

---

## 3. Metricas principais

### 3.1 Estados e eventos

| Metrica | Tipo | Uso no Grafana |
|---|---|---|
| `Event/Type` | String | Tipo de evento produtivo |
| `Event/Timestamp` | DateTime | Momento do evento |
| `WorkOrder/Id` | String | Identificador da workorder |
| `ManufacturingOrder/Id` | String | Identificador da manufacturing order |
| `Operation/Name` | String | Nome da operacao |
| `WorkCenter/Name` | String | Nome do workcenter |
| `Product/Name` | String | Nome do produto |

### 3.2 Pausas

| Metrica | Tipo | Uso no Grafana |
|---|---|---|
| `WorkOrder/PauseReason` | String | Motivo da pausa |
| `WorkOrder/PauseReasonType` | String | Pausa planeada ou nao planeada |

### 3.3 OEE

| Metrica | Tipo | Uso no Grafana |
|---|---|---|
| `KPI/Availability` | Float | Disponibilidade da workstation |
| `KPI/Performance` | Float | Performance da workstation |
| `KPI/Quality` | Float | Qualidade da workstation |
| `KPI/OEE` | Float | OEE agregado |


### 3.4 Robot

| Metrica | Tipo | Uso no Grafana |
|---|---|---|
| `Robot/RightWheel/Speed` | Int32 ou Float | Velocidade da roda direita |
| `Robot/RightWheel/Rotation` | Int32 ou Float | Rotacao da roda direita |
| `Robot/LeftWheel/Speed` | Int32 ou Float | Velocidade da roda esquerda |
| `Robot/LeftWheel/Rotation` | Int32 ou Float | Rotacao da roda esquerda |
| `Robot/ObstacleDetected` | Boolean | Detecao de obstaculo |

---

## 4. Dashboard 1 - Visao Geral Da Linha

Dashboard principal para operadores e demonstracao do sistema.

### 4.1 Estado atual por workstation

| Campo | Valor |
|---|---|
| Tipo de painel | `State timeline` ou `Status history` |
| Workstations | `WS1_PreExpansao`, `WS2_Armazenamento`, `WS3_Moldagem`, `WS4_Qualidade` |
| Metricas | `Event/Type` |
| Objetivo | Mostrar se cada workstation esta pronta, em execucao, pausada ou parada |

### 4.2 OEE global da linha

| Campo | Valor |
|---|---|
| Tipo de painel | `Stat` ou `Gauge` |
| Metrica | Media de `KPI/OEE` |
| Objetivo | Mostrar a eficiencia global da linha |

### 4.3 OEE por workstation

| Campo | Valor |
|---|---|
| Tipo de painel | `Gauge` |
| Metricas | `KPI/OEE`, `KPI/Availability`, `KPI/Performance`, `KPI/Quality` |
| Objetivo | Comparar desempenho entre workstations |

Recomendacao:

- Usar um gauge por workstation para `KPI/OEE`.
- Usar uma tabela ou barras pequenas para decompor `Availability`, `Performance` e `Quality`.

### 4.4 Workorder atual por workstation

| Campo | Valor |
|---|---|
| Tipo de painel | `Table` |
| Campos | `DeviceId`, `WorkOrder/Id`, `ManufacturingOrder/Id`, `Operation/Name`, `WorkCenter/Name`, `Product/Name` |
| Objetivo | Ver que ordem esta atribuida a cada workstation |

### 4.5 Ultimos eventos

| Campo | Valor |
|---|---|
| Tipo de painel | `Table` ou `Logs` |
| Campos | `Event/Timestamp`, `DeviceId`, `Event/Type`, `WorkOrder/Id`, `WorkOrder/PauseReason`,`WorkCenter/Name` |
| Objetivo | Mostrar a sequencia recente de eventos enviados pelas workstations |

---

## 5. Dashboard 2 - Producao E Workorders

Dashboard focado no fluxo de producao.

### 5.1 Timeline de workorders

| Campo | Valor |
|---|---|
| Tipo de painel | `State timeline` |
| Metricas | `Event/Type`,`Event/Timestamp`, `WorkOrder/Id`, `ManufacturingOrder/Id` |
| Objetivo | Mostrar quando cada workorder iniciou, pausou e terminou |

### 5.2 Workorders por estado

| Campo | Valor |
|---|---|
| Tipo de painel | `Bar chart` ou `Pie chart` |
| Fonte | Odoo ou eventos processados em CrateDB |
| Estados | `ready`, `progress`, `paused`, `done`, `cancel` |
| Objetivo | Perceber a distribuicao atual das ordens |

### 5.3 Tempo por operacao

| Campo | Valor |
|---|---|
| Tipo de painel | `Bar chart` |
| Calculo | Diferenca entre `Start_WorkOrder` e `Stop_WorkOrder` |
| Agrupamento | `Operation/Name` ou `WorkCenter/Name` |
| Objetivo | Comparar duracao das operacoes |

### 5.4 Tempo por workstation

| Campo | Valor |
|---|---|
| Tipo de painel | `Time series` ou `Bar chart` |
| Calculo | Tempo acumulado em `Start`, `Pause`, `Stop` |
| Agrupamento | `DeviceId` |
| Objetivo | Ver utilizacao das workstations |

### 5.5 Pausas por motivo

| Campo | Valor |
|---|---|
| Tipo de painel | `Bar chart` |
| Metrica | `WorkOrder/PauseReason` |
| Objetivo | Identificar as causas principais de paragem |

### 5.6 Pausas planeadas vs nao planeadas

| Campo | Valor |
|---|---|
| Tipo de painel | `Pie chart` ou `Bar chart` |
| Metrica | `WorkOrder/PauseReasonType` |
| Valores | `PLANNED`, `UNPLANNED` |
| Objetivo | Separar paragens esperadas de problemas operacionais |

---

## 6. Dashboard 3 - OEE E Performance

Dashboard focado nos indicadores de eficiencia.

### 6.1 OEE ao longo do tempo

| Campo | Valor |
|---|---|
| Tipo de painel | `Time series` |
| Metrica | `KPI/OEE` |
| Agrupamento | `Workcenter/Name` |
| Objetivo | Ver a evolucao do OEE por workstation |

### 6.2 Availability, Performance e Quality

| Campo | Valor |
|---|---|
| Tipo de painel | `Time series` |
| Metricas | `KPI/Availability`, `KPI/Performance`, `KPI/Quality` |
| Agrupamento | `Workcenter/Name` |
| Objetivo | Identificar qual componente esta a afetar o OEE |

### 6.3 Ranking de workstations por OEE

| Campo | Valor |
|---|---|
| Tipo de painel | `Bar chart` |
| Metrica | Media de `KPI/OEE` |
| Agrupamento | `Workcenter/Name` |
| Objetivo | Comparar o desempenho das workstations |

### 6.4 Alertas de OEE baixo

| Campo | Valor |
|---|---|
| Tipo de painel | `Stat` com thresholds |
| Metrica | `KPI/OEE` |
| Regra sugerida | Alerta se `KPI/OEE < 0.60` |
| Objetivo | Sinalizar queda de eficiencia |

---

## 7. Dashboard 4 - Qualidade

Dashboard especifico para `WS4_Qualidade`.

### 7.1 Defeitos por tipo

| Campo | Valor |
|---|---|
| Tipo de painel | `Bar chart` |
| Metrica | `Production/DefectCode` |
| Objetivo | Identificar os defeitos mais frequentes |


---

## 8. Dashboard 5 - Robot / Armazenamento

Dashboard especifico para `WS2_Armazenamento`.

### 8.1 Velocidade das rodas

| Campo | Valor |
|---|---|
| Tipo de painel | `Time series` |
| Metricas | `Robot/RightWheel/Speed`, `Robot/LeftWheel/Speed` |
| Objetivo | Comparar velocidades das duas rodas |

### 8.2 Rotacao das rodas

| Campo | Valor |
|---|---|
| Tipo de painel | `Time series` |
| Metricas | `Robot/RightWheel/Rotation`, `Robot/LeftWheel/Rotation` |
| Objetivo | Monitorizar movimento do robot |

### 8.3 Bloqueios do robot

| Campo | Valor |
|---|---|
| Tipo de painel | `Stat` + `Bar chart` |
| Filtro | `WorkOrder/PauseReason = ROBOT_BLOCKED` |
| Objetivo | Contar bloqueios e paragens por obstaculos |

---
