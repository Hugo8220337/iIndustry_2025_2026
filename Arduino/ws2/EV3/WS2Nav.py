#!/usr/bin/env python3
import sys
import time
import math
import serial
import logging
from ev3dev2.port import LegoPort
from ev3dev2.motor import MoveTank, OUTPUT_A, OUTPUT_B
from ev3dev2.sensor import INPUT_2
from ev3dev2.sensor.lego import ColorSensor
from ev3dev2.sound import Sound

# ==========================================
# CONFIGURAÇÃO DE LOGGING
# ==========================================
logging.basicConfig(
    level=logging.INFO, 
    format='%(asctime)s.%(msecs)03d [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S',
    handlers=[logging.StreamHandler(sys.stdout)]
)
logger = logging.getLogger(__name__)

# ==========================================
# INICIALIZAÇÃO
# ==========================================
logger.info("A inicializar hardware...")
tank = MoveTank(OUTPUT_A, OUTPUT_B)
sensor_linha = ColorSensor(INPUT_2)
sensor_linha.mode = 'COL-REFLECT'
som = Sound()

# ==========================================
# CONSTANTES DO LIDAR E HARDWARE
# ==========================================
BAUD_RATE            = 115200
WHEEL_DIAMETER_CM    = 5.6
WHEEL_BASE_CM        = 11.5
DISTANCIA_SEGURANCA  = 280   # mm — LiDAR no centro, 20cm até à frente do robô
DISTANCIA_LIVRE      = 400   # mm — considera livre quando > 40cm
MAX_PONTOS_LEITURA   = 15

# Ângulos de análise — FRENTE
ANG_FRENTE_MIN = 330   
ANG_FRENTE_MAX = 30

# Ângulos de análise — LATERAL ao contornar
# Se virou à esquerda (direcao_fuga = -1), vigia lado DIREITO: 60°-150°
# Se virou à direita  (direcao_fuga =  1), vigia lado ESQUERDO: 210°-300°
ANG_LATERAL_ESQ_MIN  = 210   # vigia esquerdo quando foi para a direita
ANG_LATERAL_ESQ_MAX  = 300
ANG_LATERAL_DIR_MIN  = 60    # vigia direito quando foi para a esquerda
ANG_LATERAL_DIR_MAX  = 150

# ==========================================
# MATEMÁTICA DA PISTA (DETECAO DE CORES)
# ==========================================
VALOR_AZUL      = 15     
VALOR_AMARELO   = 35
LIMITE_VERMELHO = 43 
TARGET          = (VALOR_AZUL + VALOR_AMARELO) / 2.0  
LIMITE_DESVIO   = 30

Kp  = 0.1  
Kd  = 0.1   
VELOCIDADE_BASE = 15  

# ==========================================
# FUNÇÕES DE TELEMETRIA
# Envia métricas para o arduino via UART (in3) no formato:
# <MET,estado,vel_esq,vel_dir,obstaculo,ws>
# ==========================================
def enviar_metrica(ser, estado, vel_esq, vel_dir, obstaculo, ws="nenhuma"):
    msg = "<MET,{},{},{},{},{}>\n".format(estado, int(vel_esq), int(vel_dir), obstaculo, ws)
    try:
        ser.write(msg.encode('ascii'))
        logger.debug("Metrica: {}".format(msg.strip()))
    except Exception:
        pass

# ==========================================
# FUNÇÃO DE LEITURA DO LIDAR (PARAMETRIZADA)
# ==========================================
def analisar_angulos(ser, ang_min, ang_max, distancia_limite):
    """
    Lê o buffer do LiDAR e conta pontos de perigo nos ângulos especificados.
    Suporta intervalos que cruzam 0° (ex: 300°-60°).
    Retorna (obstaculo_detetado, n_pontos_perigosos)
    """
    pontos_lidos    = 0
    pontos_perigo   = 0

    if ser.in_waiting == 0:
        return False, 0

    buffer_str = ser.read(ser.in_waiting).decode('ascii', errors='ignore')

    while "<" in buffer_str and ">" in buffer_str and pontos_lidos < MAX_PONTOS_LEITURA:
        inicio = buffer_str.find("<")
        fim    = buffer_str.find(">", inicio)

        if fim == -1:
            break

        mensagem = buffer_str[inicio+1:fim]
        partes   = mensagem.split(",")

        # Ignora métricas que o próprio EV3 tenha enviado e voltaram
        if len(partes) == 2:
            try:
                angulo   = int(partes[0])
                distancia = int(partes[1])

                # Filtro anti-ruído (50mm–4000mm)
                if 50 < distancia < 4000:
                    # Verifica se o ângulo está na zona de interesse
                    # Zona que cruza 0° (ex: 300–360 + 0–60)
                    if ang_min > ang_max:
                        no_intervalo = (angulo >= ang_min or angulo <= ang_max)
                    else:
                        no_intervalo = (ang_min <= angulo <= ang_max)

                    if no_intervalo:
                        pontos_lidos += 1
                        if distancia < distancia_limite:
                            pontos_perigo += 1
            except ValueError:
                pass

        buffer_str = buffer_str[fim+1:]

    obstaculo = pontos_perigo >= 3
    return obstaculo, pontos_perigo


