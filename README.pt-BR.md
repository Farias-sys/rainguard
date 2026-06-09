# RainGuard

Firmware para ESP32 que cobre automaticamente o varal quando detecta chuva e o expõe novamente quando o tempo seca. Inclui um painel IoT (Blynk) com status em tempo real, controle remoto e notificações.

[Read in English](README.md)

![Platform](https://img.shields.io/badge/plataforma-ESP32%20DOIT%20DevKit%20v1-blue)
![Framework](https://img.shields.io/badge/framework-Arduino%20%2B%20PlatformIO-orange)
![Cloud](https://img.shields.io/badge/IoT-Blynk-23C48E)

## Como funciona

O sensor de chuva é lido uma vez por segundo. O valor bruto do ADC é convertido em uma escala de intensidade de 0 a 100 com faixa de histerese (molhado ≥ 40, seco ≤ 30 por padrão), evitando oscilação perto do limiar. Uma máquina de estados decide a posição da cobertura: a chuva precisa persistir por 2 s antes de fechar (debounce) e a secura precisa persistir por 5 min antes de reabrir (atraso ajustável pelo painel). Um servo MG90S move a cobertura entre 0° (exposto) e 90° (protegido) e é desacoplado após cada movimento para eliminar ruído e consumo em repouso. A placa do sensor só recebe energia durante a leitura, o que retarda a corrosão eletrolítica.

O projeto segue a regra de **funcionamento local em primeiro lugar**: a leitura do sensor e o controle da cobertura rodam inteiramente no dispositivo, então o sistema continua protegendo as roupas sem WiFi e sem nuvem. O Blynk acrescenta visibilidade, controle remoto e notificações.

## Hardware

| Componente | Função |
|---|---|
| ESP32 DOIT DevKit v1 | controlador |
| Micro servo MG90S | movimenta a cobertura |
| Módulo sensor de chuva (tipo FC-37 / YL-83) | placa resistiva, saída analógica |

Ligações (fonte única da verdade: [`include/config.h`](include/config.h)):

| Sinal | GPIO |
|---|---|
| PWM do servo | 13 |
| AO do sensor de chuva | 34 (ADC1, somente entrada) |
| VCC do sensor (chave de energia) | 25 |
| LED de status | 2 (onboard) |
| Botão BOOT (reprovisionamento) | 0 |

Alimente o servo com 5 V (VIN/USB) com o terra em comum com o ESP32.

## Estrutura do repositório

```
platformio.ini          placa, bibliotecas, flags de compilação
include/
  config.h              todos os pinos, limiares, tempos e valores de calibração
  secrets.example.h     modelo para os IDs do Blynk (copiar para secrets.h)
lib/
  control/              máquina de estados em C++ puro: auto/manual, debounce, atraso de reabertura
  rain_logic/           matemática do sensor em C++ puro: bruto→intensidade, histerese, curva de sensibilidade
  rain_sensor/          leitura do ADC com energia chaveada
  cover/                driver do servo, attach/detach sem bloqueio
src/
  main.cpp              ponto de entrada; toda a integração Blynk (handlers, timers, envios)
  *.h                   cabeçalhos do Blynk.Edgent incorporados (provisionamento, OTA, estados do LED)
```

`lib/control` e `lib/rain_logic` não dependem do Arduino de propósito, e é isso que torna a regra de funcionamento local verificável. O ambiente `native` no `platformio.ini` existe para rodar os testes dessas bibliotecas em um PC.

## Primeiros passos

### 1. Console do Blynk

1. Crie um template em [console.blynk.cloud](https://console.blynk.cloud) (Developer Zone → Templates).
2. Adicione os datastreams V0 a V10 listados abaixo e um evento chamado `rain_detected`.
3. Em **V7** e **V8**, defina os valores padrão (3 e 5) e ative **"sync with latest server value"**, pois o dispositivo puxa esses valores ao conectar.

### 2. Secrets

```
cp include/secrets.example.h include/secrets.h
```

Preencha `BLYNK_TEMPLATE_ID` e `BLYNK_TEMPLATE_NAME` com os valores da aba Home do template. O `secrets.h` está no gitignore. **Não** defina `BLYNK_AUTH_TOKEN`: o Blynk.Edgent provisiona o token pelo ar e a compilação falha se ele estiver definido.

### 3. Compilar e gravar

Com o [PlatformIO](https://platformio.org/) (extensão do VS Code ou CLI):

```
pio run                        # compilar
pio run -t upload -t monitor   # gravar + monitor serial (115200 baud)
```

Ajuste o `upload_port` no `platformio.ini` para a sua máquina (padrão `COM6`).

### 4. Provisionar o WiFi

Na primeira inicialização o dispositivo entra em modo de configuração (LED onboard piscando) e abre um ponto de acesso WiFi. No aplicativo Blynk, use *Add device → Find devices nearby*; as credenciais de WiFi e o token de autenticação são enviados pelo ar. Segure o **BOOT por ~10 s** para apagar o provisionamento e recomeçar. Atualizações de firmware são distribuídas pelo ar via Blynk.Air (incremente `BLYNK_FIRMWARE_VERSION` no `src/main.cpp`).

## Datastreams do painel

| Pino | Nome | Valores | Descrição |
|---|---|---|---|
| V0 | intensity | 0–100 | intensidade da chuva |
| V1 | rainState | 0/1 | 1 = molhado |
| V2 | cover | 0/1 | 1 = protegido (cobertura fechada) |
| V3 | mode | 0/1 | 0 = automático, 1 = manual |
| V4 | manualCommand | 0/1 | 0 = abrir, 1 = fechar; força modo MANUAL |
| V5 | lastEvent | texto | último evento da cobertura |
| V6 | simulateRain | 0/1 | chave de demonstração, força molhado/100 |
| V7 | sensitivity | 1–5 | padrão 3, sync ativado |
| V8 | dryDelayMin | 0–30 | minutos até reabrir; padrão 5, sync ativado |
| V9 | reopenCountdownSec | segundos | contagem regressiva até a reabertura automática |
| V10 | coverControl | 0/1 | abre/fecha com um toque; força MANUAL, espelha V2 |

V7/V8 têm a nuvem como fonte da verdade (sincronizados ao conectar); os demais refletem o estado do dispositivo (enviados pelo firmware).

## Configuração e calibração

Todos os ajustes ficam em [`include/config.h`](include/config.h): pinos, limiares, tempos, geometria do servo e âncoras de calibração. Para recalibrar o sensor, defina `RAIN_CALIBRATE 1`, grave o firmware, leia pelo serial os valores brutos com a placa seca e encharcada, atualize `RAIN_RAW_DRY` / `RAIN_RAW_WET` e volte para 0 (o modo de calibração mantém a placa energizada, o que acelera a corrosão). Atenção à polaridade: o valor bruto do ADC **cai** conforme a placa molha.

A sensibilidade (V7) e o atraso de reabertura (V8) definidos pelo painel ficam gravados na NVS e sobrevivem a reinicializações e ao reprovisionamento.

---

Projeto de extensão universitária — Ciência da Computação.
