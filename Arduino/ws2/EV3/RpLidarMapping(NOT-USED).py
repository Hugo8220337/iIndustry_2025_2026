#!/usr/bin/env python3

# ========================================================================================
# Este script nao está a ser utilizado, mas serve como exemplo de como ler o LiDAR e tomar decisões de
# navegação baseadas na concentração de obstáculos. O código é mais complexo do que o necessário
# para a tarefa, mas foi escrito para ser robusto e adaptável a diferentes cenários.
#
# ========================================================================================
#
import sys
import time
import math
import serial
from ev3dev2.port import LegoPort
from ev3dev2.motor import MoveTank, OUTPUT_B, OUTPUT_C

# ==========================================
# CONFIGURACOES GERAIS
# ==========================================
BAUD_RATE = 115200 
WHEEL_DIAMETER_CM = 5.6
WHEEL_BASE_CM = 11.5

DISTANCIA_SEGURANCA_MM = 300  
MAX_PONTOS_LEITURA = 15       

tank = MoveTank(OUTPUT_B, OUTPUT_C)

# ==========================================
# FUNCAO DE LEITURA E DECISAO ESPACIAL
# ==========================================
def analisar_ambiente_frente(ser):
    """
    Lê o LiDAR (300º a 60º) e analisa de que lado está a maior concentração
    de obstáculos. Devolve (True/False, direcao_fuga).
    direcao_fuga: 1 (Direita) ou -1 (Esquerda)
    """
    ser.reset_input_buffer()
    time.sleep(0.10) 
    
    buffer_str = ""
    pontos_lidos = 0
    pontos_perigo_esq = 0
    pontos_perigo_dir = 0

    if ser.in_waiting > 0:
        buffer_str = ser.read(ser.in_waiting).decode('ascii', errors='ignore')
            
    while "<" in buffer_str and ">" in buffer_str and pontos_lidos < MAX_PONTOS_LEITURA:
        inicio = buffer_str.find("<")
        fim = buffer_str.find(">", inicio)
        
        if fim != -1:
            mensagem = buffer_str[inicio+1:fim]
            partes = mensagem.split(",")
            
            if len(partes) == 2:
                try:
                    angulo = int(partes[0])
                    distancia = int(partes[1])
                    
                    # Filtro anti-ruído (ignora lixo < 5cm e fantasmas > 4m)
                    if 50 < distancia < 4000: 
                        
                        if angulo >= 300 or angulo <= 60:
                            pontos_lidos += 1
                            
                            if distancia < DISTANCIA_SEGURANCA_MM:
                                # Regista de que lado apareceu o ponto
                                if 300 <= angulo <= 359:
                                    pontos_perigo_esq += 1
                                else:
                                    pontos_perigo_dir += 1
                                    
                except ValueError:
                    pass
                    
            buffer_str = buffer_str[fim+1:]
        else:
            break

    # Avaliação do Consenso
    total_perigo = pontos_perigo_esq + pontos_perigo_dir
    obstaculo_detetado = total_perigo >= 3
    
    # Determina a direção de fuga: foge do lado com MAIS pontos
    direcao_fuga = 1  # Por defeito vira à direita (caso seja uma parede plana perfeita)
    if pontos_perigo_dir > pontos_perigo_esq:
        direcao_fuga = -1  # Obstáculo mais denso à direita, foge para a esquerda

    return obstaculo_detetado, direcao_fuga

# ==========================================
# MOVIMENTO DIRECIONADO
# ==========================================
def rodar_90_graus_direcionado(direcao):
    """
    Roda 90 graus para a direção recebida (1 = Direita, -1 = Esquerda)
    """
    tank.left_motor.reset()
    tank.right_motor.reset()
    
    target_wheel_degrees = 90 * (WHEEL_BASE_CM / WHEEL_DIAMETER_CM)
    
    if direcao == 1:
        print("Obstaculo detectado à ESQUERDA/FRENTE -> A rodar para a DIREITA.")
    else:
        print("Obstaculo detectado à DIREITA/FRENTE -> A rodar para a ESQUERDA.")
        
    speed_left = 20 * direcao
    speed_right = -20 * direcao
    
    tank.on_for_degrees(left_speed=speed_left, right_speed=speed_right, degrees=target_wheel_degrees)
    time.sleep(0.2)

# ==========================================
# CICLO PRINCIPAL DE NAVEGACAO
# ==========================================
def exploracao_inteligente(ser):
    print("Sistema de Exploracao (Fuga Inteligente Direcionada) Iniciado.")
    time.sleep(1)
    
    try:
        while True:
            perigo_iminente, direcao_fuga = analisar_ambiente_frente(ser)
            
            if perigo_iminente:
                tank.stop()
                rodar_90_graus_direcionado(direcao_fuga)
                
                # Esvazia o buffer após rodar para ganhar visão fresca
                ser.reset_input_buffer() 
            else:
                tank.on(25, 25)

    except KeyboardInterrupt:
        tank.stop()
        print("\nParado pelo utilizador.")
    except Exception as e:
        tank.stop()
        print("\nErro detetado: {}".format(e))
        time.sleep(5)

if __name__ == '__main__':
    try:
        p1 = LegoPort('in1')
        p1.mode = 'other-uart'
        time.sleep(1.0)
        
        porta_serie = serial.Serial(port='/dev/ttyS1', baudrate=BAUD_RATE, timeout=0.1)
        porta_serie.reset_input_buffer()  # Limpeza inicial garantida
        
    except Exception as e:
        print("Erro critico na porta S1: {}".format(e))
        sys.exit(1)
        
    exploracao_inteligente(porta_serie)