def analisar_ambiente_frente(ser):
    """
    Analisa a zona frontal (300°–60°).
    Retorna (obstaculo_detetado, direcao_fuga).
    direcao_fuga: 1 = vira à direita, -1 = vira à esquerda.
    """
    # Divide a zona frontal em dois lados para decidir direção de fuga
    _, perigo_esq = analisar_angulos(ser, ANG_FRENTE_MIN, 359, DISTANCIA_SEGURANCA)
    _, perigo_dir = analisar_angulos(ser, 0,  ANG_FRENTE_MAX,  DISTANCIA_SEGURANCA)

    total_perigo       = perigo_esq + perigo_dir
    obstaculo_detetado = total_perigo >= 3

    # Foge do lado com MAIS pontos (para o lado com MENOS)
    if perigo_dir > perigo_esq:
        direcao_fuga = -1   # mais perigo à direita → vira à esquerda
    else:
        direcao_fuga = 1    # mais perigo à esquerda → vira à direita

    return obstaculo_detetado, direcao_fuga


def analisar_lado(ser, direcao_fuga):
    """
    Ao contornar, analisa o lado do obstáculo para saber quando ficou livre.
    Se virou à esquerda (direcao=-1), vigia o lado direito (60°-150°).
    Se virou à direita  (direcao= 1), vigia o lado esquerdo (210°-300°).
    Retorna True se ainda há obstáculo.
    """
    if direcao_fuga == -1:
        # Virou à esquerda → obstáculo está à direita do robô
        obst, _ = analisar_angulos(ser, ANG_LATERAL_DIR_MIN, ANG_LATERAL_DIR_MAX, DISTANCIA_LIVRE)
    else:
        # Virou à direita → obstáculo está à esquerda do robô
        obst, _ = analisar_angulos(ser, ANG_LATERAL_ESQ_MIN, ANG_LATERAL_ESQ_MAX, DISTANCIA_LIVRE)
    return obst


# ==========================================
# ROTAÇÃO 90 GRAUS
# ==========================================
def rodar_graus(direcao, ser, estado=2):
    """
    Roda 90° na direção indicada (1=direita, -1=esquerda).
    """
    tank.left_motor.reset()
    tank.right_motor.reset()
    target_deg = 90 * (WHEEL_BASE_CM / WHEEL_DIAMETER_CM)
    speed_l    = 20 * direcao
    speed_r    = -20 * direcao
    enviar_metrica(ser, estado, speed_l, speed_r, 1)
    tank.on_for_degrees(left_speed=speed_l, right_speed=speed_r, degrees=target_deg)
    time.sleep(0.2)


# ==========================================
# CONTORNAR OBSTÁCULO (LÓGICA REATIVA)
# ==========================================
def contornar_obstaculo(direcao_fuga, ser):
    tank.stop()
    som.beep()
    logger.warning("LOG -> Evasao reativa: Direcao {}".format("ESQUERDA" if direcao_fuga == -1 else "DIREITA"))

    rodar_graus(direcao_fuga, ser)
    
    logger.info("LOG -> Avanco de afastamento...")
    tank.on_for_degrees(15, 15, (30 / (math.pi * WHEEL_DIAMETER_CM)) * 360)
    
    rodar_graus(-direcao_fuga, ser)
   
    logger.info("LOG -> A passar o obstaculo...")
    tank.on_for_degrees(15, 15, 650)  

    timeout_contorno = time.time() + 8
    while time.time() < timeout_contorno:
        perigo, _ = analisar_ambiente_frente(ser)
        # Se não há perigo à frente, é porque já passámos o objeto
        if not perigo:
            break
        time.sleep(0.05)
    
    tank.stop()
    
    rodar_graus(-direcao_fuga, ser)
    
    # 6. ENTRAR: Anda para a frente até encontrar a linha
    logger.info("LOG -> A procurar linha azul...")
    tank.on(10, 10)
    timeout_linha = time.time() + 5
    while time.time() < timeout_linha:
        if sensor_linha.value() < LIMITE_DESVIO:
            # Encontrou o azul!
            tank.stop()
            
            logger.info("LOG -> Linha encontrada. Avancando 5cm para centragem...")
            tank.on_for_degrees(10, 10, 102) 
            
            rodar_graus(direcao_fuga, ser)
            break
        time.sleep(0.01)
    logger.info("LOG -> Evasao concluida.")


# ==========================================
# SETUP DA PORTA SÉRIE
# ==========================================
try:
    logger.info("A configurar UART (in3)...")
    p3 = LegoPort('in3')
    p3.mode = 'other-uart'
    time.sleep(2.0)
    porta_serie = serial.Serial(
        port='/dev/tty_ev3-ports:in3',
        baudrate=BAUD_RATE,
        timeout=0.01
    )
    porta_serie.reset_input_buffer()
except Exception as e:
    logger.error("Erro fatal na porta serie (in3): {}".format(e))
    sys.exit(1)

logger.info("Sistema Inicializado.")

