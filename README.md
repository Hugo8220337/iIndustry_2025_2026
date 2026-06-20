# Arquitetura de Integração Vertical em Ambientes IIoT: Otimização da Produção de EPS


## Resumo do Projeto

Este repositório documenta a implementação de uma arquitetura ciber-física para a digitalização de processos produtivos, com foco na produção de blocos de Poliestireno Expandido (EPS). O sistema estabelece uma integração vertical completa, conectando o ERP Odoo (gestão) ao chão de fábrica (dispositivos IIoT), utilizando um middleware baseado em Node-RED.

A solução garante a monitorização em tempo real de indicadores de performance (OEE) e a rastreabilidade da qualidade através de visão artificial (Nicla Vision) e sensores, persistindo os dados em CrateDB e visualizando-os via Grafana. O projeto valida a interoperabilidade industrial recorrendo a normas como AAS e protocolos IIoT (MQTT/Sparkplug B).


## Trabalho Individual

### Diogo Pereira

- Configuração dos Arduinos.
- Flow de comunicação MQTT entre Arduino e Odoo.
- Flow de comunicação OPC UA entre Node red e AAS.
- Configuração da DataBridge entre Node red e AAS.


### Hugo Guimarães

- Configuração dos Arduinos.
- Configuração de produtos, variantes e bills of materials no Odoo.
- Flow de comunicação MQTT entre Arduino e CrateDB.


## Services Overview

**Server IP:** `158.179.220.228`

---

| Service | URL |
|---|---|
| Node-RED | [http://158.179.220.228:1880](http://158.179.220.228:1880) |
| Odoo | [http://158.179.220.228:8069](http://158.179.220.228:8069)  |
| CrateDB | [http://158.179.220.228:4200](http://158.179.220.228:4200) <br> [http://158.179.220.228:5434](http://158.179.220.228:5434) |
| AAS Environment | [http://158.179.220.228:8081](http://158.179.220.228:8081) |
| AAS Registry | [http://158.179.220.228:8082](http://158.179.220.228:8082) |
| Submodel Registry | [http://158.179.220.228:8083](http://158.179.220.228:8083) |
| AAS Discovery | [http://158.179.220.228:8084](http://158.179.220.228:8084) |
| AAS Web UI | [http://158.179.220.228:3000](http://158.179.220.228:3000) |
| AAS Dashboard API | [http://158.179.220.228:8085](http://158.179.220.228:8085) |
| Grafana| [http://158.179.220.228:3001](http://158.179.220.228:3001) |
| Mosquitto (MQTT) | `158.179.220.228:1883` |

---

## Odoo Credentials

| | |
|---|---|
| Database | `odoodb` |
| Email | `user@odoo.com` |
| Password | `industry2026` |

## Grafana Credentials

| | |
|---|---|
| User | `admin` |
| Password | `admin` |