try:
    # ==========================================
    # LOOP MASTER
    # ==========================================
    while True:
        last_error         = 0.0
        tempo_inicio_busca = 0
        direcao_busca      = 1
        ja_inverteu        = False

        porta_serie.reset_input_buffer()
        buffer_start = ""

        # ==========================================
        # STANDBY
        # ==========================================
        logger.info("\n[STANDBY] A aguardar '<CMD:START>' ou Reflexo >= 50...")
        enviar_metrica(porta_serie, 0, 0, 0, 0)

        while True:
            if porta_serie.in_waiting > 0:
                chunk = porta_serie.read(porta_serie.in_waiting).decode('ascii', errors='ignore')
                buffer_start += chunk
                if len(buffer_start) > 50:
                    buffer_start = buffer_start[-50:]
                if "<CMD:START>" in buffer_start:
                    som.beep()
                    logger.info("START via Serial!")
                    tank.on_for_seconds(15, 15, 0.5)
                    porta_serie.reset_input_buffer()
                    break

            if sensor_linha.value() >= 50:
                som.beep()
                logger.info("START físico!")
                tank.on_for_seconds(15, 15, 0.5)
                break

            time.sleep(0.1)

        logger.info("PID Iniciado. Target: {} | Corte > {}".format(TARGET, LIMITE_DESVIO))

        # ==========================================
        # MALHA PRINCIPAL
        # ==========================================
        while True:

            # ------------------------------------------
            # LIDAR: verifica frente — PARA imediatamente
            # ------------------------------------------
            perigo, direcao = analisar_ambiente_frente(porta_serie)
            if perigo:
                tank.stop()   # PARA antes de contornar
                contornar_obstaculo(direcao, porta_serie)
                last_error         = 0.0
                tempo_inicio_busca = 0
                continue

            leitura_atual = sensor_linha.value()

            # ------------------------------------------
            # VERMELHO: WS2 ou regresso
            # ------------------------------------------
            if leitura_atual >= LIMITE_VERMELHO:

                if ja_inverteu and leitura_atual >= 45:
                    tank.stop()
                    logger.info("META COMPLETA!")
                    enviar_metrica(porta_serie, 7, 0, 0, 0, 1)
                    som.beep()
                    time.sleep(0.2)
                    som.beep()

                    tank.on_for_seconds(30, -30, 1.2)
                    while True:
                        if sensor_linha.value() >= 50:
                            tank.stop()
                            break
                        tank.on(-15, -15)
                        time.sleep(0.01)

                    tank.on_for_seconds(15, 15, 0.4)
                    tank.stop()
                    som.beep()
                    logger.info("Estacionado. Pronto para proximo START.")
                    break

                elif not ja_inverteu:
                    tank.stop()
                    logger.info("WS2_Armazenamento atingido!")
                    enviar_metrica(porta_serie, 6, 0, 0, 0, 2)
                    som.beep()

                    time.sleep(2.0)
                    tank.on_for_seconds(30, -30, 1.2)
                    tank.on_for_seconds(15, 15, 0.5)
                    ja_inverteu        = True
                    tempo_inicio_busca = 0
                    continue

            # ------------------------------------------
            # BUSCA ATIVA
            # ------------------------------------------
            if leitura_atual > LIMITE_DESVIO:
                if tempo_inicio_busca == 0:
                    tempo_inicio_busca = time.time()
                    direcao_busca      = 1 if last_error > 0 else -1
                    logger.info("Linha perdida! Varrimento...")
                    enviar_metrica(porta_serie, 4, 0, 0, 0)

                tempo_decorrido = time.time() - tempo_inicio_busca
                POTENCIA_BUSCA  = 25

                if tempo_decorrido < 0.8:
                    tank.on(-POTENCIA_BUSCA * direcao_busca, POTENCIA_BUSCA * direcao_busca)
                elif tempo_decorrido < 2.5:
                    tank.on(POTENCIA_BUSCA * direcao_busca, -POTENCIA_BUSCA * direcao_busca)
                else:
                    tank.on(20, 20)
                    if tempo_decorrido > 3.0:
                        tempo_inicio_busca = 0

                time.sleep(0.01)
                continue

            # ------------------------------------------
            # PID — seguir linha
            # ------------------------------------------
            tempo_inicio_busca = 0

            error      = TARGET - leitura_atual
            derivative = error - last_error
            Kp_real    = 0.4
            correcao   = (Kp_real * error) + (Kd * derivative)
            TRIM_ESQUERDA = 3.0

            motor_esq = (VELOCIDADE_BASE + TRIM_ESQUERDA) - correcao
            motor_dir = VELOCIDADE_BASE + correcao
            motor_esq = max(-100, min(100, motor_esq))
            motor_dir = max(-100, min(100, motor_dir))

            tank.on(motor_esq, motor_dir)

            if int(time.time() * 100) % 10 == 0:
                enviar_metrica(porta_serie, 1, motor_esq, motor_dir, 0)

            last_error = error
            time.sleep(0.01)

except KeyboardInterrupt:
    tank.stop()
    logger.warning("Programa terminado pelo utilizador.")
except Exception as e:
    tank.stop()
    logger.error("Erro inesperado: {}".format(e))